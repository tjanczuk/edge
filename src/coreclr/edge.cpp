#include "edge.h"

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

//NAN_METHOD(initializeClrFunc)
//{
//}

void init(Handle<Object> target) 
{
    debugMode = getenv("EDGE_DEBUG") != NULL;
    DBG("edge::init");
    V8SynchronizationContext::Initialize();

    if (FAILED(CoreClrEmbedding::Initialize()))
    {
    	DBG("Error occurred during CoreCLR initialization");
    	return;
    }
    
    enableScriptIgnoreAttribute = getenv("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE") != NULL;
    //NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);

// vim: ts=4 sw=4 et: 
