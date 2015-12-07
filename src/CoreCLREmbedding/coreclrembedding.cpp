#include "edge.h"
#include <limits.h>
#include <set>
#include <sys/stat.h>

#ifndef EDGE_PLATFORM_WINDOWS
#include <dlfcn.h>
#include <unistd.h>
#include <libgen.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <fstream>
#include <sstream>
#else
#include <direct.h>
#include <shlwapi.h>
#include <intsafe.h>
#endif

#if EDGE_PLATFORM_APPLE
#include <libproc.h>
#endif

#ifdef EDGE_PLATFORM_APPLE
const char* LIBCORECLR_NAME = "libcoreclr.dylib";
#elif defined(EDGE_PLATFORM_WINDOWS)
const char* LIBCORECLR_NAME = "coreclr.dll";
#else
const char* LIBCORECLR_NAME = "libcoreclr.so";
#endif

typedef HRESULT (STDMETHODCALLTYPE *coreclr_create_delegateFunction)(
		void* hostHandle,
		unsigned int domainId,
		const char* assemblyName,
		const char* typeName,
		const char* methodName,
		void** delegate);
typedef HRESULT (STDMETHODCALLTYPE *coreclr_initializeFunction)(
		const char *exePath,
		const char *appDomainFriendlyName,
		int propertyCount,
		const char** propertyKeys,
		const char** propertyValues,
		void** hostHandle,
		unsigned int* domainId);

unsigned int appDomainId;
void* hostHandle;
GetFuncFunction getFunc;
CallFuncFunction callFunc;
ContinueTaskFunction continueTask;
FreeHandleFunction freeHandle;
FreeMarshalDataFunction freeMarshalData;
CompileFuncFunction compileFunc;
InitializeFunction initialize;

#define CREATE_DELEGATE(functionName, functionPointer)\
	result = createDelegate(\
			hostHandle,\
			appDomainId,\
			"CoreCLREmbedding",\
			"CoreCLREmbedding",\
			functionName,\
			(void**) functionPointer);\
	\
	if (FAILED(result))\
	{\
		throwV8Exception("Call to coreclr_create_delegate() for %s failed with a return code of 0x%x.", functionName, result);\
		return result;\
	}\
	\
	DBG("CoreClrEmbedding::Initialize - CoreCLREmbedding.%s() loaded successfully", functionName);\

std::string GetOSName()
{
#if EDGE_PLATFORM_WINDOWS
	return "Windows";
#else
	utsname unameData;

	if (uname(&unameData) == 0)
	{
		return unameData.sysname;
	}

	// uname() failed, falling back to defaults
	else
	{
#if EDGE_PLATFORM_NIX
		return "Linux";
#elif EDGE_PLATFORM_APPLE
		return "Darwin";
#endif
	}
#endif
}

const char* GetOSArchitecture()
{
#if defined __X86__ || defined __i386__ || defined i386 || defined _M_IX86 || defined __386__
	return "x86";
#elif defined __ia64 || defined _M_IA64 || defined __ia64__ || defined __x86_64__ || defined _M_X64
	return "x64";
#elif defined ARM || defined __arm__ || defined _ARM
	return "arm";
#endif
}

std::string GetOSVersion()
{
#if EDGE_PLATFORM_WINDOWS
	OSVERSIONINFO version_info;
	ZeroMemory(&version_info, sizeof(OSVERSIONINFO));
	version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

#pragma warning(disable:4996)
	GetVersionEx(&version_info);
#pragma warning(default:4996)
	return std::to_string(version_info.dwMajorVersion).append(".").append(std::to_string(version_info.dwMinorVersion));
#elif EDGE_PLATFORM_NIX
	std::vector<std::string> qualifiers{ "ID=", "VERSION_ID=" };

	std::ifstream lsbRelease;
	lsbRelease.open("/etc/os-release", std::ifstream::in);

	if (lsbRelease.is_open())
	{
		std::string osVersion;

		for (std::string line; std::getline(lsbRelease, line); )
		{
			for (auto& qualifier : qualifiers)
			{
				if (line.compare(0, qualifier.length(), qualifier) == 0)
				{
					auto value = line.substr(qualifier.length());

					if (value.length() >= 2 && ((value[0] == '"'  && value[value.length() - 1] == '"') || (value[0] == '\'' && value[value.length() - 1] == '\'')))
					{
						value = value.substr(1, value.length() - 2);
					}

					if (value.length() == 0)
					{
						continue;
					}

					if (osVersion.length() > 0)
					{
						osVersion += " ";
					}

					osVersion += value;
				}
			}
		}

		return osVersion;
	}

	return "";
#elif EDGE_PLATFORM_APPLE
	utsname unameData;

	if (uname(&unameData) == 0)
	{
		std::string release = unameData.release;

		auto dot_position = release.find(".");

		if (dot_position == std::string::npos)
		{
			return "10.1";
		}

		auto version = atoi(release.substr(0, dot_position).c_str());
        std::stringstream versionStringStream;
        versionStringStream << (version - 4);

		return std::string("10.").append(versionStringStream.str());
	}

	else
	{
		return "10.1";
	}
#endif
}

