#include "../coreclr/edge.h"
#ifdef HAVE_MONO
#include "../mono/edge.h"
#endif

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

NAN_METHOD(initializeClrFunc)
{
#ifdef HAVE_MONO
	if (getenv("EDGE_USE_CORECLR") == NULL)
	{
		return ClrFunc::Initialize(args);
	}

#endif
	return CoreClrEmbedding::LoadFunction(args);
}

void init(Handle<Object> target)
{
    debugMode = getenv("EDGE_DEBUG") != NULL;
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
#ifdef HAVE_MONO
    if (getenv("EDGE_USE_CORECLR") == NULL)
    {
    	MonoEmbedding::Initialize();
    }

    else
#endif

    if (FAILED(CoreClrEmbedding::Initialize()))
    {
    	DBG("Error occurred during CoreCLR initialization");
    	return;
    }

    enableScriptIgnoreAttribute = getenv("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE") != NULL;
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);

// vim: ts=4 sw=4 et:
