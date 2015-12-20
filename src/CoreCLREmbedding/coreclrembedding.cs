using System;
using System.Linq;
using System.Security;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;
using System.Linq.Expressions;
using System.Dynamic;
using System.Collections.Generic;
using System.Collections;
using System.Threading.Tasks;
using System.IO;
using System.Diagnostics;
using System.Runtime.Versioning;
using Microsoft.Dnx.Compilation;
using Microsoft.Dnx.Compilation.Caching;
using Microsoft.Dnx.Runtime;
using Microsoft.Dnx.Runtime.Loader;
using Microsoft.Extensions.PlatformAbstractions;

[StructLayout(LayoutKind.Sequential)]
// ReSharper disable once CheckNamespace
public struct V8ObjectData
{
    public int propertiesCount;
    public IntPtr propertyTypes;
    public IntPtr propertyNames;
    public IntPtr propertyValues;
}

[StructLayout(LayoutKind.Sequential)]
public struct V8ArrayData
{
    public int arrayLength;
    public IntPtr itemTypes;
    public IntPtr itemValues;
}

[StructLayout(LayoutKind.Sequential)]
public struct V8BufferData
{
    public int bufferLength;
    public IntPtr buffer;
}

public enum V8Type
{
    Function = 1,
    Buffer = 2,
    Array = 3,
    Date = 4,
    Object = 5,
    String = 6,
    Boolean = 7,
    Int32 = 8,
    UInt32 = 9,
    Number = 10,
    Null = 11,
    Task = 12,
    Exception = 13
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
public struct EdgeBootstrapperContext
{
    [MarshalAs(UnmanagedType.LPStr)]
    public string OperatingSystem;

    [MarshalAs(UnmanagedType.LPStr)]
    public string OperatingSystemVersion;

    [MarshalAs(UnmanagedType.LPStr)]
    public string Architecture;

    [MarshalAs(UnmanagedType.LPStr)]
    public string RuntimeDirectory;

    [MarshalAs(UnmanagedType.LPStr)]
    public string ApplicationDirectory;

    [MarshalAs(UnmanagedType.LPStr)]
    public string EdgeNodePath;
}

public delegate void CallV8FunctionDelegate(IntPtr payload, int payloadType, IntPtr v8FunctionContext, IntPtr callbackContext, IntPtr callbackDelegate);
public delegate void TaskCompleteDelegate(IntPtr result, int resultType, int taskState, IntPtr context);

[SecurityCritical]
public class CoreCLREmbedding
{
    private class TaskState
    {
        public readonly TaskCompleteDelegate Callback;
        public readonly IntPtr Context;

        public TaskState(TaskCompleteDelegate callback, IntPtr context)
        {
            Callback = callback;
            Context = context;
        }
    }

    private class EdgeServiceProviderLocator : IServiceProviderLocator
    {
        public IServiceProvider ServiceProvider
        {
            get;
            set;
        }
    }

    private class EdgeServiceProvider : IServiceProvider
    {
        private readonly Dictionary<Type, object> _entries = new Dictionary<Type, object>();

        public EdgeServiceProvider()
        {
            Add(typeof(IServiceProvider), this);
        }

        public void Add(Type type, object instance)
        {
            _entries[type] = instance;
        }

        public bool TryAdd(Type type, object instance)
        {
            if (GetService(type) == null)
            {
                Add(type, instance);
                return true;
            }

            return false;
        }

        public object GetService(Type serviceType)
        {
            return _entries[serviceType];
        }
    }

    private class EdgeRuntimeEnvironment : IRuntimeEnvironment
    {
        public EdgeRuntimeEnvironment(EdgeBootstrapperContext bootstrapperContext)
        {
            OperatingSystem = bootstrapperContext.OperatingSystem;
            OperatingSystemVersion = bootstrapperContext.OperatingSystemVersion;
            RuntimeArchitecture = bootstrapperContext.Architecture;
            ApplicationDirectory = bootstrapperContext.ApplicationDirectory;
            RuntimeVersion = typeof(object).GetTypeInfo().Assembly.GetName().Version.ToString();
            RuntimePath = bootstrapperContext.RuntimeDirectory;
            EdgeNodePath = bootstrapperContext.EdgeNodePath;
        }

        public string OperatingSystem
        {
            get;
        }

        public string OperatingSystemVersion
        {
            get;
        }

        public string RuntimeType => "CoreCLR";

        public string RuntimeArchitecture
        {
            get;
        }

        public string RuntimeVersion
        {
            get;
        }

        public string RuntimePath
        {
            get;
        }

        public string ApplicationDirectory
        {
            get;
        }

        public string EdgeNodePath
        {
            get;
        }
    }

    private class EdgeAssemblyLoadContextAccessor : IAssemblyLoadContextAccessor
    {
        private readonly Lazy<EdgeAssemblyLoadContext> _context;

        public EdgeAssemblyLoadContextAccessor()
        {
            _context = new Lazy<EdgeAssemblyLoadContext>(() => new EdgeAssemblyLoadContext(this));
        }

        public IAssemblyLoadContext GetLoadContext(Assembly assembly)
        {
            return (IAssemblyLoadContext) AssemblyLoadContext.GetLoadContext(assembly);
        }

        public IAssemblyLoadContext Default => _context.Value;

        public EdgeAssemblyLoadContext TypedDefault => _context.Value;
    }

    private class EdgeAssemblyLoadContext : AssemblyLoadContext, IAssemblyLoadContext
    {
        internal readonly Dictionary<string, string> CompileAssemblies = new Dictionary<string, string>();
        private readonly List<IAssemblyLoader> _loaders = new List<IAssemblyLoader>();
        private readonly bool _noProjectJsonFile;
        private readonly EdgeAssemblyLoadContextAccessor _loadContextAccessor;
        private readonly Dictionary<string, Assembly> _loadedAssemblies = new Dictionary<string, Assembly>();

