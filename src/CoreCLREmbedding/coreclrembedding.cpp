#include "edge.h"
#include <limits.h>
#include <set>
#include <sys/stat.h>
#include "pal/pal_utils.h"
#include "pal/trace.h"
#include "fxr/fx_ver.h"
#include "cpprest/json.h"
#include "deps/deps_format.h"
#include "deps/deps_entry.h"

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

typedef pal::hresult_t (STDMETHODCALLTYPE *coreclr_create_delegate)(
		void* hostHandle,
		unsigned int domainId,
		const char* assemblyName,
		const char* typeName,
		const char* methodName,
		void** delegate);
typedef pal::hresult_t (STDMETHODCALLTYPE *coreclr_initialize)(
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
			"EdgeJs",\
			"CoreCLREmbedding",\
			functionName,\
			(void**) functionPointer);\
	pal::clr_palstring(functionName, &functionNameString);\
	\
	if (FAILED(result))\
	{\
		throwV8Exception("Call to coreclr_create_delegate() for %s failed with a return code of 0x%x.", functionNameString.c_str(), result);\
		return result;\
	}\
	\
	trace::info(_X("CoreClrEmbedding::Initialize - CoreCLREmbedding.%s() loaded successfully"), functionNameString.c_str());\

pal::string_t GetOSName()
{
#if EDGE_PLATFORM_WINDOWS
	return _X("win");
#elif EDGE_PLATFORM_APPLE
	return _X("osx");
#else
	utsname unameData;

	if (uname(&unameData) == 0)
	{
		return pal::string_t(unameData.sysname);
	}

	// uname() failed, falling back to defaults
	else
	{
		return _X("unix");
	}
#endif
}

pal::string_t GetOSArchitecture()
{
#if defined __X86__ || defined __i386__ || defined i386 || defined _M_IX86 || defined __386__
	return _X("x86");
#elif defined __ia64 || defined _M_IA64 || defined __ia64__ || defined __x86_64__ || defined _M_X64
	return _X("x64");
#elif defined ARM || defined __arm__ || defined _ARM
	return _X("arm");
#endif
}

pal::string_t GetOSVersion()
{
#if EDGE_PLATFORM_WINDOWS
	OSVERSIONINFO version_info;
	ZeroMemory(&version_info, sizeof(OSVERSIONINFO));
	version_info.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

#pragma warning(disable:4996)
	GetVersionEx(&version_info);
#pragma warning(default:4996)

	if (version_info.dwMajorVersion == 6)
	{
		if (version_info.dwMinorVersion == 1)
		{
			return _X("7");
		}

		else if (version_info.dwMinorVersion == 2)
		{
			return _X("8");
		}

		else if (version_info.dwMinorVersion == 3)
		{
			return _X("81");
		}
	}

	else if (version_info.dwMajorVersion == 10 && version_info.dwMinorVersion == 0)
	{
		return _X("10");
	}

	return _X("");
#elif EDGE_PLATFORM_NIX
	utility::ifstream_t lsbRelease;
	lsbRelease.open("/etc/os-release", std::ifstream::in);

	if (lsbRelease.is_open())
	{
		for (utility::string_t line; std::getline(lsbRelease, line); )
		{
			if (line.substr(0, 11) == _X("VERSION_ID="))
			{
				utility::string_t osVersion = line.substr(3);

				if ((osVersion[0] == '"' && osVersion[osVersion.length() - 1] == '"') || ((osVersion[0] == '\'' && osVersion[osVersion.length() - 1] == '\'')))
				{
					osVersion = osVersion.substr(1, osVersion.length() - 2);
				}

				return _X(".") + osVersion;
			}
		}
	}

	return _X("");
#elif EDGE_PLATFORM_APPLE
	utsname unameData;

	if (uname(&unameData) == 0)
	{
		std::string release = unameData.release;
		auto dot_position = release.find(".");

		if (dot_position == std::string::npos)
		{
			return _X("10.0");
		}

		auto version = atoi(release.substr(0, dot_position).c_str());
        pal::stringstream_t versionStringStream;
        versionStringStream << (version - 4);

		return pal::string_t("10.").append(versionStringStream.str());
	}

	else
	{
		return _X("10.0");
	}
#endif
}

