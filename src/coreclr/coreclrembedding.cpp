#include "edge.h"
#include <dlfcn.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <unistd.h>
#include <set>
#include <dirent.h>
#include <dlfcn.h>
#include <sys/stat.h>
#include <libgen.h>

#if PLATFORM_DARWIN
#include <libproc.h>
#endif

#ifdef PLATFORM_DARWIN
const char* LIBCORECLR_NAME = "libcoreclr.dylib";
#else
const char* LIBCORECLR_NAME = "libcoreclr.so";
#endif

typedef HRESULT (*GetCLRRuntimeHostFunction)(REFIID id, IUnknown** host);
typedef HRESULT (*PAL_InitializeCoreCLRFunction)(const char *szExePath, const char *szCoreCLRPath, bool fStayInPAL);

DWORD appDomainId;
GetFuncFunction getFunc;
CallFuncFunction callFunc;
ContinueTaskFunction continueTask;
FreeHandleFunction freeHandle;

HRESULT CoreClrEmbedding::Initialize(BOOL debugMode)
{
    // Much of the CoreCLR bootstrapping process is cribbed from 
    // https://github.com/aspnet/dnx/blob/dev/src/dnx.coreclr.unix/dnx.coreclr.cpp

	DBG("CoreClrEmbedding::Initialize - Started")

    HRESULT result = S_OK;
    char currentDirectory[PATH_MAX];

    if (!getcwd(&currentDirectory[0], PATH_MAX))
    {
    	NanThrowError("Unable to get the current directory.");
        return E_FAIL;
    }

    Dl_info dlInfo;
	char edgeNodePath[PATH_MAX];

	dladdr((void*)&CoreClrEmbedding::Initialize, &dlInfo);
	strcpy(edgeNodePath, dlInfo.dli_fname);
	strcpy(edgeNodePath, dirname(edgeNodePath));

    void* libCoreClr = NULL;
    char bootstrapper[PATH_MAX];

    GetPathToBootstrapper(&bootstrapper[0], PATH_MAX);
    DBG("CoreClrEmbedding::Initialize - Bootstrapper is %s", bootstrapper);

    char coreClrDirectory[PATH_MAX];
    char* coreClrEnvironmentVariable = getenv("CORECLR_DIR");

    if (coreClrEnvironmentVariable)
    {
    	DBG("CoreClrEmbedding::Initialize - Trying to load %s from the path specified in the CORECLR_DIR environment variable: %s", LIBCORECLR_NAME, coreClrEnvironmentVariable);

    	strncpy(&coreClrDirectory[0], coreClrEnvironmentVariable, strlen(coreClrEnvironmentVariable) + 1);
        LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
    }

    // TODO: check the current directory

    if (!libCoreClr)
    {
        // Try to load CoreCLR from application path
        char* lastSlash = strrchr(&bootstrapper[0], '/');

        assert(lastSlash);
        strncpy(&coreClrDirectory[0], &bootstrapper[0], lastSlash - &bootstrapper[0]);
        coreClrDirectory[lastSlash - &bootstrapper[0]] = '\0';

        LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
    }

    // TODO: check the directories in the PATH variable if there's nothing in CORECLR_DIR or the bootstrapper directory

    if (!libCoreClr)
    {
    	NanThrowError("Failed to find CoreCLR.  Make sure that you have either specified the CoreCLR directory in the CORECLR_DIR environment variable or it exists somewhere in your PATH environment variable, which you do via the \"dnvm install\" and \"dnvm use\" commands.");
        return E_FAIL;
    }

    DBG("CoreClrEmbedding::Initialize - %s loaded successfully from %s", LIBCORECLR_NAME, &coreClrDirectory[0]);

    std::string assemblySearchDirectories;

    assemblySearchDirectories.append(&currentDirectory[0]);
    assemblySearchDirectories.append(":");
    assemblySearchDirectories.append(&coreClrDirectory[0]);

    DBG("CoreClrEmbedding::Initialize - Assembly search path is %s", assemblySearchDirectories.c_str());

    ICLRRuntimeHost2* runtimeHost = NULL;

    PAL_InitializeCoreCLRFunction initializeCoreCLR = (PAL_InitializeCoreCLRFunction) dlsym(libCoreClr, "PAL_InitializeCoreCLR");

    if (!initializeCoreCLR)
    {
    	throwV8Exception("Error loading the PAL_InitializeCoreCLR function from %s: %s.", LIBCORECLR_NAME, dlerror());
        return E_FAIL;
    }

    GetCLRRuntimeHostFunction getCLRRuntimeHost = (GetCLRRuntimeHostFunction) dlsym(libCoreClr, "GetCLRRuntimeHost");

    if (!getCLRRuntimeHost)
    {
    	throwV8Exception("Error loading the GetCLRRuntimeHost function from %s: %s.", LIBCORECLR_NAME, dlerror());
    	return E_FAIL;
    }

    REFIID clrRuntimeHostGuid = (REFIID) dlsym(libCoreClr, "IID_ICLRRuntimeHost2");

    if (!clrRuntimeHostGuid)
    {
    	throwV8Exception("Error loading the IID_ICLRRuntimeHost2 GUID from %s: %s.", LIBCORECLR_NAME, dlerror());
    	return E_FAIL;
    }

    std::string coreClrDllPath(&coreClrDirectory[0]);

    coreClrDllPath.append("/");
    coreClrDllPath.append(LIBCORECLR_NAME);

    DBG("CoreClrEmbedding::Initialize - Calling PAL_InitializeCoreCLR()");
    result = initializeCoreCLR(bootstrapper, coreClrDllPath.c_str(), true);

    if (FAILED(result))
    {
    	throwV8Exception("Call to PAL_InitializeCoreCLR() failed with a return code of 0x%x.", result);
        return result;
    }

    DBG("CoreClrEmbedding::Initialize - CoreCLR initialized successfully");
    result = getCLRRuntimeHost(clrRuntimeHostGuid, (void**) &runtimeHost);

    if (FAILED(result))
    {
    	throwV8Exception("Call to GetCLRRuntimeHost() failed with a return code of 0x%x.", result);
        return result;
    }

    DBG("CoreClrEmbedding::Initialize - Got the runtime host successfully");
    result = runtimeHost->SetStartupFlags((STARTUP_FLAGS) (STARTUP_LOADER_OPTIMIZATION_SINGLE_DOMAIN | STARTUP_SINGLE_APPDOMAIN));

    if (FAILED(result))
    {
    	throwV8Exception("Call to ICLRRuntimeHost2::SetStartupFlags() failed with a return code of 0x%x.", result);
        return result;
    }

    DBG("CoreClrEmbedding::Initialize - Set runtime host startup flags successfully");
    result = runtimeHost->Start();

    if (FAILED(result))
    {
    	throwV8Exception("Call to ICLRRuntimeHost2::Start() failed with a return code of 0x%x.", result);
        return result;
    }

    DBG("CoreClrEmbedding::Initialize - Runtime host started successfully");

    LPCWSTR propertyKeys[] = {
    	u"TRUSTED_PLATFORM_ASSEMBLIES",
    	u"APP_PATHS",
    	u"APP_NI_PATHS",
    	u"NATIVE_DLL_SEARCH_DIRECTORIES",
    	u"AppDomainCompatSwitch"
    };

    std::string tpaList;
    AddToTpaList(coreClrDirectory, &tpaList);

    std::u16string utf16TpaList;
    StringToUTF16(tpaList, utf16TpaList);

    std::string appPaths(&currentDirectory[0]);
    appPaths.append(":");
    appPaths.append(edgeNodePath);

    std::u16string utf16AppPaths;
    StringToUTF16(appPaths, utf16AppPaths);

    std::u16string utf16AssemblySearchDirectories;
    StringToUTF16(assemblySearchDirectories, utf16AssemblySearchDirectories);

    LPCWSTR propertyValues[] = {
    	utf16TpaList.c_str(),
        utf16AppPaths.c_str(),
        utf16AppPaths.c_str(),
        utf16AssemblySearchDirectories.c_str(),
        u"UseLatestBehaviorWhenTFMNotSpecified"
    };

    result = runtimeHost->CreateAppDomainWithManager(
    	u"Edge",
        APPDOMAIN_ENABLE_PLATFORM_SPECIFIC_APPS |
            APPDOMAIN_ENABLE_PINVOKE_AND_CLASSIC_COMINTEROP |
            APPDOMAIN_DISABLE_TRANSPARENCY_ENFORCEMENT |
            APPDOMAIN_ENABLE_ASSEMBLY_LOADFILE,
        NULL,
        NULL,
        sizeof(propertyKeys) / sizeof(propertyKeys[0]),
        &propertyKeys[0],
        &propertyValues[0],
        &appDomainId);

    if (FAILED(result))
    {
    	throwV8Exception("Call to ICLRRuntimeHost2::CreateAppDomainWithManager() failed with a return code of 0x%x.", result);
        return result;
    }

    DBG("CoreClrEmbedding::Initialize - App domain created successfully (app domain ID: %d)", appDomainId);

    result = runtimeHost->CreateDelegate(
    		appDomainId,
    		u"CoreCLREmbedding",
    		u"CoreCLREmbedding",
    		u"GetFunc",
    		(INT_PTR*) &getFunc);

    if (FAILED(result))
    {
    	throwV8Exception("Call to ICLRRuntimeHost2::CreateDelegate() for GetFunc failed with a return code of 0x%x.", result);
        return result;
    }

    DBG("CoreClrEmbedding::Initialize - CoreCLREmbedding.GetFunc() loaded successfully");

	result = runtimeHost->CreateDelegate(
				appDomainId,
				u"CoreCLREmbedding",
				u"CoreCLREmbedding",
				u"CallFunc",
				(INT_PTR*) &callFunc);

	if (FAILED(result))
	{
		throwV8Exception("Call to ICLRRuntimeHost2::CreateDelegate() for CallFunc failed with a return code of 0x%x.", result);
		return result;
	}

	DBG("CoreClrEmbedding::Initialize - CoreCLREmbedding.CallFunc() loaded successfully");

	result = runtimeHost->CreateDelegate(
				appDomainId,
				u"CoreCLREmbedding",
				u"CoreCLREmbedding",
				u"ContinueTask",
				(INT_PTR*) &continueTask);

	if (FAILED(result))
	{
		throwV8Exception("Call to ICLRRuntimeHost2::CreateDelegate() for ContinueTask failed with a return code of 0x%x.", result);
		return result;
	}

	DBG("CoreClrEmbedding::Initialize - CoreCLREmbedding.ContinueTask() loaded successfully");

	result = runtimeHost->CreateDelegate(
				appDomainId,
				u"CoreCLREmbedding",
				u"CoreCLREmbedding",
				u"FreeHandle",
				(INT_PTR*) &freeHandle);

	if (FAILED(result))
	{
		throwV8Exception("Call to ICLRRuntimeHost2::CreateDelegate() for FreeHandle failed with a return code of 0x%x.", result);
		return result;
	}

	DBG("CoreClrEmbedding::Initialize - CoreCLREmbedding.FreeHandle() loaded successfully");

    SetDebugModeFunction setDebugMode;
    result = runtimeHost->CreateDelegate(
        		appDomainId,
        		u"CoreCLREmbedding",
        		u"CoreCLREmbedding",
        		u"SetDebugMode",
        		(INT_PTR*) &setDebugMode);

	if (FAILED(result))
	{
		throwV8Exception("Call to ICLRRuntimeHost2::CreateDelegate() for SetDebugMode failed with a return code of 0x%x.", result);
		return result;
	}

	DBG("CoreClrEmbedding::Initialize - CoreCLREmbedding.SetDebugMode() loaded successfully");

	setDebugMode(debugMode);
	DBG("CoreClrEmbedding::Initialize - Debug mode set successfully");

	DBG("CoreClrEmbedding::Initialize - Completed");

    return S_OK;
}

