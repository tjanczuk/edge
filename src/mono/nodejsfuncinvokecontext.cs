using System;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

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
