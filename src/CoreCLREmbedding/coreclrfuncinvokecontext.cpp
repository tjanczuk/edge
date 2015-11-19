#include "edge.h"

CoreClrFuncInvokeContext::CoreClrFuncInvokeContext(v8::Local<v8::Value> callback, void* task) : task(task), uv_edge_async(NULL), resultData(NULL), resultType(0)
{
    DBG("CoreClrFuncInvokeContext::CoreClrFuncInvokeContext");

	this->callback = new Nan::Persistent<Function>();
	v8::Local<v8::Function> callbackFunction = v8::Local<v8::Function>::Cast(callback);
    this->callback->Reset(callbackFunction);
}

CoreClrFuncInvokeContext::~CoreClrFuncInvokeContext()
{
	DBG("CoreClrFuncInvokeContext::~CoreClrFuncInvokeContext");

    if (this->callback)
    {
        this->callback->Reset();
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
    	CoreClrEmbedding::FreeMarshalData(this->resultData, this->resultType);
    	this->resultData = NULL;
    }
}

void CoreClrFuncInvokeContext::InitializeAsyncOperation()
{
	DBG("CoreClrFuncInvokeContext::InitializeAsyncOperation");
    this->uv_edge_async = V8SynchronizationContext::RegisterAction(CoreClrFuncInvokeContext::InvokeCallback, this);
}

void CoreClrFuncInvokeContext::TaskComplete(void* result, int resultType, int taskState, CoreClrFuncInvokeContext* context)
{
	DBG("CoreClrFuncInvokeContext::TaskComplete");

	context->resultData = result;
	context->resultType = resultType;
	context->taskState = taskState;

	V8SynchronizationContext::ExecuteAction(context->uv_edge_async);
}

void CoreClrFuncInvokeContext::TaskCompleteSynchronous(void* result, int resultType, int taskState, v8::Local<v8::Value> callback)
{
	DBG("CoreClrFuncInvokeContext::TaskCompleteSynchronous");

	CoreClrFuncInvokeContext* context = new CoreClrFuncInvokeContext(callback, NULL);

	context->resultData = result;
	context->resultType = resultType;
	context->taskState = taskState;

	InvokeCallback(context);
}

void CoreClrFuncInvokeContext::InvokeCallback(void* data)
{
	DBG("CoreClrFuncInvokeContext::InvokeCallback");

	CoreClrFuncInvokeContext* context = (CoreClrFuncInvokeContext*)data;
	v8::Local<v8::Value> callbackData = Nan::Null();
	v8::Local<v8::Value> errors = Nan::Null();

	if (context->taskState == TaskStatusFaulted)
	{
		errors = CoreClrFunc::MarshalCLRToV8(context->resultData, context->resultType);
	}

	else
	{
		callbackData = CoreClrFunc::MarshalCLRToV8(context->resultData, context->resultType);
	}

	DBG("CoreClrFuncInvokeContext::InvokeCallback - Marshalling complete");

	v8::Local<Value> argv[] = { errors, callbackData };
	int argc = 2;	

	Nan::TryCatch tryCatch;
	Nan::New<v8::Function>(*(context->callback))->Call(Nan::GetCurrentContext()->Global(), argc, argv);

	DBG("CoreClrFuncInvokeContext::InvokeCallback - Callback function invoked");
	delete context;

	if (tryCatch.HasCaught())
	{
		Nan::FatalException(tryCatch);
	}

	DBG("CoreClrFuncInvokeContext::InvokeCallback - Complete");
}