CoreClrGcHandle CoreClrEmbedding::GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName)
{
	return getFunc(assemblyFile, typeName, methodName);
}

void CoreClrEmbedding::AddToTpaList(std::string directoryPath, std::string* tpaList)
{
    const char * const tpaExtensions[] = {
        ".ni.dll",
        ".dll",
        ".ni.exe",
        ".exe"
    };

    DIR* directory = opendir(directoryPath.c_str());
    std::set<std::string> addedAssemblies;

    // Walk the directory for each extension separately so that we first get files with .ni.dll extension,
    // then files with .dll extension, etc.
    for (uint extensionIndex = 0; extensionIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extensionIndex++)
    {
        const char* currentExtension = tpaExtensions[extensionIndex];
        int currentExtensionLength = strlen(currentExtension);

        struct dirent* directoryEntry;

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
                        fullFilename.append("/");
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
                tpaList->append("/");
                tpaList->append(filename);
                tpaList->append(":");
            }
        }

        // Rewind the directory stream to be able to iterate over it for the next extension
        rewinddir(directory);
    }

    closedir(directory);
}

void CoreClrEmbedding::FreeCoreClr(void** libCoreClr)
{
    if (libCoreClr)
    {
        dlclose(libCoreClr);
        libCoreClr = NULL;
    }
}

