#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

NAN_METHOD(initializeClrFunc)
{
    ClrFunc::Initialize(info);
}

NAN_MODULE_INIT(init) 
{
    debugMode = getenv("EDGE_DEBUG") != NULL;
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
    MonoEmbedding::Initialize();
    enableScriptIgnoreAttribute = getenv("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE") != NULL;
    Nan::Set(target,
        Nan::New<v8::String>("initializeClrFunc").ToLocalChecked(),
        Nan::New<v8::FunctionTemplate>(initializeClrFunc)->GetFunction());
}

NODE_MODULE(edge, init);
