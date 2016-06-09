using System;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

public class NodejsFunc
{
    public IntPtr Context 
    { 
        get; 
        private set; 
    }

    public NodejsFunc(IntPtr context)
    {
        Context = context;
    }

    ~NodejsFunc()
    {
        // TODO: implement release
    }

    public Func<object, Task<object>> GetFunc()
    {
        return new Func<object, Task<object>>(FunctionWrapper);
    }

    private Task<object> FunctionWrapper(object payload)
    {
        CoreCLREmbedding.DebugMessage("NodejsFunc::FunctionWrapper (CLR) - Started");

        NodejsFuncInvokeContext invokeContext = new NodejsFuncInvokeContext(this, payload);
        invokeContext.CallFunc();

        CoreCLREmbedding.DebugMessage("NodejsFunc::FunctionWrapper (CLR) - Node.js function invoked on the V8 thread, returning the task completion source");

        return invokeContext.TaskCompletionSource.Task;
    }  
}
