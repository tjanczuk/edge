#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;
Persistent<Function> bufferConstructor;

void initializeClrFunc(const v8::Arguments& args)
{
    args.GetReturnValue().Set(ClrFunc::Initialize(args));
}

void init(Handle<Object> target) 
{
    DBG("edge::init");
    V8SynchronizationContext::Initialize();

    bufferConstructor.Reset(v8::Isolate::GetCurrent(), Handle<Function>::Cast(
        Context::GetCurrent()->Global()->Get(String::New("Buffer"))));

    debugMode = (0 < GetEnvironmentVariable("EDGE_DEBUG", NULL, 0));
    enableScriptIgnoreAttribute = (0 < GetEnvironmentVariable("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE", NULL, 0));
    NODE_SET_METHOD(target, "initializeClrFunc", reinterpret_cast<v8::FunctionCallback>(initializeClrFunc));
}

NODE_MODULE(edge, init);