HRESULT CoreClrEmbedding::Initialize(BOOL debugMode)
{
    // Much of the CoreCLR bootstrapping process is cribbed from 
    // https://github.com/aspnet/dnx/blob/dev/src/dnx.coreclr.unix/dnx.coreclr.cpp

	DBG("CoreClrEmbedding::Initialize - Started")

    HRESULT result = S_OK;
    char currentDirectory[PATH_MAX];

#ifdef EDGE_PLATFORM_WINDOWS
    if (!_getcwd(&currentDirectory[0], PATH_MAX))
#else
    if (!getcwd(&currentDirectory[0], PATH_MAX))
#endif
    {
		throwV8Exception("Unable to get the current directory.");
        return E_FAIL;
    }

	char edgeNodePath[PATH_MAX];

#ifdef EDGE_PLATFORM_WINDOWS
    HMODULE moduleHandle = NULL;

    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR) &CoreClrEmbedding::Initialize, &moduleHandle);
    GetModuleFileName(moduleHandle, edgeNodePath, PATH_MAX);
    PathRemoveFileSpec(edgeNodePath);
#else
    Dl_info dlInfo;

	dladdr((void*)&CoreClrEmbedding::Initialize, &dlInfo);
	strcpy(edgeNodePath, dlInfo.dli_fname);
	strcpy(edgeNodePath, dirname(edgeNodePath));