        public EdgeAssemblyLoadContext(EdgeAssemblyLoadContextAccessor loadContextAccessor)
        {
            DebugMessage("EdgeAssemblyLoadContext::ctor (CLR) - Starting");
            DebugMessage("EdgeAssemblyLoadContext::ctor (CLR) - Application root is {0}", RuntimeEnvironment.ApplicationDirectory);

            _loadContextAccessor = loadContextAccessor;

            if (File.Exists(Path.Combine(RuntimeEnvironment.ApplicationDirectory, "project.lock.json")))
            {
                IList<LibraryDescription> libraries = ApplicationHostContext.GetRuntimeLibraries(ApplicationHostContext);
                Dictionary<string, ProjectDescription> projects = libraries.Where(p => p.Type == LibraryTypes.Project).ToDictionary(p => p.Identity.Name, p => (ProjectDescription)p);
                Dictionary<AssemblyName, string> assemblies = PackageDependencyProvider.ResolvePackageAssemblyPaths(libraries);
                
                CompilationEngineContext compilationContext = new CompilationEngineContext(ApplicationEnvironment, RuntimeEnvironment, this, new CompilationCache());
                CompilationEngine compilationEngine = new CompilationEngine(compilationContext);

                AddCompileAssemblies(libraries);

                _loaders.Add(new ProjectAssemblyLoader(_loadContextAccessor, compilationEngine, projects.Values));
                _loaders.Add(new PackageAssemblyLoader(_loadContextAccessor, assemblies, libraries));
            }

            else
            {
                _noProjectJsonFile = true;

                if (File.Exists(Path.Combine(RuntimeEnvironment.EdgeNodePath, "project.lock.json")))
                {
                    ApplicationHostContext stockHostContext = new ApplicationHostContext
                    {
                        ProjectDirectory = RuntimeEnvironment.EdgeNodePath,
                        TargetFramework = TargetFrameworkName
                    };

                    IList<LibraryDescription> libraries = ApplicationHostContext.GetRuntimeLibraries(stockHostContext);
                    Dictionary<AssemblyName, string> assemblies = PackageDependencyProvider.ResolvePackageAssemblyPaths(libraries);

                    AddCompileAssemblies(libraries);

                    _loaders.Add(new PackageAssemblyLoader(_loadContextAccessor, assemblies, libraries));
                }
            }

            DebugMessage("EdgeAssemblyLoadContext::ctor (CLR) - Created the dependency providers for the application");
        }

        protected void AddCompileAssemblies(IList<LibraryDescription> libraries)
        {
            foreach (LibraryDescription libraryDescription in libraries.Where(l => l.Type == LibraryTypes.Package))
            {
                if (CompileAssemblies.ContainsKey(libraryDescription.Identity.Name))
                {
                    continue;
                }

                PackageDescription packageDescription = (PackageDescription)libraryDescription;

                if (packageDescription.Target.CompileTimeAssemblies != null && packageDescription.Target.CompileTimeAssemblies.Count > 0)
                {
                    CompileAssemblies[libraryDescription.Identity.Name] = Path.Combine(packageDescription.Path, packageDescription.Target.CompileTimeAssemblies[0].Path);
                }

                else if (packageDescription.Target.RuntimeAssemblies != null && packageDescription.Target.RuntimeAssemblies.Count > 0)
                {
                    CompileAssemblies[libraryDescription.Identity.Name] = Path.Combine(packageDescription.Path, packageDescription.Target.RuntimeAssemblies[0].Path);
                }
            }
        }

        internal void AddCompiler(string compilerDirectory)
        {
            DebugMessage("EdgeAssemblyLoadContext::AddCompiler (CLR) - Adding the compiler in {0}", compilerDirectory);

            ApplicationHostContext compilerHostContext = new ApplicationHostContext
            {
                ProjectDirectory = compilerDirectory,
                TargetFramework = TargetFrameworkName
            };

            IList<LibraryDescription> libraries = ApplicationHostContext.GetRuntimeLibraries(compilerHostContext);
            Dictionary<AssemblyName, string> assemblies = PackageDependencyProvider.ResolvePackageAssemblyPaths(libraries);

            AddCompileAssemblies(libraries);

            _loaders.Add(new PackageAssemblyLoader(_loadContextAccessor, assemblies, libraries));

            DebugMessage("EdgeAssemblyLoadContext::AddCompiler (CLR) - Finished");
        }

        [SecuritySafeCritical]
        protected override Assembly Load(AssemblyName assemblyName)
        {
            DebugMessage("EdgeAssemblyLoadContext::Load (CLR) - Trying to load {0}", assemblyName.Name);

            if (_loadedAssemblies.ContainsKey(assemblyName.Name))
            {
                DebugMessage("EdgeAssemblyLoadContext::Load (CLR) - Returning previously loaded assembly");
                return _loadedAssemblies[assemblyName.Name];
            }

            foreach (IAssemblyLoader loader in _loaders)
            {
                try
                {
                    Assembly assembly = loader.Load(assemblyName);

                    if (assembly != null)
                    {
                        DebugMessage("EdgeAssemblyLoadContext::Load (CLR) - Successfully resolved assembly with {0}", loader.GetType().Name);
                        _loadedAssemblies[assemblyName.Name] = assembly;

                        return assembly;
                    }
                }

                catch (Exception e)
                {
                    DebugMessage("EdgeAssemblyLoadContext::Load (CLR) - Error trying to load {0} with {1}: {2}{3}{4}", assemblyName.Name, loader.GetType().Name, e.Message, Environment.NewLine, e.StackTrace);
                    throw;
                }
            }

            DebugMessage("EdgeAssemblyLoadContext::Load (CLR) - Unable to resolve assembly with any of the loaders");

            try
            {
                return Assembly.Load(assemblyName);
            }

            catch (Exception e)
            {
                if (_noProjectJsonFile)
                {
                    throw new Exception(
                        String.Format(
                            "Could not load assembly '{0}'.  Please ensure that a project.json file specifying the dependencies for this application exists either in the current directory or in a .NET project directory specified using the EDGE_APP_ROOT environment variable and that the 'dnu restore' command has been run for this project.json file.",
                            assemblyName.Name), e);
                }

                throw;
            }
        }

