using System;
using System.Threading.Tasks;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Collections.Generic;

public static class MonoEmbedding
{
    // dummy Main so we can exec from mono
    static public int Main()
    {
        return 0;
    }

    static public Func<Object, Task<Object>> GetFunc(string assemblyFile, string typeName, string methodName)
    {
        var assembly = Assembly.LoadFrom(assemblyFile);
        var wrap = ClrFuncReflectionWrap.Create(assembly, typeName, methodName);
        return new Func<Object, Task<Object>>(wrap.Call);
    }

    static public Dictionary<string, object> CreateDictionary()
    {
        return new Dictionary<string, object>();
    }

    static public object[] IEnumerableToArray(System.Collections.IEnumerable enumerable)
    {
        var list = new List<object>();
        foreach (var item in enumerable)
            list.Add(item);

        return list.ToArray();
    }

    static public Type GetFuncType()
    {
        return typeof(Func<Object, Task<Object>>);
    }

    static public void edgeAppCompletedOnCLRThread(Task<object> task, object state)
    {
        var context = (ClrFuncInvokeContext)state;
        context.CompleteOnCLRThread(task);
    }

    static public void ContinueTask(Task<object> task, object state)
    {
        // Will complete asynchronously. Schedule continuation to finish processing.
        task.ContinueWith(new Action<Task<object>, object>(edgeAppCompletedOnCLRThread), state);
    }
}

class ClrFuncInvokeContext
{
    IntPtr native;
    //Persistent<Function>* callback;
    //uv_edge_async_t* uv_edge_async;

    //void DisposeCallback();


    Object Payload;
    Task<Object> Task;
    bool Sync;

    //ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync);

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void CompleteOnCLRThreadICall(IntPtr ptr, Task<object> task);

    internal void CompleteOnCLRThread(Task<object> task)
    {
        CompleteOnCLRThreadICall(native, task);
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void CompleteOnV8ThreadAsynchronousICall(IntPtr ptr);

    void CompleteOnV8ThreadAsynchronous()
    {
        CompleteOnV8ThreadAsynchronousICall(native);
    }

    public Action GetCompleteOnV8ThreadAsynchronousAction()
    {
        return new Action(CompleteOnV8ThreadAsynchronous);
    }

    //Handle<v8::Value> CompleteOnV8Thread(bool completedSynchronously);
};

class NodejsFunc
{
    IntPtr native;
    //~NodejsFunc();
    //!NodejsFunc();

    Func<object, Task<object>> GetFunc()
    {
        return new Func<object, Task<object>>(this.FunctionWrapper);
    }

    Task<object> FunctionWrapper(object payload)
    {
        return null;
    }
};

class ClrFunc
{
    Func<Object, Task<Object>> func;

    //static Handle<v8::Value> MarshalCLRObjectToV8(System::Object^ netdata);

    //publicstatic Handle<v8::Value> Initialize(const v8::Arguments& args);
    //static Handle<v8::Function> Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func);
    //Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callback);
    //static Handle<v8::Value> MarshalCLRToV8(System::Object^ netdata);
    //static System::Object^ MarshalV8ToCLR(ClrFuncInvokeContext^ context, Handle<v8::Value> jsdata);    
};


class ClrFuncReflectionWrap
{
    Object instance;
    MethodInfo invokeMethod;

    public static ClrFuncReflectionWrap Create(Assembly assembly, String typeName, String methodName)
    {
        Type startupType = assembly.GetType(typeName, true, true);
        ClrFuncReflectionWrap wrap = new ClrFuncReflectionWrap();
        wrap.instance = System.Activator.CreateInstance(startupType, false);
        wrap.invokeMethod = startupType.GetMethod(methodName, BindingFlags.Instance | BindingFlags.Public);
        if (wrap.invokeMethod == null)
        {
            throw new System.InvalidOperationException(
                "Unable to access the CLR method to wrap through reflection. Make sure it is a public instance method.");
        }

        return wrap;
    }

    public Task<Object> Call(Object payload)
    {
        return (Task<Object>)this.invokeMethod.Invoke(this.instance, new object[] { payload });
    }
};

// vim: ts=4 sw=4 et: 
