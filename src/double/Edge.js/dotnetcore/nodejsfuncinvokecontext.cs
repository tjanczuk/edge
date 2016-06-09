using System;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

public delegate void NodejsFuncCompleteDelegate(IntPtr context, int taskStatus, IntPtr result, int resultType);

public class NodejsFuncInvokeContext 
{
    private NodejsFunc functionContext;
    private object payload;
    private static IntPtr NodejsFuncCompleteCallback;
    private static NodejsFuncCompleteDelegate NodeJsFuncCompleteInstance;

    static NodejsFuncInvokeContext()
    {
        NodeJsFuncCompleteInstance = new NodejsFuncCompleteDelegate(NodejsFuncComplete);
        NodejsFuncCompleteCallback = Marshal.GetFunctionPointerForDelegate<NodejsFuncCompleteDelegate>(NodeJsFuncCompleteInstance);
    }

    internal static CallV8FunctionDelegate CallV8Function
    {
        get;
        set;
    }

    public TaskCompletionSource<object> TaskCompletionSource 
    { 
        get; 
        private set; 
    }

    public NodejsFuncInvokeContext(NodejsFunc functionContext, object payload)
    {
        this.functionContext = functionContext;
        this.payload = payload;
        this.TaskCompletionSource = new TaskCompletionSource<object>();
    }

    internal void CallFunc()
    {
        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::CallFunc (CLR) - Starting");

        V8Type v8PayloadType;
        IntPtr v8Payload = CoreCLREmbedding.MarshalCLRToV8(payload, out v8PayloadType);

        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::CallFunc (CLR) - Marshalled payload data for V8");

        IntPtr callbackContext = GCHandle.ToIntPtr(GCHandle.Alloc(this));
        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::CallFunc (CLR) - Getting a GCHandle for this context object");

        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::CallFunc (CLR) - Invoking the unmanaged V8 function to invoke the Node.js function on the V8 thread");
        CallV8Function(v8Payload, (int)v8PayloadType, functionContext.Context, callbackContext, NodejsFuncCompleteCallback);
    }      

    public static void NodejsFuncComplete(IntPtr context, int taskStatus, IntPtr result, int resultType)
    {
        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::NodejsFuncComplete (CLR) - Starting");

        GCHandle gcHandle = GCHandle.FromIntPtr(context);
        NodejsFuncInvokeContext actualContext = (NodejsFuncInvokeContext)gcHandle.Target;

        gcHandle.Free();

        V8Type v8ResultType = (V8Type)resultType;
        object marshalledResult = CoreCLREmbedding.MarshalV8ToCLR(result, v8ResultType);

        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::NodejsFuncComplete (CLR) - Marshalled result data back to the CLR");

        if (taskStatus == (int)TaskStatus.Faulted)
        {
            actualContext.TaskCompletionSource.SetException((Exception)marshalledResult);
            CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::NodejsFuncComplete (CLR) - Set the exception received from Node.js: {0}", ((Exception)marshalledResult).Message);
        }

        else
        {
            actualContext.TaskCompletionSource.SetResult(marshalledResult);
            CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::NodejsFuncComplete (CLR) - Set result data");
        }

        CoreCLREmbedding.DebugMessage("NodejsFuncInvokeContext::NodejsFuncComplete (CLR) - Finished");
    }
}