#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;
Persistent<Function> bufferConstructor;

NAN_METHOD(initializeClrFunc)
{
    return ClrFunc::Initialize(args);
}

void init(Handle<Object> target)
{
    DBG("edge::init");
    V8SynchronizationContext::Initialize();

    NanAssignPersistent(bufferConstructor, Local<Function>::Cast(
        NanGetCurrentContext()->Global()->Get(NanNew<String>("Buffer"))));

    debugMode = (0 < GetEnvironmentVariable("EDGE_DEBUG", NULL, 0));
    enableScriptIgnoreAttribute = (0 < GetEnvironmentVariable("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE", NULL, 0));
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);
