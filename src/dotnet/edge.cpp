#pragma unmanaged

#include "../common/edge_common.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

v8::Persistent<v8::Function,v8::CopyablePersistentTraits<v8::Function>> bufferConstructor;

// Handle<Value> 
void initializeClrFunc(const v8::FunctionCallbackInfo<Value>& args);

void exitCallback( void * arg )
{
	bufferConstructor.Reset();
}

void init(v8::Handle<v8::Object> target) 
{
	DBG("edge::init");
	V8SynchronizationContext::Initialize();
	Isolate * isolate = Isolate::GetCurrent();
	bufferConstructor = Persistent<Function, CopyablePersistentTraits<Function>>(isolate, Handle<Function>::Cast(
		isolate->GetCurrentContext()->Global()->Get(String::NewFromUtf8(isolate, "Buffer"))));
    debugMode = (0 < GetEnvironmentVariable("EDGE_DEBUG", NULL, 0));
    enableScriptIgnoreAttribute = (0 < GetEnvironmentVariable("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE", NULL, 0));
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
	node::AtExit(exitCallback, 0);
}

NODE_MODULE(edge, init)