        public Assembly LoadFile(string assemblyPath)
        {
            if (!Path.IsPathRooted(assemblyPath))
            {
                assemblyPath = Path.Combine(RuntimeEnvironment.ApplicationDirectory, assemblyPath);
            }

            if (!File.Exists(assemblyPath))
            {
                throw new FileNotFoundException("Assembly file not found.", assemblyPath);
            }

            return LoadFromAssemblyPath(assemblyPath);
        }

        public Assembly LoadStream(Stream assemblyStream, Stream assemblySymbols)
        {
            return LoadFromStream(assemblyStream);
        }

        public IntPtr LoadUnmanagedLibrary(string name)
        {
            return LoadUnmanagedDll(name);
        }

        public IntPtr LoadUnmanagedLibraryFromPath(string path)
        {
            return LoadUnmanagedDllFromPath(path);
        }

        public void Dispose()
        {
        }

        Assembly IAssemblyLoadContext.Load(AssemblyName assemblyName)
        {
            return Load(assemblyName);
        }
    }
    
    // ReSharper disable InconsistentNaming
    private static EdgeAssemblyLoadContextAccessor LoadContextAccessor;
    private static ApplicationHostContext ApplicationHostContext;
    private static ApplicationEnvironment ApplicationEnvironment;
    private static EdgeRuntimeEnvironment RuntimeEnvironment;
    // ReSharper enable InconsistentNaming

    private static readonly bool DebugMode = Environment.GetEnvironmentVariable("EDGE_DEBUG") == "1";
    private static readonly FrameworkName TargetFrameworkName = new FrameworkName("DNXCore,Version=v5.0");
    private static readonly long MinDateTimeTicks = 621355968000000000;
    private static readonly Dictionary<Type, List<Tuple<string, Func<object, object>>>> TypePropertyAccessors = new Dictionary<Type, List<Tuple<string, Func<object, object>>>>();
    private static readonly int PointerSize = Marshal.SizeOf<IntPtr>();
    private static readonly int V8BufferDataSize = Marshal.SizeOf<V8BufferData>();
    private static readonly int V8ObjectDataSize = Marshal.SizeOf<V8ObjectData>();
    private static readonly int V8ArrayDataSize = Marshal.SizeOf<V8ArrayData>();
    private static readonly Dictionary<string, Tuple<Type, MethodInfo>> Compilers = new Dictionary<string, Tuple<Type, MethodInfo>>();

