#include "edge_common.h"

// The Callback to use to force the next tick to happen
Nan::Callback* CallbackHelper::tickCallback;

static void NoOpFunction(const Nan::FunctionCallbackInfo<v8::Value>& info)
{
	// Do nothing, this is a no-op function
}

// Initialize the callback
void CallbackHelper::Initialize()
{
    DBG("CallbackHelper::Initialize");

    tickCallback = new Nan::Callback(Nan::New<v8::Function>(NoOpFunction, Nan::Null()));
}

// Make the no-op callback, forcing the next tick to execute
void CallbackHelper::KickNextTick()
{
	Nan::HandleScope scope;
	tickCallback->Call(0, 0);
}