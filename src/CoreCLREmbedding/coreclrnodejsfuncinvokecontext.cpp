#include "edge.h"

NAN_METHOD(coreClrV8FuncCallback)
{
    DBG("coreClrV8FuncCallback");

    NanScope();
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
    CoreClrNodejsFuncInvokeContext* context = (CoreClrNodejsFuncInvokeContext*)(correlator->Value());

    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
    	void* clrExceptionData;
    	CoreClrFunc::MarshalV8ExceptionToCLR(args[0], &clrExceptionData);

        context->Complete(TaskStatus::Faulted, clrExceptionData, V8Type::PropertyTypeException);
    }

    else
    {
    	void* marshalData;
    	int payloadType;

        CoreClrFunc::MarshalV8ToCLR(args[1], &marshalData, &payloadType);
        context->Complete(TaskStatus::RanToCompletion, marshalData, payloadType);
    }

    NanReturnValue(NanUndefined());
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
	Handle<v8::Value> v8Payload = CoreClrFunc::MarshalCLRToV8(context->Payload, context->PayloadType);

	static Persistent<v8::Function> callbackFactory;
	static Persistent<v8::Function> callbackFunction;

	NanScope();

	// See https://github.com/tjanczuk/edge/issues/125 for context
	if (callbackFactory.IsEmpty())
	{
		NanAssignPersistent(
			callbackFunction,
			NanNew<FunctionTemplate>(coreClrV8FuncCallback)->GetFunction());
		Handle<v8::String> code = NanNew<v8::String>(
			"(function (cb, ctx) { return function (e, d) { return cb(e, d, ctx); }; })");
		NanAssignPersistent(
			callbackFactory,
			Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
	}

	Handle<v8::Value> factoryArgv[] = { NanNew(callbackFunction), NanNew<v8::External>((void*)context) };
	Handle<v8::Function> callback = Handle<v8::Function>::Cast(
		NanNew(callbackFactory)->Call(NanGetCurrentContext()->Global(), 2, factoryArgv));

	Handle<v8::Value> argv[] = { v8Payload, callback };
	TryCatch tryCatch;

	DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Calling JavaScript function");
	NanNew(*(context->FunctionContext->Func))->Call(NanGetCurrentContext()->Global(), 2, argv);
	DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Called JavaScript function");

	if (tryCatch.HasCaught())
	{
		DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Caught JavaScript exception");

		void* exceptionData;
		CoreClrFunc::MarshalV8ExceptionToCLR(tryCatch.Exception(), &exceptionData);

		DBG("CoreClrNodejsFuncInvokeContext::InvokeCallback - Exception message is: %s", (char*)exceptionData);

		context->Complete(TaskStatus::Faulted, exceptionData, V8Type::PropertyTypeException);
	}
}