#endif

    DBG("CoreClrEmbedding::Initialize - edge.node path is %s", edgeNodePath);

    void* libCoreClr = NULL;
    char bootstrapper[PATH_MAX];

    GetPathToBootstrapper(&bootstrapper[0], PATH_MAX);
    DBG("CoreClrEmbedding::Initialize - Bootstrapper is %s", bootstrapper);

    char coreClrDirectory[PATH_MAX];
    char* coreClrEnvironmentVariable = getenv("CORECLR_DIR");

    if (coreClrEnvironmentVariable)
    {
        if (coreClrEnvironmentVariable[0] == '"')
        {
            strncpy(&coreClrDirectory[0], &coreClrEnvironmentVariable[1], strlen(coreClrEnvironmentVariable) - 2);
            coreClrDirectory[strlen(coreClrEnvironmentVariable) - 2] = '\0';
        }

        else
        {
            strncpy(&coreClrDirectory[0], coreClrEnvironmentVariable, strlen(coreClrEnvironmentVariable) + 1);
        }

    	DBG("CoreClrEmbedding::Initialize - Trying to load %s from the path specified in the CORECLR_DIR environment variable: %s", LIBCORECLR_NAME, coreClrDirectory);

        LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
    }

    if (!libCoreClr)
    {
    	strncpy(&coreClrDirectory[0], currentDirectory, strlen(currentDirectory) + 1);
    	LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
    }

    if (!libCoreClr)
    {
        // Try to load CoreCLR from application path
#ifdef EDGE_PLATFORM_WINDOWS
        char* lastSlash = strrchr(&bootstrapper[0], '\\');
#else
        char* lastSlash = strrchr(&bootstrapper[0], '/');
#endif

        assert(lastSlash);
        strncpy(&coreClrDirectory[0], &bootstrapper[0], lastSlash - &bootstrapper[0]);
        coreClrDirectory[lastSlash - &bootstrapper[0]] = '\0';

        LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
    }

    if (!libCoreClr)
    {
    	std::string pathEnvironmentVariable = getenv("PATH");
#if EDGE_PLATFORM_WINDOWS
    	char delimeter = ';';
#else
    	char delimeter = ':';
#endif

    	size_t previousIndex = 0;
    	size_t currentIndex = pathEnvironmentVariable.find(delimeter);

    	while (!libCoreClr && currentIndex != std::string::npos)
    	{
    		strncpy(&coreClrDirectory[0], pathEnvironmentVariable.substr(previousIndex, currentIndex - previousIndex).c_str(), currentIndex - previousIndex);
    		coreClrDirectory[currentIndex - previousIndex] = '\0';

    		LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);

    		if (!libCoreClr)
    		{
				previousIndex = currentIndex + 1;
				currentIndex = pathEnvironmentVariable.find(delimeter, previousIndex);
    		}
    	}
    }

    if (!libCoreClr)
    {
		throwV8Exception("Failed to find CoreCLR.  Make sure that you have either specified the CoreCLR directory in the CORECLR_DIR environment variable or it exists somewhere in your PATH environment variable, which you do via the \"dnvm install\" and \"dnvm use\" commands.");
        return E_FAIL;
    }

    DBG("CoreClrEmbedding::Initialize - %s loaded successfully from %s", LIBCORECLR_NAME, &coreClrDirectory[0]);

    std::string assemblySearchDirectories;

    assemblySearchDirectories.append(&currentDirectory[0]);
    assemblySearchDirectories.append(":");
    assemblySearchDirectories.append(&coreClrDirectory[0]);

    DBG("CoreClrEmbedding::Initialize - Assembly search path is %s", assemblySearchDirectories.c_str());

    coreclr_initializeFunction initializeCoreCLR = (coreclr_initializeFunction) LoadSymbol(libCoreClr, "coreclr_initialize");

    if (!initializeCoreCLR)
    {
    	throwV8Exception("Error loading the coreclr_initialize function from %s: %s.", LIBCORECLR_NAME, GetLoadError());
        return E_FAIL;
    }

    DBG("CoreClrEmbedding::Initialize - coreclr_initialize loaded successfully");
    coreclr_create_delegateFunction createDelegate = (coreclr_create_delegateFunction) LoadSymbol(libCoreClr, "coreclr_create_delegate");

    if (!createDelegate)
    {
    	throwV8Exception("Error loading the coreclr_create_delegate function from %s: %s.", LIBCORECLR_NAME, GetLoadError());
    	return E_FAIL;
    }

    DBG("CoreClrEmbedding::Initialize - coreclr_create_delegate loaded successfully");

    const char* propertyKeys[] = {
    	"TRUSTED_PLATFORM_ASSEMBLIES",
    	"APP_PATHS",
    	"APP_NI_PATHS",
    	"NATIVE_DLL_SEARCH_DIRECTORIES",
    	"AppDomainCompatSwitch"
    };

    std::string tpaList;
    AddToTpaList(coreClrDirectory, &tpaList);

    std::string appPaths(&currentDirectory[0]);
#if EDGE_PLATFORM_WINDOWS
    appPaths.append(";");
#else
    appPaths.append(":");
