#include "edge.h"

BOOL debugMode;
Persistent<Function> bufferConstructor;

Handle<Value> initializeClrFunc(const v8::Arguments& args)
{
    return ClrFunc::Initialize(args);
}

void init(Handle<Object> target) 
{
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
    bufferConstructor = Persistent<Function>::New(Handle<Function>::Cast(
        Context::GetCurrent()->Global()->Get(String::New("Buffer")))); 
    debugMode = (0 < GetEnvironmentVariable("EDGE_DEBUG", NULL, 0));
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);