HRESULT CoreClrEmbedding::Initialize(BOOL debugMode)
{
	trace::setup();

	pal::string_t edgeDebug;
	pal::getenv(_X("EDGE_DEBUG"), &edgeDebug);

	if (edgeDebug.length() > 0)
	{
		trace::enable();
	}

	trace::info(_X("CoreClrEmbedding::Initialize - Started"));

    HRESULT result = S_OK;
	pal::string_t functionNameString;
	pal::string_t currentDirectory;

    if (!pal::getcwd(&currentDirectory))
    {
		throwV8Exception("Unable to get the current directory.");
        return E_FAIL;
    }

	pal::string_t edgeNodePath;
	std::vector<char> edgeNodePathCstr;

	char tempEdgeNodePath[PATH_MAX];

#ifdef EDGE_PLATFORM_WINDOWS
    HMODULE moduleHandle = NULL;

    GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR) &CoreClrEmbedding::Initialize, &moduleHandle);
    GetModuleFileName(moduleHandle, tempEdgeNodePath, PATH_MAX);
#else
    Dl_info dlInfo;

	dladdr((void*)&CoreClrEmbedding::Initialize, &dlInfo);
	strcpy(tempEdgeNodePath, dlInfo.dli_fname);
#endif

	pal::clr_palstring(tempEdgeNodePath, &edgeNodePath);
	edgeNodePath = get_directory(edgeNodePath);

	pal::pal_clrstring(edgeNodePath, &edgeNodePathCstr);

    trace::info(_X("CoreClrEmbedding::Initialize - edge.node path is %s"), edgeNodePath.c_str());

    pal::dll_t libCoreClr = NULL;
    pal::string_t bootstrapper;

    pal::get_own_executable_path(&bootstrapper);
	trace::info(_X("CoreClrEmbedding::Initialize - Bootstrapper is %s"), bootstrapper.c_str());

	pal::realpath(&bootstrapper);
    trace::info(_X("CoreClrEmbedding::Initialize - Resolved bootstrapper is %s"), bootstrapper.c_str());

	pal::string_t edgeAppDir;
	pal::getenv(_X("EDGE_APP_ROOT"), &edgeAppDir);

	pal::string_t edgeBootstrapDir;
	pal::getenv(_X("EDGE_BOOTSTRAP_DIR"), &edgeBootstrapDir);

	if (edgeAppDir.length() == 0)
	{
		if (edgeBootstrapDir.length() != 0)
		{
			trace::info(_X("CoreClrEmbedding::Initialize - No EDGE_APP_ROOT environment variable present, using the Edge bootstrapper directory at %s"), edgeBootstrapDir.c_str());
			edgeAppDir = edgeBootstrapDir;
		}

		else
		{
			trace::info(_X("CoreClrEmbedding::Initialize - No EDGE_APP_ROOT environment variable present, using the current directory at %s"), currentDirectory.c_str());
			edgeAppDir = currentDirectory;
		}
	}

	pal::string_t coreClrDirectory;
	pal::string_t coreClrEnvironmentVariable;
	pal::string_t dependencyManifestFile;
	pal::string_t frameworkDependencyManifestFile;

	trace::info(_X("CoreClrEmbedding::Initialize - Getting the dependency manifest file for the Edge app directory: %s"), edgeAppDir.c_str());

	std::vector<pal::string_t> depsJsonFiles;
	pal::readdir(edgeAppDir, _X("*.deps.json"), &depsJsonFiles);

	if (depsJsonFiles.size() > 1)
	{
		std::vector<char> edgeAppDirCstr;
		pal::pal_clrstring(edgeAppDir, &edgeAppDirCstr);

		throwV8Exception("Multiple dependency manifest (*.deps.json) files exist in the Edge.js application directory (%s).", edgeAppDirCstr.data());
		return E_FAIL;
	}

	else if (depsJsonFiles.size() == 1)
	{
		dependencyManifestFile = pal::string_t(edgeAppDir);
		append_path(&dependencyManifestFile, depsJsonFiles[0].c_str());

		trace::info(_X("CoreClrEmbedding::Initialize - Exactly one (%s) dependency manifest file found in the Edge app directory, using it"), dependencyManifestFile.c_str());
	}

	pal::getenv(_X("CORECLR_DIR"), &coreClrEnvironmentVariable);

    if (coreClrEnvironmentVariable.length() > 0)
    {
        trace::info(_X("CoreClrEmbedding::Initialize - Trying to load %s from the path specified in the CORECLR_DIR environment variable: %s"), LIBCORECLR_NAME, coreClrDirectory.c_str());
        LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);

		if (!libCoreClr)
		{
			std::vector<char> coreClrEnvironmentVariableCstr;
			pal::pal_clrstring(coreClrEnvironmentVariable, &coreClrEnvironmentVariableCstr);

			throwV8Exception("Unable to load the CLR from the directory (%s) specified in the CORECLR_DIR environment variable.", coreClrEnvironmentVariableCstr.data());
			return E_FAIL;
		}
    }

	if (!libCoreClr)
	{
		coreClrDirectory = edgeAppDir.length() > 0 ? edgeAppDir : currentDirectory;
		LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
	}

    if (!libCoreClr)
    {
		pal::string_t pathEnvironmentVariable;
		pal::getenv(_X("PATH"), &pathEnvironmentVariable);

        trace::info(_X("CoreClrEmbedding::Initialize - Path is %s"), pathEnvironmentVariable.c_str());

		pal::string_t dotnetExecutablePath;

    	size_t previousIndex = 0;
    	size_t currentIndex = pathEnvironmentVariable.find(PATH_SEPARATOR);

    	while (!libCoreClr && previousIndex != std::string::npos)
    	{
    		if (currentIndex != std::string::npos)
    		{
				dotnetExecutablePath = pathEnvironmentVariable.substr(previousIndex, currentIndex - previousIndex);
			}

			else
			{
				dotnetExecutablePath = pathEnvironmentVariable.substr(previousIndex);
			}

			append_path(&dotnetExecutablePath, _X("dotnet"));
			dotnetExecutablePath.append(pal::exe_suffix());

			trace::info(_X("CoreClrEmbedding::Initialize - Checking for dotnet at %s"), dotnetExecutablePath.c_str());

			if (pal::file_exists(dotnetExecutablePath))
			{
				pal::realpath(&dotnetExecutablePath);

				coreClrDirectory = get_directory(dotnetExecutablePath);
				trace::info(_X("CoreClrEmbedding::Initialize - Found dotnet directory at %s"), coreClrDirectory.c_str());

				append_path(&coreClrDirectory, _X("shared"));
				append_path(&coreClrDirectory, _X("Microsoft.NETCore.App"));

				pal::string_t coreClrVersion;
				pal::getenv(_X("CORECLR_VERSION"), &coreClrVersion);

				if (coreClrVersion.length() > 0)
				{
					trace::info(_X("CoreClrEmbedding::Initialize - CORECLR_VERSION environment variable provided (%s)"), coreClrVersion.c_str());

					append_path(&coreClrDirectory, coreClrVersion.c_str());
					LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);

					if (!libCoreClr)
					{
						std::vector<char> coreClrVersionCstr;
						pal::pal_clrstring(coreClrVersion, &coreClrVersionCstr);

						throwV8Exception("The targeted CLR version (%s) specified in the CORECLR_VERSION environment variable was not found.", coreClrVersionCstr.data());
						return E_FAIL;
					}
				}

				else if (dependencyManifestFile.length() > 0)
				{
					trace::info(_X("CoreClrEmbedding::Initialize - Checking for a framework version in the application dependency manifest"));

					pal::ifstream_t dependencyManifestStream(dependencyManifestFile);

					if (!dependencyManifestStream.good())
					{
						std::vector<char> manifestFileCstr;
						pal::pal_clrstring(dependencyManifestFile, &manifestFileCstr);

						throwV8Exception("Unable to open the application dependency manifest file at %s", manifestFileCstr.data());
						return E_FAIL;
					}

					const web::json::value manifestRoot = web::json::value::parse(dependencyManifestStream);
					const web::json::object& rootJson = manifestRoot.as_object();
					const web::json::object& libraries = rootJson.at(_X("libraries")).as_object();

					for (const auto& library : libraries)
					{
						size_t pos = library.first.find(_X("/"));
						pal::string_t libraryName = library.first.substr(0, pos);
						pal::string_t libraryVersion = library.first.substr(pos + 1);

						if (libraryName == _X("Microsoft.NETCore.App"))
						{
							trace::info(_X("CoreClrEmbedding::Initialize - Found a framework version (%s) in the application dependency manifest, trying to load it"), libraryVersion.c_str());

							append_path(&coreClrDirectory, libraryVersion.c_str());
							LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);

							if (!libCoreClr)
							{
								std::vector<char> coreClrVersionCstr;
								pal::pal_clrstring(libraryVersion, &coreClrVersionCstr);

								throwV8Exception("The targeted CLR version (%s) specified in application dependency manifest was not found.", coreClrVersionCstr.data());
								return E_FAIL;
							}

							break;
						}
					}
				}

				if (!libCoreClr)
				{
					trace::info(_X("CoreClrEmbedding::Initialize - No CORECLR_VERSION environment variable provided and no framework version provided in the application dependency manifest, getting the max installed framework version"));

					std::vector<pal::string_t> frameworkVersions;
					pal::readdir(coreClrDirectory, &frameworkVersions);
					fx_ver_t maxVersion(-1, -1, -1);
					fx_ver_t currentVersion(-1, -1, -1);

					for (u_int versionIndex = 0; versionIndex < frameworkVersions.size(); versionIndex++)
					{
						if (fx_ver_t::parse(frameworkVersions[versionIndex], &currentVersion))
						{
							if (currentVersion > maxVersion)
							{
								maxVersion = currentVersion;
							}
						}
					}

					if (maxVersion.get_major() > 0)
					{
						trace::info(_X("CoreClrEmbedding::Initialize - Max framework version was %s"), maxVersion.as_str().c_str());

						append_path(&coreClrDirectory, maxVersion.as_str().c_str());
						LoadCoreClrAtPath(coreClrDirectory, &libCoreClr);
					}

					else
					{
						trace::info(_X("CoreClrEmbedding::Initialize - None of the subdirectories of %s were valid framework versions"), coreClrDirectory.c_str());
					}
				}

				if (libCoreClr)
				{
					frameworkDependencyManifestFile = pal::string_t(coreClrDirectory);
					append_path(&frameworkDependencyManifestFile, _X("Microsoft.NETCore.App.deps.json"));

					if (!pal::file_exists(frameworkDependencyManifestFile))
					{
						frameworkDependencyManifestFile = dependencyManifestFile.length() > 0 ? pal::string_t(dependencyManifestFile) : _X("");
					}
				}
			}

    		if (!libCoreClr)
    		{
				previousIndex = currentIndex == std::string::npos ? currentIndex : currentIndex + 1;

				if (previousIndex != std::string::npos)
				{
					currentIndex = pathEnvironmentVariable.find(PATH_SEPARATOR, previousIndex);
				}
    		}
    	}
    }

    if (!libCoreClr)
    {
		throwV8Exception("Failed to find CoreCLR.  Make sure that you have either specified the CoreCLR directory in the CORECLR_DIR environment variable or the dotnet executable exists somewhere in your PATH environment variable and you have specified a CORECLR_VERSION environment variable.");
        return E_FAIL;
    }

    trace::info(_X("CoreClrEmbedding::Initialize - %s loaded successfully from %s"), LIBCORECLR_NAME, coreClrDirectory.c_str());
	trace::info(_X("CoreClrEmbedding::Initialize - Getting bootstrap assemblies list"));

	pal::string_t pathSeparator(1, PATH_SEPARATOR);
	std::map<pal::string_t, std::pair<pal::string_t, pal::string_t>> bootstrapAssembliesMap;
	pal::string_t bootstrapAssemblies;
	pal::string_t edgeJsAssemblyPath;

	if (dependencyManifestFile.length() > 0)
	{
		trace::info(_X("CoreClrEmbedding::Initialize - Checking application dependency manifest file at %s for bootstrap assemblies"), dependencyManifestFile.c_str());

		deps_json_t dependencyManifest(true, dependencyManifestFile, GetOSName() + GetOSVersion() + _X("-") + GetOSArchitecture());
		std::vector<deps_entry_t> applicationDependencies = dependencyManifest.get_entries(deps_entry_t::asset_types::runtime);

		for (const deps_entry_t& applicationDependency : applicationDependencies)
		{
			trace::info(_X("CoreClrEmbedding::Initialize - Added version %s of %s to the bootstrap assemblies list"), applicationDependency.library_version.c_str(), bootstrapAssembliesMap[applicationDependency.library_name].first.c_str(), applicationDependency.relative_path.c_str());
			bootstrapAssembliesMap[applicationDependency.library_name] = std::pair<pal::string_t, pal::string_t>(applicationDependency.library_version, applicationDependency.relative_path);

			if (applicationDependency.library_name == _X("Edge.js"))
			{
				pal::string_t packagesEnvironmentVariable;
				pal::getenv(_X("NUGET_PACKAGES"), &packagesEnvironmentVariable);

				if (packagesEnvironmentVariable.length() == 0)
				{
					pal::string_t profileDirectory;
					pal::getenv(_X("USERPROFILE"), &profileDirectory);

					if (profileDirectory.length() == 0)
					{
						pal::getenv(_X("HOME"), &profileDirectory);
					}

					edgeJsAssemblyPath = profileDirectory;

					append_path(&edgeJsAssemblyPath, _X(".nuget"));
					append_path(&edgeJsAssemblyPath, _X("packages"));
				}

				else
				{
					edgeJsAssemblyPath = packagesEnvironmentVariable;
				}

				append_path(&edgeJsAssemblyPath, applicationDependency.library_name.c_str());
				append_path(&edgeJsAssemblyPath, applicationDependency.library_version.c_str());

				pal::string_t relativePath(applicationDependency.relative_path);
				size_t replacePosition = relativePath.find_first_of(_X('/'));

				while (replacePosition != pal::string_t::npos)
				{
					relativePath[replacePosition] = DIR_SEPARATOR;
					replacePosition = relativePath.find_first_of(_X('/'), replacePosition + 1);
				}

				append_path(&edgeJsAssemblyPath, relativePath.c_str());
				edgeJsAssemblyPath = get_directory(edgeJsAssemblyPath);
			}
		}
	}

	if (edgeJsAssemblyPath.length() == 0)
	{
		throwV8Exception("Failed to find the Edge.js runtime assembly in the dependency manifest list.  Make sure that your project.json file has a reference to the Edge.js NuGet package.");
		return E_FAIL;
	}

	for (std::map<pal::string_t, std::pair<pal::string_t, pal::string_t>>::iterator iter = bootstrapAssembliesMap.begin(); iter != bootstrapAssembliesMap.end(); ++iter)
	{
		pal::string_t currentPackageName = iter->first;
		std::pair<pal::string_t, pal::string_t> versionAssemblyPair = iter->second;

		if (bootstrapAssemblies.length() > 0)
		{
			bootstrapAssemblies.append(_X(";"));
		}

		bootstrapAssemblies.append(currentPackageName);
		bootstrapAssemblies.append(_X("/"));
		bootstrapAssemblies.append(versionAssemblyPair.first);
		bootstrapAssemblies.append(_X(":"));
		bootstrapAssemblies.append(versionAssemblyPair.second);
	}

    pal::string_t assemblySearchDirectories;

    assemblySearchDirectories.append(currentDirectory);
    assemblySearchDirectories.append(pathSeparator);
    assemblySearchDirectories.append(coreClrDirectory);
	assemblySearchDirectories.append(pathSeparator);
	assemblySearchDirectories.append(edgeNodePath);
	assemblySearchDirectories.append(pathSeparator);
	assemblySearchDirectories.append(edgeJsAssemblyPath);

    trace::info(_X("CoreClrEmbedding::Initialize - Assembly search path is %s"), assemblySearchDirectories.c_str());

    coreclr_initialize initializeCoreCLR = (coreclr_initialize) pal::get_symbol(libCoreClr, "coreclr_initialize");

    if (!initializeCoreCLR)
    {
    	throwV8Exception("Error loading the coreclr_initialize function from %s: %s.", LIBCORECLR_NAME, GetLoadError());
        return E_FAIL;
    }

    trace::info(_X("CoreClrEmbedding::Initialize - coreclr_initialize loaded successfully"));
    coreclr_create_delegate createDelegate = (coreclr_create_delegate) pal::get_symbol(libCoreClr, "coreclr_create_delegate");

    if (!createDelegate)
    {
    	throwV8Exception("Error loading the coreclr_create_delegate function from %s: %s.", LIBCORECLR_NAME, GetLoadError());
    	return E_FAIL;
    }

    trace::info(_X("CoreClrEmbedding::Initialize - coreclr_create_delegate loaded successfully"));

    const char* propertyKeys[] = {
    	"TRUSTED_PLATFORM_ASSEMBLIES",
    	"APP_PATHS",
    	"APP_NI_PATHS",
    	"NATIVE_DLL_SEARCH_DIRECTORIES",
    	"AppDomainCompatSwitch",
		"APP_CONTEXT_BASE_DIRECTORY",
		"APP_CONTEXT_DEPS_FILES",
		"FX_DEPS_FILE"
    };

    pal::string_t tpaList;
    AddToTpaList(frameworkDependencyManifestFile.length() > 0 ? frameworkDependencyManifestFile : dependencyManifestFile, coreClrDirectory, &tpaList);

	pal::string_t appBase = edgeJsAssemblyPath;
    trace::info(_X("CoreClrEmbedding::Initialize - Using %s as the app path value"), appBase.c_str());

	pal::string_t appContextDepsFiles(dependencyManifestFile);

	if (dependencyManifestFile != frameworkDependencyManifestFile)
	{
		appContextDepsFiles += _X(";") + frameworkDependencyManifestFile;
	}

	std::vector<char> tpaListCstr, appBaseCstr, assemblySearchDirectoriesCstr, bootstrapperCstr, dependencyManifestFileCstr, frameworkDependencyManifestFileCstr, appContextDepsFilesCstr;
	
	pal::pal_clrstring(tpaList, &tpaListCstr);
	pal::pal_clrstring(appBase, &appBaseCstr);
	pal::pal_clrstring(assemblySearchDirectories, &assemblySearchDirectoriesCstr);
	pal::pal_clrstring(bootstrapper, &bootstrapperCstr);
	pal::pal_clrstring(dependencyManifestFile, &dependencyManifestFileCstr);
	pal::pal_clrstring(frameworkDependencyManifestFile, &frameworkDependencyManifestFileCstr);
	pal::pal_clrstring(appContextDepsFiles, &appContextDepsFilesCstr);

    const char* propertyValues[] = {
    	tpaListCstr.data(),
        appBaseCstr.data(),
		appBaseCstr.data(),
        assemblySearchDirectoriesCstr.data(),
        "UseLatestBehaviorWhenTFMNotSpecified",
		appBaseCstr.data(),
		appContextDepsFilesCstr.data(),
		frameworkDependencyManifestFileCstr.data()
    };

    trace::info(_X("CoreClrEmbedding::Initialize - Calling coreclr_initialize()"));
	result = initializeCoreCLR(
			bootstrapperCstr.data(),
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

	trace::info(_X("CoreClrEmbedding::Initialize - CoreCLR initialized successfully"));
    trace::info(_X("CoreClrEmbedding::Initialize - App domain created successfully (app domain ID: %d)"), appDomainId);

    SetCallV8FunctionDelegateFunction setCallV8Function;

    CREATE_DELEGATE("GetFunc", &getFunc);
    CREATE_DELEGATE("CallFunc", &callFunc);
    CREATE_DELEGATE("ContinueTask", &continueTask);
    CREATE_DELEGATE("FreeHandle", &freeHandle);
    CREATE_DELEGATE("FreeMarshalData", &freeMarshalData);
    CREATE_DELEGATE("SetCallV8FunctionDelegate", &setCallV8Function);
    CREATE_DELEGATE("CompileFunc", &compileFunc);
	CREATE_DELEGATE("Initialize", &initialize);

	trace::info(_X("CoreClrEmbedding::Initialize - Getting runtime info"));

	CoreClrGcHandle exception = NULL;
	BootstrapperContext context;

	std::vector<char> coreClrDirectoryCstr, currentDirectoryCstr, edgeAppDirCstr;
	pal::pal_clrstring(coreClrDirectory, &coreClrDirectoryCstr);
	pal::pal_clrstring(currentDirectory, &currentDirectoryCstr);
	pal::pal_clrstring(edgeAppDir, &edgeAppDirCstr);

	context.runtimeDirectory = coreClrDirectoryCstr.data();
	context.applicationDirectory = edgeAppDirCstr.data();
	context.edgeNodePath = edgeNodePathCstr.data();
	context.dependencyManifestFile = dependencyManifestFileCstr.data();

	if (!context.applicationDirectory)
	{
		context.applicationDirectory = currentDirectoryCstr.data();
	}
	
	trace::info(_X("CoreClrEmbedding::Initialize - Runtime directory: %s"), coreClrDirectory.c_str());
	trace::info(_X("CoreClrEmbedding::Initialize - Application directory: %s"), edgeAppDir.c_str());
	trace::info(_X("CoreClrEmbedding::Initialize - Bootstrapper assemblies: %s"), bootstrapAssemblies.c_str());

	std::vector<char> bootstrapAssembliesCstr;
	pal::pal_clrstring(bootstrapAssemblies, &bootstrapAssembliesCstr);

	context.bootstrapAssemblies = bootstrapAssembliesCstr.data();

	trace::info(_X("CoreClrEmbedding::Initialize - Calling CLR Initialize() function"));
	initialize(&context, &exception);

	if (exception)
	{
		v8::Local<v8::Value> v8Exception = CoreClrFunc::MarshalCLRToV8(exception, V8TypeException);
		FreeMarshalData(exception, V8TypeException);

		throwV8Exception(v8Exception);
		return E_FAIL;
	}

	else
	{
		trace::info(_X("CoreClrEmbedding::Initialize - CLR Initialize() function called successfully"));
	}

	exception = NULL;
	setCallV8Function(CoreClrNodejsFunc::Call, &exception);

	if (exception)
	{
		v8::Local<v8::Value> v8Exception = CoreClrFunc::MarshalCLRToV8(exception, V8TypeException);
		FreeMarshalData(exception, V8TypeException);

		throwV8Exception(v8Exception);
		return E_FAIL;
	}

	else
	{
		trace::info(_X("CoreClrEmbedding::Initialize - CallV8Function delegate set successfully"));
	}

	trace::info(_X("CoreClrEmbedding::Initialize - Completed"));

    return S_OK;
}