#endif
    appPaths.append(edgeNodePath);

    DBG("CoreClrEmbedding::Initialize - Using %s as the app path value", appPaths.c_str());

    const char* propertyValues[] = {
    	tpaList.c_str(),
        appPaths.c_str(),
        appPaths.c_str(),
        assemblySearchDirectories.c_str(),
        "UseLatestBehaviorWhenTFMNotSpecified"
    };

    DBG("CoreClrEmbedding::Initialize - Calling coreclr_initialize()");
	result = initializeCoreCLR(
			bootstrapper,
			"Edge",
			sizeof(propertyKeys) / sizeof(propertyKeys[0]),
			&propertyKeys[0],
			&propertyValues[0],
			&hostHandle,
			&appDomainId);

	if (FAILED(result))
	{
		throwV8Exception("Call to coreclr_initialize() failed with a return code of 0x%x.", result);
		return result;
	}

	DBG("CoreClrEmbedding::Initialize - CoreCLR initialized successfully");
    DBG("CoreClrEmbedding::Initialize - App domain created successfully (app domain ID: %d)", appDomainId);

    SetCallV8FunctionDelegateFunction setCallV8Function;

    CREATE_DELEGATE("GetFunc", &getFunc);
    CREATE_DELEGATE("CallFunc", &callFunc);
    CREATE_DELEGATE("ContinueTask", &continueTask);
    CREATE_DELEGATE("FreeHandle", &freeHandle);
    CREATE_DELEGATE("FreeMarshalData", &freeMarshalData);
    CREATE_DELEGATE("SetCallV8FunctionDelegate", &setCallV8Function);
    CREATE_DELEGATE("CompileFunc", &compileFunc);
	CREATE_DELEGATE("Initialize", &initialize);

	DBG("CoreClrEmbedding::Initialize - Getting runtime info");

	CoreClrGcHandle exception = NULL;
	BootstrapperContext context;

	context.runtimeDirectory = &coreClrDirectory[0];
	context.applicationDirectory = getenv("EDGE_APP_ROOT");
	context.edgeNodePath = &edgeNodePath[0];

	if (!context.applicationDirectory)
	{
		context.applicationDirectory = &currentDirectory[0];
	}

	std::string operatingSystem = GetOSName();
	std::string operatingSystemVersion = GetOSVersion();

	context.architecture = GetOSArchitecture();
	context.operatingSystem = operatingSystem.c_str();
	context.operatingSystemVersion = operatingSystemVersion.c_str();

	DBG("CoreClrEmbedding::Initialize - Operating system: %s", context.operatingSystem);
	DBG("CoreClrEmbedding::Initialize - Operating system version: %s", context.operatingSystemVersion);
	DBG("CoreClrEmbedding::Initialize - Architecture: %s", context.architecture);
	DBG("CoreClrEmbedding::Initialize - Runtime directory: %s", context.runtimeDirectory);
	DBG("CoreClrEmbedding::Initialize - Application directory: %s", context.applicationDirectory);

	DBG("CoreClrEmbedding::Initialize - Calling CLR Initialize() function");

	initialize(&context, &exception);

	if (exception)
	{
		v8::Local<v8::Value> v8Exception = CoreClrFunc::MarshalCLRToV8(exception, V8TypeException);
		FreeMarshalData(exception, V8TypeException);

		throwV8Exception(v8Exception);
	}

	else
	{
		DBG("CoreClrEmbedding::Initialize - CLR Initialize() function called successfully")
	}

	exception = NULL;
	setCallV8Function(CoreClrNodejsFunc::Call, &exception);

	if (exception)
	{
		v8::Local<v8::Value> v8Exception = CoreClrFunc::MarshalCLRToV8(exception, V8TypeException);
		FreeMarshalData(exception, V8TypeException);

		throwV8Exception(v8Exception);
	}

	else
	{
		DBG("CoreClrEmbedding::Initialize - CallV8Function delegate set successfully");
	}

	DBG("CoreClrEmbedding::Initialize - Completed");

    return S_OK;
}

CoreClrGcHandle CoreClrEmbedding::GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName, v8::Local<v8::Value>* v8Exception)
{
	DBG("CoreClrEmbedding::GetClrFuncReflectionWrapFunc - Starting");

	CoreClrGcHandle exception;
	CoreClrGcHandle function = getFunc(assemblyFile, typeName, methodName, &exception);

	if (exception)
	{
		*v8Exception = CoreClrFunc::MarshalCLRToV8(exception, V8TypeException);
		FreeMarshalData(exception, V8TypeException);

		return NULL;
	}

	else
	{
		DBG("CoreClrEmbedding::GetClrFuncReflectionWrapFunc - Finished");
		return function;
	}
}