    public static void Initialize(IntPtr context, IntPtr exception)
    {
        try
        {
            DebugMessage("CoreCLREmbedding::Initialize (CLR) - Starting");

            RuntimeEnvironment = new EdgeRuntimeEnvironment(Marshal.PtrToStructure<EdgeBootstrapperContext>(context));

            Project project;
            Project.TryGetProject(RuntimeEnvironment.ApplicationDirectory, out project);

            ApplicationHostContext = new ApplicationHostContext
            {
                ProjectDirectory = RuntimeEnvironment.ApplicationDirectory,
                TargetFramework = TargetFrameworkName,
                Project = project
            };

            ApplicationEnvironment = new ApplicationEnvironment(ApplicationHostContext.Project, TargetFrameworkName, "Release", null);
            LoadContextAccessor = new EdgeAssemblyLoadContextAccessor();

            EdgeServiceProvider serviceProvider = new EdgeServiceProvider();

            serviceProvider.Add(typeof(IRuntimeEnvironment), RuntimeEnvironment);
            serviceProvider.Add(typeof(IApplicationEnvironment), ApplicationEnvironment);
            serviceProvider.Add(typeof(IAssemblyLoadContextAccessor), LoadContextAccessor);

            CallContextServiceLocator.Locator = new EdgeServiceProviderLocator
            {
                ServiceProvider = serviceProvider
            };

            PlatformServices.SetDefault(PlatformServices.Create(null, ApplicationEnvironment, RuntimeEnvironment, null, LoadContextAccessor, null));

            DebugMessage("CoreCLREmbedding::Initialize (CLR) - Complete");
        }

        catch (Exception e)
        {
            DebugMessage("CoreCLREmbedding::Initialize (CLR) - Exception was thrown: {0}", e.Message);

            V8Type v8Type;
            Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8Type));
        }
    }

    [SecurityCritical]
    public static IntPtr GetFunc(string assemblyFile, string typeName, string methodName, IntPtr exception)
    {
        try
        {
            Marshal.WriteIntPtr(exception, IntPtr.Zero);

            Assembly assembly;

            if (assemblyFile.EndsWith(".dll", StringComparison.OrdinalIgnoreCase) || assemblyFile.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
            {
                if (!Path.IsPathRooted(assemblyFile))
                {
                    assemblyFile = Path.Combine(Directory.GetCurrentDirectory(), assemblyFile);
                }

                assembly = LoadContextAccessor.Default.LoadFile(assemblyFile);
            }

            else
            {
                assembly = LoadContextAccessor.Default.Load(new AssemblyName(assemblyFile));
            }

            DebugMessage("CoreCLREmbedding::GetFunc (CLR) - Assembly {0} loaded successfully", assemblyFile);

            ClrFuncReflectionWrap wrapper = ClrFuncReflectionWrap.Create(assembly, typeName, methodName);
            DebugMessage("CoreCLREmbedding::GetFunc (CLR) - Method {0}.{1}() loaded successfully", typeName, methodName);

            Func<object, Task<object>> wrapperFunc = wrapper.Call;
            GCHandle wrapperHandle = GCHandle.Alloc(wrapperFunc);

            return GCHandle.ToIntPtr(wrapperHandle);
        }

        catch (Exception e)
        {
            DebugMessage("CoreCLREmbedding::GetFunc (CLR) - Exception was thrown: {0}", e.Message);

            V8Type v8Type;
            Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8Type));

            return IntPtr.Zero;
        }
    }

    [SecurityCritical]
    public static IntPtr CompileFunc(IntPtr v8Options, int payloadType, IntPtr exception)
    {
        try
        {
            Marshal.WriteIntPtr(exception, IntPtr.Zero);

            IDictionary<string, object> options = (IDictionary<string, object>)MarshalV8ToCLR(v8Options, (V8Type)payloadType);
            string compiler = (string)options["compiler"];

            if (!Path.IsPathRooted(compiler))
            {
                compiler = Path.Combine(Directory.GetCurrentDirectory(), compiler);
            }

            MethodInfo compileMethod;
            Type compilerType;

            if (!Compilers.ContainsKey(compiler))
            {
                LoadContextAccessor.TypedDefault.AddCompiler(Directory.GetParent(compiler).FullName);

                Assembly compilerAssembly = LoadContextAccessor.Default.LoadFile(compiler);
                DebugMessage("CoreCLREmbedding::CompileFunc (CLR) - Compiler assembly {0} loaded successfully", compiler);

                compilerType = compilerAssembly.GetType("EdgeCompiler");

                if (compilerType == null)
                {
                    throw new TypeLoadException("Could not load type 'EdgeCompiler'");
                }

                compileMethod = compilerType.GetMethod("CompileFunc", BindingFlags.Instance | BindingFlags.Public);

                if (compileMethod == null)
                {
                    throw new Exception("Unable to find the CompileFunc() method on " + compilerType.FullName + ".");
                }

                MethodInfo setAssemblyLoader = compilerType.GetMethod("SetAssemblyLoader", BindingFlags.Static | BindingFlags.Public);

                setAssemblyLoader?.Invoke(null, new object[]
                {
                    new Func<Stream, Assembly>(assemblyStream => LoadContextAccessor.Default.LoadStream(assemblyStream, null))
                });

                Compilers[compiler] = new Tuple<Type, MethodInfo>(compilerType, compileMethod);
            }

            else
            {
                compilerType = Compilers[compiler].Item1;
                compileMethod = Compilers[compiler].Item2;
            }

            object compilerInstance = Activator.CreateInstance(compilerType);

            DebugMessage("CoreCLREmbedding::CompileFunc (CLR) - Starting compilation");
            Func<object, Task<object>> compiledFunction = (Func<object, Task<object>>)compileMethod.Invoke(compilerInstance, new object[]
            {
                options,
                LoadContextAccessor.TypedDefault.CompileAssemblies
            });
            DebugMessage("CoreCLREmbedding::CompileFunc (CLR) - Compilation complete");

            GCHandle handle = GCHandle.Alloc(compiledFunction);

            return GCHandle.ToIntPtr(handle);
        }

        catch (TargetInvocationException e)
        {
            DebugMessage("CoreCLREmbedding::CompileFunc (CLR) - Exception was thrown: {0}\n{1}", e.InnerException.Message, e.InnerException.StackTrace);

            V8Type v8Type;
            Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8Type));

            return IntPtr.Zero;
        }

        catch (Exception e)
        {
            DebugMessage("CoreCLREmbedding::CompileFunc (CLR) - Exception was thrown: {0}\n{1}", e.Message, e.StackTrace);

            V8Type v8Type;
            Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8Type));

            return IntPtr.Zero;
        }
    }

    [SecurityCritical]
    public static void FreeHandle(IntPtr gcHandle)
    {
        GCHandle actualHandle = GCHandle.FromIntPtr(gcHandle);
        actualHandle.Free();
    }

    [SecurityCritical]
    public static void CallFunc(IntPtr function, IntPtr payload, int payloadType, IntPtr taskState, IntPtr result, IntPtr resultType)
    {
        try
        {
            DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Starting");

            GCHandle wrapperHandle = GCHandle.FromIntPtr(function);
            Func<object, Task<object>> wrapperFunc = (Func<object, Task<object>>)wrapperHandle.Target;

            DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Marshalling data of type {0} and calling the .NET method", ((V8Type)payloadType).ToString("G"));
            Task<Object> functionTask = wrapperFunc(MarshalV8ToCLR(payload, (V8Type)payloadType));

            if (functionTask.IsFaulted)
            {
                DebugMessage("CoreCLREmbedding::CallFunc (CLR) - .NET method ran synchronously and faulted, marshalling exception data for V8");

                V8Type taskExceptionType;

                Marshal.WriteInt32(taskState, (int)TaskStatus.Faulted);
                Marshal.WriteIntPtr(result, MarshalCLRToV8(functionTask.Exception, out taskExceptionType));
                Marshal.WriteInt32(resultType, (int)V8Type.Exception);
            }

            else if (functionTask.IsCompleted)
            {
                DebugMessage("CoreCLREmbedding::CallFunc (CLR) - .NET method ran synchronously, marshalling data for V8");

                V8Type taskResultType;
                IntPtr marshalData = MarshalCLRToV8(functionTask.Result, out taskResultType);

                DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Method return data is of type {0}", taskResultType.ToString("G"));

                Marshal.WriteInt32(taskState, (int)TaskStatus.RanToCompletion);
                Marshal.WriteIntPtr(result, marshalData);
                Marshal.WriteInt32(resultType, (int)taskResultType);
            }

            else
            {
                DebugMessage("CoreCLREmbedding::CallFunc (CLR) - .NET method ran asynchronously, returning task handle and status");

                GCHandle taskHandle = GCHandle.Alloc(functionTask);

                Marshal.WriteInt32(taskState, (int)functionTask.Status);
                Marshal.WriteIntPtr(result, GCHandle.ToIntPtr(taskHandle));
                Marshal.WriteInt32(resultType, (int)V8Type.Task);
            }

            DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Finished");
        }

        catch (Exception e)
        {
            DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Exception was thrown: {0}", e.Message);

            V8Type v8Type;

            Marshal.WriteIntPtr(result, MarshalCLRToV8(e, out v8Type));
            Marshal.WriteInt32(resultType, (int)v8Type);
            Marshal.WriteInt32(taskState, (int)TaskStatus.Faulted);
        }
    }

    private static void TaskCompleted(Task<object> task, object state)
    {
        DebugMessage("CoreCLREmbedding::TaskCompleted (CLR) - Task completed with a state of {0}", task.Status.ToString("G"));
        DebugMessage("CoreCLREmbedding::TaskCompleted (CLR) - Marshalling data to return to V8", task.Status.ToString("G"));

        V8Type v8Type;
        TaskState actualState = (TaskState)state;
        IntPtr resultObject;
        TaskStatus taskStatus;

        if (task.IsFaulted)
        {
            taskStatus = TaskStatus.Faulted;

            try
            {
                resultObject = MarshalCLRToV8(task.Exception, out v8Type);
            }

            catch (Exception e)
            {
                taskStatus = TaskStatus.Faulted;
                resultObject = MarshalCLRToV8(e, out v8Type);
            }
        }

        else
        {
            taskStatus = TaskStatus.RanToCompletion;

            try
            {
                resultObject = MarshalCLRToV8(task.Result, out v8Type);
            }

            catch (Exception e)
            {
                taskStatus = TaskStatus.Faulted;
                resultObject = MarshalCLRToV8(e, out v8Type);
            }
        }

        DebugMessage("CoreCLREmbedding::TaskCompleted (CLR) - Invoking unmanaged callback");
        actualState.Callback(resultObject, (int)v8Type, (int)taskStatus, actualState.Context);
    }

    [SecurityCritical]
    public static void ContinueTask(IntPtr task, IntPtr context, IntPtr callback, IntPtr exception)
    {
        try
        {
            Marshal.WriteIntPtr(exception, IntPtr.Zero);

            DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Starting");

            GCHandle taskHandle = GCHandle.FromIntPtr(task);
            Task<Object> actualTask = (Task<Object>)taskHandle.Target;

            TaskCompleteDelegate taskCompleteDelegate = Marshal.GetDelegateForFunctionPointer<TaskCompleteDelegate>(callback);
            DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Marshalled unmanaged callback successfully");

            actualTask.ContinueWith(TaskCompleted, new TaskState(taskCompleteDelegate, context));

            DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Finished");
        }

        catch (Exception e)
        {
            DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Exception was thrown: {0}", e.Message);

            V8Type v8Type;
            Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8Type));
        }
    }

    [SecurityCritical]
    public static void SetCallV8FunctionDelegate(IntPtr callV8Function, IntPtr exception)
    {
        try
        {
            Marshal.WriteIntPtr(exception, IntPtr.Zero);
            NodejsFuncInvokeContext.CallV8Function = Marshal.GetDelegateForFunctionPointer<CallV8FunctionDelegate>(callV8Function);
        }

        catch (Exception e)
        {
            DebugMessage("CoreCLREmbedding::SetCallV8FunctionDelegate (CLR) - Exception was thrown: {0}", e.Message);

            V8Type v8Type;
            Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8Type));
        }
    }

    [SecurityCritical]
    public static void FreeMarshalData(IntPtr marshalData, int v8Type)
    {
        switch ((V8Type)v8Type)
        {
            case V8Type.String:
            case V8Type.Int32:
            case V8Type.Boolean:
            case V8Type.Number:
            case V8Type.Date:
                Marshal.FreeHGlobal(marshalData);
                break;

            case V8Type.Object:
            case V8Type.Exception:
                V8ObjectData objectData = Marshal.PtrToStructure<V8ObjectData>(marshalData);

                for (int i = 0; i < objectData.propertiesCount; i++)
                {
                    int propertyType = Marshal.ReadInt32(objectData.propertyTypes, i * sizeof(int));
                    IntPtr propertyValue = Marshal.ReadIntPtr(objectData.propertyValues, i * PointerSize);
                    IntPtr propertyName = Marshal.ReadIntPtr(objectData.propertyNames, i * PointerSize);

                    FreeMarshalData(propertyValue, propertyType);
                    Marshal.FreeHGlobal(propertyName);
                }

                Marshal.FreeHGlobal(objectData.propertyTypes);
                Marshal.FreeHGlobal(objectData.propertyValues);
                Marshal.FreeHGlobal(objectData.propertyNames);
                Marshal.FreeHGlobal(marshalData);

                break;

            case V8Type.Array:
                V8ArrayData arrayData = Marshal.PtrToStructure<V8ArrayData>(marshalData);

                for (int i = 0; i < arrayData.arrayLength; i++)
                {
                    int itemType = Marshal.ReadInt32(arrayData.itemTypes, i * sizeof(int));
                    IntPtr itemValue = Marshal.ReadIntPtr(arrayData.itemValues, i * PointerSize);

                    FreeMarshalData(itemValue, itemType);
                }

                Marshal.FreeHGlobal(arrayData.itemTypes);
                Marshal.FreeHGlobal(arrayData.itemValues);
                Marshal.FreeHGlobal(marshalData);

                break;

            case V8Type.Buffer:
                V8BufferData bufferData = Marshal.PtrToStructure<V8BufferData>(marshalData);

                Marshal.FreeHGlobal(bufferData.buffer);
                Marshal.FreeHGlobal(marshalData);

                break;

            case V8Type.Null:
            case V8Type.Function:
                break;

            default:
                throw new Exception("Unsupported marshalled data type: " + v8Type);
        }
    }

    // ReSharper disable once InconsistentNaming
    public static IntPtr MarshalCLRToV8(object clrObject, out V8Type v8Type)
    {
        if (clrObject == null)
        {
            v8Type = V8Type.Null;
            return IntPtr.Zero;
        }

        else if (clrObject is string)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi((string) clrObject);
        }

        else if (clrObject is char)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi(clrObject.ToString());
        }

        else if (clrObject is bool)
        {
            v8Type = V8Type.Boolean;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (int));

            Marshal.WriteInt32(memoryLocation, ((bool) clrObject)
                ? 1
                : 0);
            return memoryLocation;
        }

        else if (clrObject is Guid)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi(clrObject.ToString());
        }

        else if (clrObject is DateTime)
        {
            v8Type = V8Type.Date;
            DateTime dateTime = (DateTime) clrObject;

            if (dateTime.Kind == DateTimeKind.Local)
            {
                dateTime = dateTime.ToUniversalTime();
            }

            else if (dateTime.Kind == DateTimeKind.Unspecified)
            {
                dateTime = new DateTime(dateTime.Ticks, DateTimeKind.Utc);
            }

            long ticks = (dateTime.Ticks - MinDateTimeTicks)/10000;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (double));

            WriteDouble(memoryLocation, ticks);
            return memoryLocation;
        }

        else if (clrObject is DateTimeOffset)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi(clrObject.ToString());
        }

        else if (clrObject is Uri)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi(clrObject.ToString());
        }

        else if (clrObject is short)
        {
            v8Type = V8Type.Int32;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (int));

            Marshal.WriteInt32(memoryLocation, Convert.ToInt32(clrObject));
            return memoryLocation;
        }

        else if (clrObject is int)
        {
            v8Type = V8Type.Int32;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (int));

            Marshal.WriteInt32(memoryLocation, (int) clrObject);
            return memoryLocation;
        }

        else if (clrObject is long)
        {
            v8Type = V8Type.Number;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (double));

            WriteDouble(memoryLocation, Convert.ToDouble((long) clrObject));
            return memoryLocation;
        }

        else if (clrObject is double)
        {
            v8Type = V8Type.Number;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (double));

            WriteDouble(memoryLocation, (double) clrObject);
            return memoryLocation;
        }

        else if (clrObject is float)
        {
            v8Type = V8Type.Number;
            IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof (double));

            WriteDouble(memoryLocation, Convert.ToDouble((Single) clrObject));
            return memoryLocation;
        }

        else if (clrObject is decimal)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi(clrObject.ToString());
        }

        else if (clrObject is Enum)
        {
            v8Type = V8Type.String;
            return Marshal.StringToHGlobalAnsi(clrObject.ToString());
        }

        else if (clrObject is byte[] || clrObject is IEnumerable<byte>)
        {
            v8Type = V8Type.Buffer;

            V8BufferData bufferData = new V8BufferData();
            byte[] buffer;

            if (clrObject is byte[])
            {
                buffer = (byte[]) clrObject;
            }

            else
            {
                buffer = ((IEnumerable<byte>) clrObject).ToArray();
            }

            bufferData.bufferLength = buffer.Length;
            bufferData.buffer = Marshal.AllocHGlobal(buffer.Length*sizeof (byte));

            Marshal.Copy(buffer, 0, bufferData.buffer, bufferData.bufferLength);

            IntPtr destinationPointer = Marshal.AllocHGlobal(V8BufferDataSize);
            Marshal.StructureToPtr(bufferData, destinationPointer, false);

            return destinationPointer;
        }

        else if (clrObject is IDictionary || clrObject is ExpandoObject)
        {
            v8Type = V8Type.Object;

            IEnumerable keys;
            int keyCount;
            Func<object, object> getValue;

            if (clrObject is ExpandoObject)
            {
                IDictionary<string, object> objectDictionary = (IDictionary<string, object>) clrObject;

                keys = objectDictionary.Keys;
                keyCount = objectDictionary.Keys.Count;
                getValue = index => objectDictionary[index.ToString()];
            }

            else
            {
                IDictionary objectDictionary = (IDictionary) clrObject;

                keys = objectDictionary.Keys;
                keyCount = objectDictionary.Keys.Count;
                getValue = index => objectDictionary[index];
            }

            V8ObjectData objectData = new V8ObjectData();
            int counter = 0;

            objectData.propertiesCount = keyCount;
            objectData.propertyNames = Marshal.AllocHGlobal(PointerSize*keyCount);
            objectData.propertyTypes = Marshal.AllocHGlobal(sizeof (int)*keyCount);
            objectData.propertyValues = Marshal.AllocHGlobal(PointerSize*keyCount);

            foreach (object key in keys)
            {
                Marshal.WriteIntPtr(objectData.propertyNames, counter*PointerSize, Marshal.StringToHGlobalAnsi(key.ToString()));
                V8Type propertyType;
                Marshal.WriteIntPtr(objectData.propertyValues, counter*PointerSize, MarshalCLRToV8(getValue(key), out propertyType));
                Marshal.WriteInt32(objectData.propertyTypes, counter*sizeof (int), (int) propertyType);

                counter++;
            }

            IntPtr destinationPointer = Marshal.AllocHGlobal(V8ObjectDataSize);
            Marshal.StructureToPtr(objectData, destinationPointer, false);

            return destinationPointer;
        }

        else if (clrObject is IEnumerable)
        {
            v8Type = V8Type.Array;

            V8ArrayData arrayData = new V8ArrayData();
            List<IntPtr> itemValues = new List<IntPtr>();
            List<int> itemTypes = new List<int>();

            foreach (object item in (IEnumerable) clrObject)
            {
                V8Type itemType;

                itemValues.Add(MarshalCLRToV8(item, out itemType));
                itemTypes.Add((int) itemType);
            }

            arrayData.arrayLength = itemValues.Count;
            arrayData.itemTypes = Marshal.AllocHGlobal(sizeof (int)*arrayData.arrayLength);
            arrayData.itemValues = Marshal.AllocHGlobal(PointerSize*arrayData.arrayLength);

            Marshal.Copy(itemTypes.ToArray(), 0, arrayData.itemTypes, arrayData.arrayLength);
            Marshal.Copy(itemValues.ToArray(), 0, arrayData.itemValues, arrayData.arrayLength);

            IntPtr destinationPointer = Marshal.AllocHGlobal(V8ArrayDataSize);
            Marshal.StructureToPtr(arrayData, destinationPointer, false);

            return destinationPointer;
        }

        else if (clrObject.GetType().GetTypeInfo().IsGenericType && clrObject.GetType().GetGenericTypeDefinition() == typeof (Func<,>))
        {
            Func<object, Task<object>> funcObject = clrObject as Func<object, Task<object>>;

            if (funcObject == null)
            {
                throw new Exception("Properties that return Func<> instances must return Func<object, Task<object>> instances");
            }

            v8Type = V8Type.Function;
            return GCHandle.ToIntPtr(GCHandle.Alloc(funcObject));
        }

        else
        {
            v8Type = clrObject is Exception
                ? V8Type.Exception
                : V8Type.Object;

            if (clrObject is Exception)
            {
                AggregateException aggregateException = clrObject as AggregateException;

                if (aggregateException?.InnerExceptions != null && aggregateException.InnerExceptions.Count > 0)
                {
                    clrObject = aggregateException.InnerExceptions[0];
                }

                else
                {
                    TargetInvocationException targetInvocationException = clrObject as TargetInvocationException;

                    if (targetInvocationException?.InnerException != null)
                    {
                        clrObject = targetInvocationException.InnerException;
                    }
                }
            }

            List<Tuple<string, Func<object, object>>> propertyAccessors = GetPropertyAccessors(clrObject.GetType());
            V8ObjectData objectData = new V8ObjectData();
            int counter = 0;

            objectData.propertiesCount = propertyAccessors.Count;
            objectData.propertyNames = Marshal.AllocHGlobal(PointerSize*propertyAccessors.Count);
            objectData.propertyTypes = Marshal.AllocHGlobal(sizeof (int)*propertyAccessors.Count);
            objectData.propertyValues = Marshal.AllocHGlobal(PointerSize*propertyAccessors.Count);

            foreach (Tuple<string, Func<object, object>> propertyAccessor in propertyAccessors)
            {
                Marshal.WriteIntPtr(objectData.propertyNames, counter*PointerSize, Marshal.StringToHGlobalAnsi(propertyAccessor.Item1));

                V8Type propertyType;

                Marshal.WriteIntPtr(objectData.propertyValues, counter*PointerSize, MarshalCLRToV8(propertyAccessor.Item2(clrObject), out propertyType));
                Marshal.WriteInt32(objectData.propertyTypes, counter*sizeof (int), (int) propertyType);
                counter++;
            }

            IntPtr destinationPointer = Marshal.AllocHGlobal(V8ObjectDataSize);
            Marshal.StructureToPtr(objectData, destinationPointer, false);

            return destinationPointer;
        }
    }

    public static object MarshalV8ToCLR(IntPtr v8Object, V8Type objectType)
    {
        switch (objectType)
        {
            case V8Type.String:
                return Marshal.PtrToStringAnsi(v8Object);

            case V8Type.Object:
                return V8ObjectToExpando(Marshal.PtrToStructure<V8ObjectData>(v8Object));

            case V8Type.Boolean:
                return Marshal.ReadByte(v8Object) != 0;

            case V8Type.Number:
                return ReadDouble(v8Object);

            case V8Type.Date:
                double ticks = ReadDouble(v8Object);
                return new DateTime(Convert.ToInt64(ticks) * 10000 + MinDateTimeTicks, DateTimeKind.Utc);

            case V8Type.Null:
                return null;

            case V8Type.Int32:
                return Marshal.ReadInt32(v8Object);

            case V8Type.UInt32:
                return (uint)Marshal.ReadInt32(v8Object);

            case V8Type.Function:
                NodejsFunc nodejsFunc = new NodejsFunc(v8Object);
                return nodejsFunc.GetFunc();

            case V8Type.Array:
                V8ArrayData arrayData = Marshal.PtrToStructure<V8ArrayData>(v8Object);
                object[] array = new object[arrayData.arrayLength];

                for (int i = 0; i < arrayData.arrayLength; i++)
                {
                    int itemType = Marshal.ReadInt32(arrayData.itemTypes, i * sizeof(int));
                    IntPtr itemValuePointer = Marshal.ReadIntPtr(arrayData.itemValues, i * PointerSize);

                    array[i] = MarshalV8ToCLR(itemValuePointer, (V8Type)itemType);
                }

                return array;

            case V8Type.Buffer:
                V8BufferData bufferData = Marshal.PtrToStructure<V8BufferData>(v8Object);
                byte[] buffer = new byte[bufferData.bufferLength];

                Marshal.Copy(bufferData.buffer, buffer, 0, bufferData.bufferLength);

                return buffer;

            case V8Type.Exception:
                string message = Marshal.PtrToStringAnsi(v8Object);
                return new Exception(message);

            default:
                throw new Exception("Unsupported V8 object type: " + objectType + ".");
        }
    }

    private static unsafe void WriteDouble(IntPtr pointer, double value)
    {
        try
        {
            byte* address = (byte*)pointer;

            if ((unchecked((int)address) & 0x7) == 0)
            {
                *((double*)address) = value;
            }

            else
            {
                byte* valuePointer = (byte*)&value;

                address[0] = valuePointer[0];
                address[1] = valuePointer[1];
                address[2] = valuePointer[2];
                address[3] = valuePointer[3];
                address[4] = valuePointer[4];
                address[5] = valuePointer[5];
                address[6] = valuePointer[6];
                address[7] = valuePointer[7];
            }
        }

        catch (NullReferenceException)
        {
            throw new Exception("Access violation.");
        }
    }

    private static unsafe double ReadDouble(IntPtr pointer)
    {
        try
        {
            byte* address = (byte*)pointer;

            if ((unchecked((int)address) & 0x7) == 0)
            {
                return *((double*)address);
            }

            else
            {
                double value;
                byte* valuePointer = (byte*)&value;

                valuePointer[0] = address[0];
                valuePointer[1] = address[1];
                valuePointer[2] = address[2];
                valuePointer[3] = address[3];
                valuePointer[4] = address[4];
                valuePointer[5] = address[5];
                valuePointer[6] = address[6];
                valuePointer[7] = address[7];

                return value;
            }
        }

        catch (NullReferenceException)
        {
            throw new Exception("Access violation.");
        }
    }

    private static ExpandoObject V8ObjectToExpando(V8ObjectData v8Object)
    {
        ExpandoObject expando = new ExpandoObject();
        IDictionary<string, object> expandoDictionary = expando;

        for (int i = 0; i < v8Object.propertiesCount; i++)
        {
            int propertyType = Marshal.ReadInt32(v8Object.propertyTypes, i * sizeof(int));
            IntPtr propertyNamePointer = Marshal.ReadIntPtr(v8Object.propertyNames, i * PointerSize);
            string propertyName = Marshal.PtrToStringAnsi(propertyNamePointer);
            IntPtr propertyValuePointer = Marshal.ReadIntPtr(v8Object.propertyValues, i * PointerSize);

            expandoDictionary.Add(propertyName, MarshalV8ToCLR(propertyValuePointer, (V8Type)propertyType));
        }

        return expando;
    }

    [Conditional("DEBUG")]
    internal static void DebugMessage(string message, params object[] parameters)
    {
        if (DebugMode)
        {
            Console.WriteLine(message, parameters);
        }
    }

    private static List<Tuple<string, Func<object, object>>> GetPropertyAccessors(Type type)
    {
        if (TypePropertyAccessors.ContainsKey(type))
        {
            return TypePropertyAccessors[type];
        }

        List<Tuple<string, Func<object, object>>> propertyAccessors = new List<Tuple<string, Func<object, object>>>();

        foreach (PropertyInfo propertyInfo in type.GetProperties(BindingFlags.Instance | BindingFlags.Public))
        {
            ParameterExpression instance = Expression.Parameter(typeof(object));
            UnaryExpression instanceConvert = Expression.TypeAs(instance, type);
            MemberExpression property = Expression.Property(instanceConvert, propertyInfo);
            UnaryExpression propertyConvert = Expression.TypeAs(property, typeof(object));

            propertyAccessors.Add(new Tuple<string, Func<object, object>>(propertyInfo.Name, (Func<object, object>)Expression.Lambda(propertyConvert, instance).Compile()));
        }

        foreach (FieldInfo fieldInfo in type.GetFields(BindingFlags.Instance | BindingFlags.Public))
        {
            ParameterExpression instance = Expression.Parameter(typeof(object));
            UnaryExpression instanceConvert = Expression.TypeAs(instance, type);
            MemberExpression field = Expression.Field(instanceConvert, fieldInfo);
            UnaryExpression fieldConvert = Expression.TypeAs(field, typeof(object));

            propertyAccessors.Add(new Tuple<string, Func<object, object>>(fieldInfo.Name, (Func<object, object>)Expression.Lambda(fieldConvert, instance).Compile()));
        }

        if (typeof(Exception).IsAssignableFrom(type) && !propertyAccessors.Any(a => a.Item1 == "Name"))
        {
            propertyAccessors.Add(new Tuple<string, Func<object, object>>("Name", o => type.FullName));
        }

        TypePropertyAccessors[type] = propertyAccessors;

        return propertyAccessors;
    }
}
