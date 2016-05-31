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
			"CoreCLREmbedding",\
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

	pal::string_t edgeAppDir;
	pal::getenv(_X("EDGE_APP_ROOT"), &edgeAppDir);

	edgeAppDir = edgeAppDir.length() > 0 ? edgeAppDir : currentDirectory;

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

		pal::string_t dotnetExecutablePath;

    	size_t previousIndex = 0;
    	size_t currentIndex = pathEnvironmentVariable.find(PATH_SEPARATOR);

    	while (!libCoreClr && currentIndex != std::string::npos)
    	{
			dotnetExecutablePath = pathEnvironmentVariable.substr(previousIndex, currentIndex - previousIndex);

			append_path(&dotnetExecutablePath, _X("dotnet"));
			dotnetExecutablePath.append(pal::exe_suffix());

			if (pal::file_exists(dotnetExecutablePath))
			{
				coreClrDirectory = pathEnvironmentVariable.substr(previousIndex, currentIndex - previousIndex);
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
				previousIndex = currentIndex + 1;
				currentIndex = pathEnvironmentVariable.find(PATH_SEPARATOR, previousIndex);
    		}
    	}
    }

    if (!libCoreClr)
    {
		throwV8Exception("Failed to find CoreCLR.  Make sure that you have either specified the CoreCLR directory in the CORECLR_DIR environment variable or the dotnet executable exists somewhere in your PATH environment variable and you have specified a CORECLR_VERSION environment variable.");
        return E_FAIL;
    }

    trace::info(_X("CoreClrEmbedding::Initialize - %s loaded successfully from %s"), LIBCORECLR_NAME, coreClrDirectory.c_str());

    pal::string_t assemblySearchDirectories;

    assemblySearchDirectories.append(currentDirectory);
    assemblySearchDirectories.append(_X(":"));
    assemblySearchDirectories.append(coreClrDirectory);
	assemblySearchDirectories.append(_X(":"));
	assemblySearchDirectories.append(edgeNodePath);

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

	// TODO: may want to consider looking at .deps.json for TPA instead of loading everything in the runtime directory
    AddToTpaList(coreClrDirectory, &tpaList);

	pal::string_t appBase = edgeNodePath;
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

	std::vector<char> coreClrDirectoryCstr, currentDirectoryCstr;
	pal::pal_clrstring(coreClrDirectory, &coreClrDirectoryCstr);
	pal::pal_clrstring(currentDirectory, &currentDirectoryCstr);

	context.runtimeDirectory = coreClrDirectoryCstr.data();
	context.applicationDirectory = getenv("EDGE_APP_ROOT");
	context.edgeNodePath = edgeNodePathCstr.data();
	context.dependencyManifestFile = dependencyManifestFileCstr.data();

	if (!context.applicationDirectory)
	{
		context.applicationDirectory = currentDirectoryCstr.data();
	}
	
	DBG("CoreClrEmbedding::Initialize - Runtime directory: %s", context.runtimeDirectory);
	DBG("CoreClrEmbedding::Initialize - Application directory: %s", context.applicationDirectory);

	trace::info(_X("CoreClrEmbedding::Initialize - Getting bootstrap assemblies list"));
	
	pal::string_t edgeJsProjectJsonFile(edgeNodePath);
	append_path(&edgeJsProjectJsonFile, _X("project.lock.json"));

	if (!pal::file_exists(edgeJsProjectJsonFile))
	{
		throwV8Exception("Unable to locate the project.lock.json file for Edge.js under %s.", edgeNodePathCstr);
		return E_FAIL;
	}

	trace::info(_X("CoreClrEmbedding::Initialize - Opening a stream for Edge.js package dependencies file at %s"), edgeJsProjectJsonFile.c_str());
	pal::ifstream_t edgeJsProjectJsonStream(edgeJsProjectJsonFile);

	if (!edgeJsProjectJsonStream.good())
	{
		std::vector<char> edgeJsProjectJsonFileCstr;
		pal::pal_clrstring(edgeJsProjectJsonFile, &edgeJsProjectJsonFileCstr);

		throwV8Exception("Unable to open the project.lock.json file for Edge.js at %s", edgeJsProjectJsonFileCstr.data());
		return E_FAIL;
	}

	const web::json::value root = web::json::value::parse(edgeJsProjectJsonStream);
	const web::json::object& json = root.as_object();
	const web::json::object& dependencies = json.at(_X("targets")).as_object().at(_X(".NETStandard,Version=v1.5")).as_object();

	std::map<pal::string_t, std::pair<pal::string_t, pal::string_t>> bootstrapAssembliesMap;
	pal::string_t bootstrapAssemblies;

	for (const auto& dependency : dependencies)
	{
		if (dependency.second.has_field(_X("runtime")))
		{
			pal::string_t runtimeAssembly = dependency.second.at(_X("runtime")).as_object().begin()->first;

			if (!ends_with(runtimeAssembly, _X("/_._"), false))
			{
				pal::stringstream_t packageNameVersionStream(dependency.first);
				pal::string_t packageName;
				pal::string_t packageVersion;

				std::getline(packageNameVersionStream, packageName, _X('/'));
				std::getline(packageNameVersionStream, packageVersion, _X('/'));

				trace::info(_X("CoreClrEmbedding::Initialize - Found runtime dependency assembly for %s (%s) at %s"), packageName.c_str(), packageVersion.c_str(), runtimeAssembly.c_str());
				bootstrapAssembliesMap[packageName] = std::pair<pal::string_t, pal::string_t>(packageVersion, runtimeAssembly);
			}
		}
	}

	// TODO: update deps_format.cpp to get TARGET_RUNTIME_ID from coreclr.dll somehow
	if (dependencyManifestFile.length() > 0)
	{
		trace::info(_X("CoreClrEmbedding::Initialize - Checking application dependency manifest file for overrides to the bootstrap packages"));

		deps_json_t dependencyManifest(true, dependencyManifestFile);
		std::vector<deps_entry_t> applicationDependencies = dependencyManifest.get_entries(deps_entry_t::asset_types::runtime);

		for (const deps_entry_t& applicationDependency : applicationDependencies)
		{
			trace::info(_X("CoreClrEmbedding::Initialize - Checking %s"), applicationDependency.library_name.c_str());

			if (bootstrapAssembliesMap.find(applicationDependency.library_name) != bootstrapAssembliesMap.end())
			{
				fx_ver_t applicationDependencyVersion(-1, -1, -1);
				fx_ver_t bootstrapVersion(-1, -1, -1);
				fx_ver_t::parse(applicationDependency.library_version, &applicationDependencyVersion);
				fx_ver_t::parse(bootstrapAssembliesMap[applicationDependency.library_name].first, &bootstrapVersion);

				if (applicationDependencyVersion > bootstrapVersion)
				{
					trace::info(_X("CoreClrEmbedding::Initialize - Application dependency manifest version (%s) is greater than the Edge.js dependency manifest version (%s), new path for the dependency is %s"), applicationDependency.library_version.c_str(), bootstrapAssembliesMap[applicationDependency.library_name].first.c_str(), applicationDependency.relative_path.c_str());
					bootstrapAssembliesMap[applicationDependency.library_name] = std::pair<pal::string_t, pal::string_t>(applicationDependency.library_version, applicationDependency.relative_path);
				}

				else
				{
					trace::info(_X("CoreClrEmbedding::Initialize - Application dependency manifest version (%s) is less than or equal to the Edge.js dependency manifest version (%s), skipping"), applicationDependency.library_version.c_str(), bootstrapAssembliesMap[applicationDependency.library_name].first.c_str());
				}
			}

			else
			{
				trace::info(_X("CoreClrEmbedding::Initialize - Not present in the bootstrap assemblies list, skipping"));
			}
		}
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

void CoreClrEmbedding::AddToTpaList(pal::string_t directoryPath, pal::string_t* tpaList)
{
	trace::info(_X("CoreClrEmbedding::AddToTpaList - Searching %s for assemblies to add to the TPA list"), directoryPath.c_str());

    const pal::char_t * const tpaExtensions[] = {
        _X("*.ni.dll"),
        _X("*.dll"),
        _X("*.ni.exe"),
        _X("*.exe")
    };

	std::vector<pal::string_t> files;
    std::set<pal::string_t> addedAssemblies;
	pal::string_t dirSeparator(1, DIR_SEPARATOR);
	pal::string_t pathSeparator(1, PATH_SEPARATOR);

    // Walk the directory for each extension separately so that we first get files with .ni.dll extension, then files with .dll 
	// extension, etc.
    for (int extensionIndex = 0; extensionIndex < sizeof(tpaExtensions) / sizeof(tpaExtensions[0]); extensionIndex++)
    {
		pal::readdir(directoryPath, tpaExtensions[extensionIndex], &files);

		for (u_int fileIndex = 0; fileIndex < files.size(); fileIndex++)
		{
			pal::string_t filename = get_filename(files[fileIndex]);
			pal::string_t filenameWithoutExtension = get_filename_without_ext(files[fileIndex]);

			// Make sure if we have an assembly with multiple extensions present, we insert only one version of it.
			if (addedAssemblies.find(filenameWithoutExtension) == addedAssemblies.end())
			{
				addedAssemblies.insert(filenameWithoutExtension);

				tpaList->append(directoryPath);
				tpaList->append(dirSeparator);
				tpaList->append(files[fileIndex]);
				tpaList->append(pathSeparator);

				trace::info(_X("CoreClrEmbedding::AddToTpaList - Added %s to the TPA list"), filename.c_str());
			}
		}
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
