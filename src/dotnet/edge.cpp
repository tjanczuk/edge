#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

NAN_METHOD(initializeClrFunc)
{
    ClrFunc::Initialize(info);
}

#pragma unmanaged
NAN_MODULE_INIT(init)
{
    debugMode = (0 < GetEnvironmentVariable("EDGE_DEBUG", NULL, 0));
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
    enableScriptIgnoreAttribute = (0 < GetEnvironmentVariable("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE", NULL, 0));
    Nan::Set(target,
        Nan::New<v8::String>("initializeClrFunc").ToLocalChecked(),
        Nan::New<v8::FunctionTemplate>(initializeClrFunc)->GetFunction());
}

#pragma unmanaged
NODE_MODULE(edge, init);
