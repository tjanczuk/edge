#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

NAN_METHOD(initializeClrFunc)
{
    return ClrFunc::Initialize(args);
}

void init(Handle<Object> target) 
{
    debugMode = getenv("EDGE_DEBUG") != NULL;
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
    MonoEmbedding::Initialize();
    enableScriptIgnoreAttribute = getenv("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE") != NULL;
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);

// vim: ts=4 sw=4 et: 
