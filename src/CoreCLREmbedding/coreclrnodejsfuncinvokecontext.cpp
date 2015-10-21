#include "edge.h"

NAN_METHOD(coreClrV8FuncCallback)
{
    DBG("coreClrV8FuncCallback");

    Nan::HandleScope scope;
    v8::Local<v8::External> correlator = v8::Local<v8::External>::Cast(info[2]);
    CoreClrNodejsFuncInvokeContext* context = (CoreClrNodejsFuncInvokeContext*)(correlator->Value());

    if (!info[0]->IsUndefined() && !info[0]->IsNull())
    {
    	void* clrExceptionData;
    	CoreClrFunc::MarshalV8ExceptionToCLR(info[0], &clrExceptionData);

        context->Complete(TaskStatusFaulted, clrExceptionData, V8TypeException);
    }

    else
    {
    	void* marshalData;
    	int payloadType;

        CoreClrFunc::MarshalV8ToCLR(info[1], &marshalData, &payloadType);
        context->Complete(TaskStatusRanToCompletion, marshalData, payloadType);
    }

    info.GetReturnValue().SetUndefined();
}

CoreClrNodejsFuncInvokeContext::CoreClrNodejsFuncInvokeContext(void* payload, int payloadType, CoreClrNodejsFunc* functionContext, CoreClrGcHandle callbackContext, NodejsFuncCompleteFunction callbackFunction)
{
	DBG("CoreClrNodejsFuncInvokeContext::CoreClrNodejsFuncInvokeContext");

	uv_edge_async = NULL;
	Payload = payload;
	PayloadType = payloadType;
	FunctionContext = functionContext;
	CallbackContext = callbackContext;
	CallbackFunction = callbackFunction;
}

CoreClrNodejsFuncInvokeContext::~CoreClrNodejsFuncInvokeContext()
{
	if (uv_edge_async)
	{
		delete uv_edge_async;
	}

	if (Payload)
	{
		CoreClrFunc::FreeMarshalData(Payload, PayloadType);
	}
}

void CoreClrNodejsFuncInvokeContext::Complete(TaskStatus taskStatus, void* result, int resultType)
{
	DBG("CoreClrNodejsFuncInvokeContext::Complete");
	CallbackFunction(CallbackContext, taskStatus, result, resultType);
}

void CoreClrNodejsFuncInvokeContext::Invoke()
{
	this->uv_edge_async = V8SynchronizationContext::RegisterAction(CoreClrNodejsFuncInvokeContext::InvokeCallback, this);
	V8SynchronizationContext::ExecuteAction(uv_edge_async);
}

void CoreClrNodejsFuncInvokeContext::InvokeCallback(void* data)
{
	DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback");

	CoreClrNodejsFuncInvokeContext* context = (CoreClrNodejsFuncInvokeContext*) data;
	v8::Local<v8::Value> v8Payload = CoreClrFunc::MarshalCLRToV8(context->Payload, context->PayloadType);

	static Nan::Persistent<v8::Function> callbackFactory;
	static Nan::Persistent<v8::Function> callbackFunction;

	Nan::HandleScope scope;

	// See https://github.com/tjanczuk/edge/issues/125 for context
	if (callbackFactory.IsEmpty())
	{
		v8::Local<v8::Function> v8FuncCallbackFunction = Nan::New<v8::FunctionTemplate>(coreClrV8FuncCallback)->GetFunction();
		callbackFunction.Reset(v8FuncCallbackFunction);
		v8::Local<v8::String> code = Nan::New<v8::String>(
			"(function (cb, ctx) { return function (e, d) { return cb(e, d, ctx); }; })").ToLocalChecked();
		v8::Local<v8::Function> callbackFactoryFunction = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
		callbackFactory.Reset(callbackFactoryFunction);
	}

	v8::Local<v8::Value> factoryArgv[] = { Nan::New(callbackFunction), Nan::New<v8::External>((void*)context) };
	v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(
		Nan::New(callbackFactory)->Call(Nan::GetCurrentContext()->Global(), 2, factoryArgv));

	v8::Local<v8::Value> argv[] = { v8Payload, callback };
	TryCatch tryCatch;

	DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Calling JavaScript function");
	Nan::New(*(context->FunctionContext->Func))->Call(Nan::GetCurrentContext()->Global(), 2, argv);
	DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Called JavaScript function");

	if (tryCatch.HasCaught())
	{
		DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Caught JavaScript exception");

		void* exceptionData;
		CoreClrFunc::MarshalV8ExceptionToCLR(tryCatch.Exception(), &exceptionData);

		DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Exception message is: %s", (char*)exceptionData);

		context->Complete(TaskStatusFaulted, exceptionData, V8TypeException);
	}
}