#ifdef EDGE_PLATFORM_WINDOWS
void CoreClrEmbedding::AddToTpaList(std::string directoryPath, std::string* tpaList)
{
    const char * const tpaExtensions[] = {
        ".ni.dll",
        ".dll",
        ".ni.exe",
        ".exe"
    };

    std::string directoryPathWithFilter(directoryPath);
    directoryPathWithFilter.append("\\*");

    WIN32_FIND_DATA directoryEntry;
    HANDLE fileHandle = FindFirstFile(directoryPathWithFilter.c_str(), &directoryEntry);

    if (fileHandle == INVALID_HANDLE_VALUE)
    {
        return;
    }

    DBG("CoreClrEmbedding::AddToTpaList - Searching %s for assemblies to add to the TPA list", directoryPath.c_str());

    std::set<std::string> addedAssemblies;

    // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
    // then files with .dll extension, etc.
    for (int extensionIndex = 0; extensionIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extensionIndex++)
    {
        const char* currentExtension = tpaExtensions[extensionIndex];
        size_t currentExtensionLength = strlen(currentExtension);

        // For all entries in the directory
        do
        {
            if ((directoryEntry.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY)
            {
                continue;
            }

            std::string filename(directoryEntry.cFileName);

            // Check if the extension matches the one we are looking for
            size_t extensionPosition = filename.length() - currentExtensionLength;

            if ((extensionPosition <= 0) || (filename.compare(extensionPosition, currentExtensionLength, currentExtension) != 0))
            {
                continue;
            }

            std::string filenameWithoutExtension(filename.substr(0, extensionPosition));

            // Make sure if we have an assembly with multiple extensions present,
            // we insert only one version of it.
            if (addedAssemblies.find(filenameWithoutExtension) == addedAssemblies.end())
            {
                addedAssemblies.insert(filenameWithoutExtension);

                tpaList->append(directoryPath);
#ifdef EDGE_PLATFORM_WINDOWS
                tpaList->append("\\");
#else
                tpaList->append("/");
#endif
                tpaList->append(filename);
#ifdef EDGE_PLATFORM_WINDOWS
                tpaList->append(";");
#else
                tpaList->append(":");
#endif

                DBG("CoreClrEmbedding::AddToTpaList - Added %s to the TPA list", filename.c_str());
            }
        }
        while (FindNextFile(fileHandle, &directoryEntry));

        FindClose(fileHandle);

        // Rewind the directory stream to be able to iterate over it for the next extension
        fileHandle = FindFirstFile(directoryPathWithFilter.c_str(), &directoryEntry);
    }

    FindClose(fileHandle);
}
#else
void CoreClrEmbedding::AddToTpaList(std::string directoryPath, std::string* tpaList)
{
    const char * const tpaExtensions[] = {
        ".ni.dll",
        ".dll",
        ".ni.exe",
        ".exe"
    };
    DIR* directory = opendir(directoryPath.c_str());
    struct dirent* directoryEntry;
    std::set<std::string> addedAssemblies;

    // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
    // then files with .dll extension, etc.
    for (uint extensionIndex = 0; extensionIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extensionIndex++)
    {
        const char* currentExtension = tpaExtensions[extensionIndex];
        int currentExtensionLength = strlen(currentExtension);

        // For all entries in the directory
        while ((directoryEntry = readdir(directory)) != NULL)
        {
            // We are interested in files only
            switch (directoryEntry->d_type)
            {
                case DT_REG:
                    break;

                // Handle symlinks and file systems that do not support d_type
                case DT_LNK:
                case DT_UNKNOWN:
                    {
                        std::string fullFilename;

                        fullFilename.append(directoryPath);
#ifdef EDGE_PLATFORM_WINDOWS
                        fullFilename.append("\\");
#else
                        fullFilename.append("/");
#endif
                        fullFilename.append(directoryEntry->d_name);

                        struct stat fileInfo;

                        if (stat(fullFilename.c_str(), &fileInfo) == -1)
                        {
                            continue;
                        }

                        if (!S_ISREG(fileInfo.st_mode))
                        {
                            continue;
                        }
                    }

                    break;

                default:
                    continue;
            }

            std::string filename(directoryEntry->d_name);

            // Check if the extension matches the one we are looking for
            int extensionPosition = filename.length() - currentExtensionLength;

            if ((extensionPosition <= 0) || (filename.compare(extensionPosition, currentExtensionLength, currentExtension) != 0))
            {
                continue;
            }

            std::string filenameWithoutExtension(filename.substr(0, extensionPosition));

            // Make sure if we have an assembly with multiple extensions present,
            // we insert only one version of it.
            if (addedAssemblies.find(filenameWithoutExtension) == addedAssemblies.end())
            {
                addedAssemblies.insert(filenameWithoutExtension);

                tpaList->append(directoryPath);
#ifdef EDGE_PLATFORM_WINDOWS
                tpaList->append("\\");
#else
                tpaList->append("/");
#endif
                tpaList->append(filename);
                tpaList->append(":");
            }
        }

        // Rewind the directory stream to be able to iterate over it for the next extension
        rewinddir(directory);
    }

    closedir(directory);
}
#endif

void CoreClrEmbedding::FreeCoreClr(void** libCoreClr)
{
    if (libCoreClr)
    {
#ifdef EDGE_PLATFORM_WINDOWS
        FreeLibrary((HMODULE) libCoreClr);
#else
        dlclose(libCoreClr);
#endif
        libCoreClr = NULL;
    }
}

bool CoreClrEmbedding::LoadCoreClrAtPath(const char* loadPath, void** libCoreClrPointer)
{
    std::string coreClrDllPath(loadPath);

    DBG("CoreClrEmbedding::LoadCoreClrAtPath - Trying to load %s from %s", LIBCORECLR_NAME, loadPath);

#ifdef EDGE_PLATFORM_WINDOWS
    coreClrDllPath.append("\\");
#else
    coreClrDllPath.append("/");
#endif
    coreClrDllPath.append(LIBCORECLR_NAME);

#ifdef EDGE_PLATFORM_WINDOWS
    *libCoreClrPointer = LoadLibrary(coreClrDllPath.c_str());
#else
    *libCoreClrPointer = dlopen(coreClrDllPath.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif

    if (*libCoreClrPointer == NULL )
    {
        DBG("CoreClrEmbedding::LoadCoreClrAtPath - Errors loading %s from %s: %s", LIBCORECLR_NAME, loadPath, GetLoadError());
    }

    else
    {
    	DBG("CoreClrEmbedding::LoadCoreClrAtPath - Load of %s succeeded", LIBCORECLR_NAME);
    }

    return *libCoreClrPointer != NULL;
}

void* CoreClrEmbedding::LoadSymbol(void* library, const char* symbolName)
{
#ifdef EDGE_PLATFORM_WINDOWS
    return GetProcAddress((HMODULE) library, symbolName);
#else
    return dlsym(library, symbolName);
#endif
}

char* CoreClrEmbedding::GetLoadError()
{
#ifdef EDGE_PLATFORM_WINDOWS
    LPVOID message;
    FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &message, 0, NULL);

    return (char*) message;
#else
    return dlerror();
#endif
}

void CoreClrEmbedding::GetPathToBootstrapper(char* pathToBootstrapper, size_t bufferSize)
{
#ifdef EDGE_PLATFORM_APPLE
    ssize_t pathLength = proc_pidpath(getpid(), pathToBootstrapper, bufferSize);
#elif defined(EDGE_PLATFORM_WINDOWS)
    DWORD dwBufferSize;
    SIZETToDWord(bufferSize, &dwBufferSize);

    size_t pathLength = GetModuleFileName(GetModuleHandle(NULL), pathToBootstrapper, dwBufferSize);
#else
    ssize_t pathLength = readlink("/proc/self/exe", pathToBootstrapper, bufferSize);
#endif
    assert(pathLength > 0);

    // ensure pathToBootstrapper is null terminated, readlink for example
    // will not null terminate it.
    pathToBootstrapper[pathLength] = '\0';
}

void CoreClrEmbedding::CallClrFunc(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType)
{
	DBG("CoreClrEmbedding::CallClrFunc");
	callFunc(functionHandle, payload, payloadType, taskState, result, resultType);
}

void CoreClrEmbedding::ContinueTask(CoreClrGcHandle taskHandle, void* context, TaskCompleteFunction callback, void** exception)
{
	DBG("CoreClrEmbedding::ContinueTask");
	continueTask(taskHandle, context, callback, exception);
}

void CoreClrEmbedding::FreeHandle(CoreClrGcHandle handle)
{
	DBG("CoreClrEmbedding::FreeHandle");
	freeHandle(handle);
}

void CoreClrEmbedding::FreeMarshalData(void* marshalData, int marshalDataType)
{
	DBG("CoreClrEmbedding::FreeMarshalData");
	freeMarshalData(marshalData, marshalDataType);
}

CoreClrGcHandle CoreClrEmbedding::CompileFunc(const void* options, const int payloadType, v8::Local<v8::Value>* v8Exception)
{
    DBG("CoreClrEmbedding::CompileFunc - Starting");

    CoreClrGcHandle exception;
    CoreClrGcHandle function = compileFunc(options, payloadType, &exception);

    if (exception)
    {
        *v8Exception = CoreClrFunc::MarshalCLRToV8(exception, V8TypeException);
        FreeMarshalData(exception, V8TypeException);

        return NULL;
    }

    else
    {
        DBG("CoreClrEmbedding::CompileFunc - Finished");
        return function;
    }
}