bool CoreClrEmbedding::LoadCoreClrAtPath(const char* loadPath, void** libCoreClrPointer)
{
    std::string coreClrDllPath(loadPath);

    DBG("CoreClrEmbedding::LoadCoreClrAtPath - Trying to load %s from %s", LIBCORECLR_NAME, loadPath);

    coreClrDllPath.append("/");
    coreClrDllPath.append(LIBCORECLR_NAME);

    *libCoreClrPointer = dlopen(coreClrDllPath.c_str(), RTLD_NOW | RTLD_GLOBAL);

    if (*libCoreClrPointer == NULL )
    {
        DBG("CoreClrEmbedding::LoadCoreClrAtPath - Errors loading %s from %s: %s", LIBCORECLR_NAME, loadPath, dlerror());
    }

    else
    {
    	DBG("CoreClrEmbedding::LoadCoreClrAtPath - Load of %s succeeded", LIBCORECLR_NAME);
    }

    return *libCoreClrPointer != NULL;
}

void CoreClrEmbedding::GetPathToBootstrapper(char* pathToBootstrapper, size_t bufferSize)
{
#ifdef PLATFORM_DARWIN
    ssize_t pathLength = proc_pidpath(getpid(), pathToBootstrapper, bufferSize);
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

void CoreClrEmbedding::ContinueTask(CoreClrGcHandle taskHandle, void* context, TaskCompleteFunction callback)
{
	DBG("CoreClrEmbedding::ContinueTask");
	continueTask(taskHandle, context, callback);
}

void CoreClrEmbedding::FreeHandle(CoreClrGcHandle handle)
{
	freeHandle(handle);
}
