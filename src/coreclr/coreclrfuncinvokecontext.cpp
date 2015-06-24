#include "edge.h"

CoreClrFuncInvokeContext::CoreClrFuncInvokeContext(Handle<v8::Value> callback, void* task) : task(task), uv_edge_async(NULL), resultData(NULL), resultType(0)
{
    DBG("CoreClrFuncInvokeContext::CoreClrFuncInvokeContext");

	this->callback = new Persistent<Function>();
	NanAssignPersistent(*(this->callback), Handle<Function>::Cast(callback));
}

CoreClrFuncInvokeContext::~CoreClrFuncInvokeContext()
{
	DBG("CoreClrFuncInvokeContext::~CoreClrFuncInvokeContext");

    if (this->callback)
    {
        NanDisposePersistent(*(this->callback));
        delete this->callback;
        this->callback = NULL;
    }

    if (this->task)
    {
    	CoreClrEmbedding::FreeHandle(this->task);
    	this->task = NULL;
    }

    if (this->resultData)
    {
    	// TODO: free marshal data
    }
}

void CoreClrFuncInvokeContext::InitializeAsyncOperation()
{
	DBG("CoreClrFuncInvokeContext::InitializeAsyncOperation");
    this->uv_edge_async = V8SynchronizationContext::RegisterAction(CoreClrFuncInvokeContext::InvokeCallback, this);
}

void CoreClrFuncInvokeContext::TaskComplete(void* result, int resultType, CoreClrFuncInvokeContext* context)
{
	DBG("CoreClrFuncInvokeContext::TaskComplete");

	context->resultData = result;
	context->resultType = resultType;

	V8SynchronizationContext::ExecuteAction(context->uv_edge_async);
}

void CoreClrFuncInvokeContext::InvokeCallback(void* data)
{
	DBG("CoreClrFuncInvokeContext::InvokeCallback");

	CoreClrFuncInvokeContext* context = (CoreClrFuncInvokeContext*)data;

	// TODO: pass errors if provided
	Handle<Value> argv[] = { NanUndefined(), CoreClrFunc::MarshalCLRToV8(context->resultData, context->resultType) };
	int argc = 2;

	TryCatch tryCatch;
	NanNew<v8::Function>(*(context->callback))->Call(NanGetCurrentContext()->Global(), argc, argv);
	delete context;

	if (tryCatch.HasCaught())
	{
		node::FatalException(tryCatch);
	}
}