CoreClrGcHandle CoreClrEmbedding::GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName, v8::Local<v8::Value>* v8Exception)
{
	trace::info(_X("CoreClrEmbedding::GetClrFuncReflectionWrapFunc - Starting"));

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
		trace::info(_X("CoreClrEmbedding::GetClrFuncReflectionWrapFunc - Finished"));
		return function;
	}
}

void CoreClrEmbedding::AddToTpaList(pal::string_t dependencyManifestFile, pal::string_t frameworkDirectory, pal::string_t* tpaList)
{
	trace::info(_X("CoreClrEmbedding::AddToTpaList - Searching %s for serviceable assemblies to add to the TPA list"), dependencyManifestFile.c_str());

    std::vector<pal::string_t> files;
    std::set<pal::string_t> addedAssemblies;
	pal::string_t dirSeparator(1, DIR_SEPARATOR);
	pal::string_t pathSeparator(1, PATH_SEPARATOR);

	deps_json_t dependencyManifest(false, dependencyManifestFile);
	std::vector<deps_entry_t> dependencies = dependencyManifest.get_entries(deps_entry_t::asset_types::runtime);

	for (const deps_entry_t& dependency : dependencies)
	{
		if (!dependency.is_serviceable)
		{
			continue;
		}

		pal::string_t filename = pal::string_t(dependency.relative_path);		
		size_t separatorPosition = filename.find_last_of(_X("/\\"));

		if (separatorPosition != pal::string_t::npos)
		{
			filename = filename.substr(separatorPosition + 1);
		}

		tpaList->append(frameworkDirectory);
		tpaList->append(dirSeparator);
		tpaList->append(filename);
		tpaList->append(pathSeparator);

		trace::info(_X("CoreClrEmbedding::AddToTpaList - Added %s%s%s to the TPA list"), frameworkDirectory.c_str(), dirSeparator.c_str(), filename.c_str());
	}
}

