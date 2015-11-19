#include "edge.h"

CoreClrNodejsFunc::CoreClrNodejsFunc(v8::Local<v8::Function> function)
{
    DBG("CoreClrNodejsFunc::CoreClrNodejsFunc");

    this->Func = new Nan::Persistent<v8::Function>;
    this->Func->Reset(function);
}

CoreClrNodejsFunc::~CoreClrNodejsFunc()
{
    DBG("CoreClrNodejsFunc::~CoreClrNodejsFunc");
	this->Func->Reset();
    delete this->Func;
	this->Func = NULL;
}

void CoreClrNodejsFunc::Release(CoreClrNodejsFunc* function)
{
	DBG("CoreClrNodejsFunc::Release");
	delete function;
}

void CoreClrNodejsFunc::Call(void* payload, int payloadType, CoreClrNodejsFunc* functionContext, CoreClrGcHandle callbackContext, NodejsFuncCompleteFunction callbackFunction)
{
	DBG("CoreClrNodejsFunc::Call");

	CoreClrNodejsFuncInvokeContext* invokeContext = new CoreClrNodejsFuncInvokeContext(payload, payloadType, functionContext, callbackContext, callbackFunction);
	invokeContext->Invoke();
}
