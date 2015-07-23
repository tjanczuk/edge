#include "edge.h"

CoreClrNodejsFunc::CoreClrNodejsFunc(Handle<Function> function)
{
    DBG("CoreClrNodejsFunc::CoreClrNodejsFunc");

    this->Func = new Persistent<Function>;
    NanAssignPersistent(*(this->Func), function);
}

CoreClrNodejsFunc::~CoreClrNodejsFunc()
{
    DBG("CoreClrNodejsFunc::~CoreClrNodejsFunc");
    NanDisposePersistent(*(this->Func));
    delete this->Func;
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
