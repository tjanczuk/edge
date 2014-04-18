using System;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

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
