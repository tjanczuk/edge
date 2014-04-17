#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;
Persistent<Function> bufferConstructor;

Handle<Value> initializeClrFunc(const v8::Arguments& args)
{
    return ClrFunc::Initialize(args);
}

void init(Handle<Object> target) 
{
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
    MonoEmbedding::Initialize();
    bufferConstructor = Persistent<Function>::New(Handle<Function>::Cast(
        Context::GetCurrent()->Global()->Get(String::New("Buffer")))); 
    debugMode = getenv("EDGE_DEBUG") != NULL;
    enableScriptIgnoreAttribute = getenv("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE") != NULL;
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);

// vim: ts=4 sw=4 et: 