void CoreClrEmbedding::FreeCoreClr(pal::dll_t libCoreClr)
{
    if (libCoreClr)
    {
		pal::unload_library(libCoreClr);
        libCoreClr = NULL;
    }
}

bool CoreClrEmbedding::LoadCoreClrAtPath(pal::string_t loadPath, pal::dll_t* libCoreClrPointer)
{
    trace::info(_X("CoreClrEmbedding::LoadCoreClrAtPath - Trying to load %s from %s"), LIBCORECLR_NAME, loadPath.c_str());

	pal::string_t coreClrFilePath = pal::string_t(loadPath);
	append_path(&coreClrFilePath, LIBCORECLR_NAME);

	if (coreclr_exists_in_dir(loadPath) && pal::load_library(coreClrFilePath.c_str(), libCoreClrPointer))
	{
    	trace::info(_X("CoreClrEmbedding::LoadCoreClrAtPath - Load of %s succeeded"), LIBCORECLR_NAME);
		return true;
    }

    return false;
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

void CoreClrEmbedding::CallClrFunc(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType)
{
	trace::info(_X("CoreClrEmbedding::CallClrFunc"));
	callFunc(functionHandle, payload, payloadType, taskState, result, resultType);
}

void CoreClrEmbedding::ContinueTask(CoreClrGcHandle taskHandle, void* context, TaskCompleteFunction callback, void** exception)
{
	trace::info(_X("CoreClrEmbedding::ContinueTask"));
	continueTask(taskHandle, context, callback, exception);
}

void CoreClrEmbedding::FreeHandle(CoreClrGcHandle handle)
{
	trace::info(_X("CoreClrEmbedding::FreeHandle"));
	freeHandle(handle);
}

void CoreClrEmbedding::FreeMarshalData(void* marshalData, int marshalDataType)
{
	trace::info(_X("CoreClrEmbedding::FreeMarshalData"));
	freeMarshalData(marshalData, marshalDataType);
}

CoreClrGcHandle CoreClrEmbedding::CompileFunc(const void* options, const int payloadType, v8::Local<v8::Value>* v8Exception)
{
    trace::info(_X("CoreClrEmbedding::CompileFunc - Starting"));

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
        trace::info(_X("CoreClrEmbedding::CompileFunc - Finished"));
        return function;
    }
}
