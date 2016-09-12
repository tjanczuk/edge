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
#include "deps/deps_resolver.h"
#include "fxr/fx_muxer.h"
#include "host/coreclr.h"
#include "host/error_codes.h"

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

GetFuncFunction getFunc;
CallFuncFunction callFunc;
ContinueTaskFunction continueTask;
FreeHandleFunction freeHandle;
FreeMarshalDataFunction freeMarshalData;
CompileFuncFunction compileFunc;
InitializeFunction initialize;

#define CREATE_DELEGATE(functionName, functionPointer)\
	result = coreclr::create_delegate(\
			host_handle,\
			domain_id,\
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

	set_own_rid(GetOSName() + GetOSVersion() + _X("-") + GetOSArchitecture());

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

	pal::string_t entryPointAssembly(dependencyManifestFile);
	entryPointAssembly = strip_file_ext(strip_file_ext(entryPointAssembly));
	entryPointAssembly.append(_X(".dll"));

	pal::string_t pathEnvironmentVariable;
	pal::getenv(_X("PATH"), &pathEnvironmentVariable);

	trace::info(_X("CoreClrEmbedding::Initialize - Path is %s"), pathEnvironmentVariable.c_str());

	pal::string_t dotnetExecutablePath, dotnetDirectory;

	size_t previousIndex = 0;
	size_t currentIndex = pathEnvironmentVariable.find(PATH_SEPARATOR);

	while (dotnetExecutablePath.length() == 0 && previousIndex != std::string::npos)
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
			dotnetDirectory = get_directory(dotnetExecutablePath);
			trace::info(_X("CoreClrEmbedding::Initialize - Found dotnet at %s"), dotnetExecutablePath.c_str());

			break;
		}

		else
		{
			dotnetExecutablePath = _X("");
			previousIndex = currentIndex == std::string::npos ? currentIndex : currentIndex + 1;

			if (previousIndex != std::string::npos)
			{
				currentIndex = pathEnvironmentVariable.find(PATH_SEPARATOR, previousIndex);
			}
		}
	}

	host_mode_t mode = coreclr_exists_in_dir(edgeAppDir) ? host_mode_t::standalone : host_mode_t::muxer;

	if (mode == host_mode_t::standalone && dotnetExecutablePath.empty())
	{
		throwV8Exception("This is not a published, standalone application and we are unable to locate the .NET Core SDK.  Please make sure that it is installed; see http://microsoft.com/net/core for more details.");
	}

	pal::string_t configFile, devConfigFile, sdkPath;

	if (mode != host_mode_t::standalone)
	{
		pal::string_t sdkDirectory;

		fx_muxer_t::resolve_sdk_dotnet_path(dotnetDirectory, &sdkDirectory);
		
		pal::string_t dotnetAssemblyPath(sdkDirectory);
		append_path(&dotnetAssemblyPath, _X("dotnet.dll"));

		get_runtime_config_paths_from_app(dotnetAssemblyPath, &configFile, &devConfigFile);
	}

	else
	{
		get_runtime_config_paths_from_app(entryPointAssembly, &configFile, &devConfigFile);
	}

	pal::string_t packagesPath;
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

		packagesPath = profileDirectory;

		append_path(&packagesPath, _X(".nuget"));
		append_path(&packagesPath, _X("packages"));
	}

	else
	{
		packagesPath = packagesEnvironmentVariable;
	}

	runtime_config_t config(configFile, devConfigFile);
	std::vector<pal::string_t> probePaths;

	for (pal::string_t path : config.get_probe_paths())
	{
		if (!path.empty())
		{
			pal::string_t modifiedPath(path);
			pal::realpath(&modifiedPath);
			probePaths.push_back(modifiedPath);
		}
	}

	pal::string_t coreClrVersion;
	pal::getenv(_X("CORECLR_VERSION"), &coreClrVersion);

	pal::string_t frameworkDirectory = config.get_portable() ? fx_muxer_t::resolve_fx_dir(mode, dotnetDirectory, config, coreClrVersion) : _X("");
	corehost_init_t init(dependencyManifestFile, probePaths, frameworkDirectory, mode, config);
	host_interface_t hostInterface = init.get_host_init_data();
	hostpolicy_init_t g_init;
	arguments_t args;

	args.probe_paths = probePaths;
	args.app_dir = edgeAppDir;
	args.app_argc = 0;
	args.app_argv = NULL;
	args.deps_path = dependencyManifestFile;
	args.own_path = dotnetExecutablePath;
	args.dotnet_packages_cache = packagesPath;
	args.managed_application = entryPointAssembly;

	hostpolicy_init_t::init(&hostInterface, &g_init);

	// Below copied from https://github.com/dotnet/core-setup/blob/master/src/corehost/cli/hostpolicy.cpp#L18
	// *********************************************************************************************************************

	// Load the deps resolver
	deps_resolver_t resolver(g_init, args);

	pal::string_t resolver_errors;
	if (!resolver.valid(&resolver_errors))
	{
		trace::error(_X("CoreClrEmbedding::Initialize - Error initializing the dependency resolver: %s"), resolver_errors.c_str());
		return StatusCode::ResolverInitFailure;
	}

	// Setup breadcrumbs
	//pal::string_t policy_name = _STRINGIFY(HOST_POLICY_PKG_NAME);
	//pal::string_t policy_version = _STRINGIFY(HOST_POLICY_PKG_VER);

	// Always insert the hostpolicy that the code is running on.
	std::unordered_set<pal::string_t> breadcrumbs;
	//breadcrumbs.insert(policy_name);
	//breadcrumbs.insert(policy_name + _X(",") + policy_version);

	probe_paths_t probe_paths;
	if (!resolver.resolve_probe_paths(&probe_paths, &breadcrumbs))
	{
		return StatusCode::ResolverResolveFailure;
	}

	pal::string_t clr_path = probe_paths.coreclr;
	if (clr_path.empty() || !pal::realpath(&clr_path))
	{
		trace::error(_X("CoreClrEmbedding::Initialize - Could not resolve CoreCLR path. For more details, enable tracing by setting COREHOST_TRACE environment variable to 1"));;
		return StatusCode::CoreClrResolveFailure;
	}

	pal::string_t clrjit_path = probe_paths.clrjit;
	if (clrjit_path.empty())
	{
		trace::warning(_X("CoreClrEmbedding::Initialize - Could not resolve CLRJit path"));
	}
	else if (pal::realpath(&clrjit_path))
	{
		trace::verbose(_X("CoreClrEmbedding::Initialize - The resolved JIT path is '%s'"), clrjit_path.c_str());
	}
	else
	{
		clrjit_path.clear();
		trace::warning(_X("CoreClrEmbedding::Initialize - Could not resolve symlink to CLRJit path '%s'"), probe_paths.clrjit.c_str());
	}

	// Build CoreCLR properties
	std::vector<const char*> property_keys = {
		"TRUSTED_PLATFORM_ASSEMBLIES",
		"APP_PATHS",
		"APP_NI_PATHS",
		"NATIVE_DLL_SEARCH_DIRECTORIES",
		"PLATFORM_RESOURCE_ROOTS",
		"AppDomainCompatSwitch",
		// Workaround: mscorlib does not resolve symlinks for AppContext.BaseDirectory dotnet/coreclr/issues/2128
		"APP_CONTEXT_BASE_DIRECTORY",
		"APP_CONTEXT_DEPS_FILES",
		"FX_DEPS_FILE"
	};

	// Note: these variables' lifetime should be longer than coreclr_initialize.
	std::vector<char> tpa_paths_cstr, app_base_cstr, native_dirs_cstr, resources_dirs_cstr, fx_deps, deps, clrjit_path_cstr;
	pal::pal_clrstring(probe_paths.tpa, &tpa_paths_cstr);
	pal::pal_clrstring(args.app_dir, &app_base_cstr);
	pal::pal_clrstring(probe_paths.native, &native_dirs_cstr);
	pal::pal_clrstring(probe_paths.resources, &resources_dirs_cstr);

	pal::pal_clrstring(resolver.get_fx_deps_file(), &fx_deps);
	pal::pal_clrstring(resolver.get_deps_file() + _X(";") + resolver.get_fx_deps_file(), &deps);

	std::vector<const char*> property_values = {
		// TRUSTED_PLATFORM_ASSEMBLIES
		tpa_paths_cstr.data(),
		// APP_PATHS
		app_base_cstr.data(),
		// APP_NI_PATHS
		app_base_cstr.data(),
		// NATIVE_DLL_SEARCH_DIRECTORIES
		native_dirs_cstr.data(),
		// PLATFORM_RESOURCE_ROOTS
		resources_dirs_cstr.data(),
		// AppDomainCompatSwitch
		"UseLatestBehaviorWhenTFMNotSpecified",
		// APP_CONTEXT_BASE_DIRECTORY
		app_base_cstr.data(),
		// APP_CONTEXT_DEPS_FILES,
		deps.data(),
		// FX_DEPS_FILE
		fx_deps.data()
	};

	if (!clrjit_path.empty())
	{
		pal::pal_clrstring(clrjit_path, &clrjit_path_cstr);
		property_keys.push_back("JIT_PATH");
		property_values.push_back(clrjit_path_cstr.data());
	}

	for (int i = 0; i < g_init.cfg_keys.size(); ++i)
	{
		property_keys.push_back(g_init.cfg_keys[i].data());
		property_values.push_back(g_init.cfg_values[i].data());
	}

	size_t property_size = property_keys.size();
	assert(property_keys.size() == property_values.size());

	// Add API sets to the process DLL search
	pal::setup_api_sets(resolver.get_api_sets());

	// Bind CoreCLR
	pal::string_t clr_dir = get_directory(clr_path);
	trace::verbose(_X("CoreClrEmbedding::Initialize - CoreCLR path = '%s', CoreCLR dir = '%s'"), clr_path.c_str(), clr_dir.c_str());
	if (!coreclr::bind(clr_dir))
	{
		trace::error(_X("CoreClrEmbedding::Initialize - Failed to bind to CoreCLR at '%s'"), clr_path.c_str());
		return StatusCode::CoreClrBindFailure;
	}

	// Verbose logging
	if (trace::is_enabled())
	{
		for (size_t i = 0; i < property_size; ++i)
		{
			pal::string_t key, val;
			pal::clr_palstring(property_keys[i], &key);
			pal::clr_palstring(property_values[i], &val);
			trace::verbose(_X("CoreClrEmbedding::Initialize - Property %s = %s"), key.c_str(), val.c_str());
		}
	}

	std::vector<char> own_path, bootstrapperCstr;
	pal::pal_clrstring(args.own_path, &own_path);
	pal::pal_clrstring(bootstrapper, &bootstrapperCstr);

	// Initialize CoreCLR
	coreclr::host_handle_t host_handle;
	coreclr::domain_id_t domain_id;
	auto hr = coreclr::initialize(
		bootstrapperCstr.data(),
		"Edge",
		property_keys.data(),
		property_values.data(),
		property_size,
		&host_handle,
		&domain_id);
	if (!SUCCEEDED(hr))
	{
		trace::error(_X("CoreClrEmbedding::Initialize - Failed to initialize CoreCLR, HRESULT: 0x%X"), hr);
		return StatusCode::CoreClrInitFailure;
	}

	// *********************************************************************************************************************
	// End of copying from hostpolicy.cpp

	trace::info(_X("CoreClrEmbedding::Initialize - CoreCLR initialized successfully"));
	trace::info(_X("CoreClrEmbedding::Initialize - App domain created successfully (app domain ID: %d)"), domain_id);

	bool foundEdgeJs = false;

	for (deps_entry_t entry : resolver.m_deps->get_entries(deps_entry_t::asset_types::runtime))
	{
		if (entry.library_name == _X("Edge.js"))
		{
			foundEdgeJs = true;
			break;
		}
	}

	if (!foundEdgeJs)
	{
		throwV8Exception("Failed to find the Edge.js runtime assembly in the dependency manifest list.  Make sure that your project.json file has a reference to the Edge.js NuGet package.");
		return E_FAIL;
	}

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

	std::vector<char> coreClrDirectoryCstr, currentDirectoryCstr, edgeAppDirCstr, dependencyManifestFileCstr;
	pal::pal_clrstring(clr_path, &coreClrDirectoryCstr);
	pal::pal_clrstring(currentDirectory, &currentDirectoryCstr);
	pal::pal_clrstring(edgeAppDir, &edgeAppDirCstr);
	pal::pal_clrstring(dependencyManifestFile, &dependencyManifestFileCstr);

	context.runtimeDirectory = coreClrDirectoryCstr.data();
	context.applicationDirectory = edgeAppDirCstr.data();
	context.dependencyManifestFile = dependencyManifestFileCstr.data();

	if (!context.applicationDirectory)
	{
		context.applicationDirectory = currentDirectoryCstr.data();
	}
	
	trace::info(_X("CoreClrEmbedding::Initialize - Runtime directory: %s"), coreClrDirectory.c_str());
	trace::info(_X("CoreClrEmbedding::Initialize - Application directory: %s"), edgeAppDir.c_str());

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
