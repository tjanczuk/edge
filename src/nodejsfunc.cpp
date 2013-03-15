#include "edge.h"

NodejsFunc::NodejsFunc(ClrFuncInvokeContext^ appInvokeContext, Handle<Function> function)
{
    DBG("NodejsFunc::NodejsFunc");
    this->ClrInvokeContext = appInvokeContext;
    this->Func = new Persistent<Function>;
    *(this->Func) = Persistent<Function>::New(function);
    // transfer pointer ownership to appInvokeContext
    appInvokeContext->AddPersistentHandle((Persistent<Value>*)this->Func); 
}

Task<System::Object^>^ NodejsFunc::FunctionWrapper(System::Object^ payload)
{
    DBG("NodejsFunc::FunctionWrapper");
    uv_edge_async_t* uv_edge_async = this->ClrInvokeContext->WaitForUvEdgeAsyncFunc();
    NodejsFuncInvokeContext^ context = gcnew NodejsFuncInvokeContext(this, payload);
    uv_edge_async->context = context;
    BOOL ret = PostQueuedCompletionStatus(
        uv_default_loop()->iocp, 
        0, 
        (ULONG_PTR)NULL, 
        &uv_edge_async->uv_async.async_req.overlapped);

    return context->TaskCompletionSource->Task;
}
