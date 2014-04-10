using System;
using System.Dynamic;
using System.Threading.Tasks;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Collections;
using System.Collections.Generic;

public static class MonoEmbedding
{
    static Int64 MinDateTimeTicks = 621355968000000000; // (new DateTime(1970, 1, 1, 0, 0, 0)).Ticks;

    // dummy Main so we can exec from mono
    static public int Main()
    {
        return 0;
    }

    static public Type GetIDictionaryStringObjectType() 
    {
        return typeof(IDictionary<string, Object>);
    }

    static public Type GetUriType() 
    {
        return typeof(Uri);
    }

    static public Func<Object, Task<Object>> GetFunc(string assemblyFile, string typeName, string methodName)
    {
        var assembly = Assembly.LoadFrom(assemblyFile);
        var wrap = ClrFuncReflectionWrap.Create(assembly, typeName, methodName);
        return new Func<Object, Task<Object>>(wrap.Call);
    }

    static public DateTime CreateDateTime(double ticks) 
    {
        return new DateTime((Int64)ticks * 10000 + MinDateTimeTicks, DateTimeKind.Utc);
    }

    static public ExpandoObject CreateExpandoObject()
    {
        return new ExpandoObject();
    }

    static public double GetDateValue(DateTime dt)
    {
        if (dt.Kind == DateTimeKind.Local)
            dt = dt.ToUniversalTime();
        else if (dt.Kind == DateTimeKind.Unspecified)
            dt = new DateTime(dt.Ticks, DateTimeKind.Utc);
        Int64 value = (dt.Ticks - MinDateTimeTicks) / 10000;

        return (double)value;        
    }

    static public object[] IDictionaryToFlatArray(object dictionary)
    {
        var list = new List<object>();
        IDictionary<string, object> dso = dictionary as IDictionary<string, object>;
        if (dso != null)
        {
            foreach (var kv in dso) 
            {
                list.Add(kv.Key);
                list.Add(kv.Value);
            }
        }
        else 
        {
            System.Collections.IDictionary d = dictionary as System.Collections.IDictionary;
            if (d != null)
            {
                foreach (System.Collections.DictionaryEntry kv in d) 
                {
                    if ((kv.Key as string) != null)
                    {
                        list.Add(kv.Key);
                        list.Add(kv.Value);
                    }
                }
            }
            else 
            {
                throw new InvalidOperationException("Expected a dictionary.");
            }
        }

        return list.ToArray();
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

    static public string ObjectToString(object o)
    {
        return o.ToString();
    }

    static public double Int64ToDouble(long i64)
    {
        return (double)i64;
    }

    static public string TryConvertPrimitiveOrDecimal(object obj)
    {
        Type t = obj.GetType();
        if (t.IsPrimitive || typeof(Decimal) == t)
        {
            IConvertible c = obj as IConvertible;
            return c == null ? obj.ToString() : c.ToString();
        }
        else 
        {
            return null;
        }
    }
}

class ClrFuncInvokeContext
{
    IntPtr native;
    Object Payload;
    Task<Object> Task;
    bool Sync;

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
};

class NodejsFunc
{
    public IntPtr NativeNodejsFunc { get; set; }

    public NodejsFunc(IntPtr native)
    {
        this.NativeNodejsFunc = native;
    }

    ~NodejsFunc()
    {
        IntPtr native = this.NativeNodejsFunc;
        ExecuteActionOnV8Thread(() => {
            Release(native);
        });
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void Release(IntPtr nativeNodejsFunc); 

    Func<object, Task<object>> GetFunc()
    {
        return new Func<object, Task<object>>(this.FunctionWrapper);
    }

    Task<object> FunctionWrapper(object payload)
    {
        NodejsFuncInvokeContext ctx = new NodejsFuncInvokeContext(this, payload);
        ExecuteActionOnV8Thread(ctx.CallFuncOnV8Thread);

        return ctx.TaskCompletionSource.Task;
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void ExecuteActionOnV8Thread(Action action);    
};

class NodejsFuncInvokeContext {
    NodejsFunc functionContext;
    object payload;

    public TaskCompletionSource<object> TaskCompletionSource { get; set; }

    public NodejsFuncInvokeContext(NodejsFunc functionContext, object payload)
    {
        this.functionContext = functionContext;
        this.payload = payload;
        this.TaskCompletionSource = new TaskCompletionSource<object>();
    }

    public void CallFuncOnV8Thread()
    {
        // This function must run on V8 thread
        CallFuncOnV8ThreadInternal(
            this, this.functionContext.NativeNodejsFunc, this.payload);
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void CallFuncOnV8ThreadInternal(NodejsFuncInvokeContext _this, IntPtr nativeNodejsFunc, object payload);        

    public void Complete(object exception, object result)
    {
        Task.Run(() => {
            if (exception != null)
            {
                var e = exception as Exception;
                var s = exception as string;
                if (e != null)
                {
                    this.TaskCompletionSource.SetException(e);
                }
                else if (!string.IsNullOrEmpty(s))
                {
                    this.TaskCompletionSource.SetException(new Exception(s));
                }
                else
                {
                    this.TaskCompletionSource.SetException(
                        new InvalidOperationException("Unrecognized exception received from JavaScript."));
                }
            }
            else
            {
                this.TaskCompletionSource.SetResult(result);
            }
        });
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
