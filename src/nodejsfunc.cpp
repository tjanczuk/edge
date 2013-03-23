#include "edge.h"

NodejsFunc::NodejsFunc(ClrFuncInvokeContext^ appInvokeContext, Handle<Function> function)
{
    DBG("NodejsFunc::NodejsFunc");
    this->Func = new Persistent<Function>;
    *(this->Func) = Persistent<Function>::New(function);
    // transfer pointer ownership to appInvokeContext
    appInvokeContext->AddPersistentHandle((Persistent<Value>*)this->Func); 
}

Task<System::Object^>^ NodejsFunc::FunctionWrapper(System::Object^ payload)
{
    DBG("NodejsFunc::FunctionWrapper");
    NodejsFuncInvokeContext^ context = gcnew NodejsFuncInvokeContext(this, payload);
    uv_edge_async_t* uv_edge_async = V8SynchronizationContext::RegisterAction(
        gcnew System::Action(context, &NodejsFuncInvokeContext::CallFuncOnV8Thread));
    V8SynchronizationContext::ExecuteAction(uv_edge_async);

    return context->TaskCompletionSource->Task;
}
