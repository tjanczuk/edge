#ifndef HAVE_CORECLR
#ifndef HAVE_MONO
#error "CoreCLR and/or Mono must be installed in order for Edge.js to compile."
#endif
#endif

#ifdef HAVE_CORECLR
#include "../coreclr/edge.h"
#endif
#ifdef HAVE_MONO
#include "../mono/edge.h"
#endif

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;

NAN_METHOD(initializeClrFunc)
{
#ifdef HAVE_MONO
#ifdef HAVE_CORECLR
	if (getenv("EDGE_USE_CORECLR") != NULL)
	{
		return CoreClrEmbedding::LoadFunction(args);
	}
#endif
	return ClrFunc::Initialize(args);
#else
	return CoreClrEmbedding::LoadFunction(args);
#endif
}

void init(Handle<Object> target)
{
    debugMode = getenv("EDGE_DEBUG") != NULL;
    DBG("edge::init");
    V8SynchronizationContext::Initialize();
#ifdef HAVE_MONO
#ifdef HAVE_CORECLR
    if (getenv("EDGE_USE_CORECLR") == NULL)
    {
    	MonoEmbedding::Initialize();
    }

    else if (FAILED(CoreClrEmbedding::Initialize(debugMode)))
    {
    	DBG("Error occurred during CoreCLR initialization");
    	return;
    }
#endif
#else
    if (FAILED(CoreClrEmbedding::Initialize(debugMode)))
	{
		DBG("Error occurred during CoreCLR initialization");
		return;
	}
#endif

    enableScriptIgnoreAttribute = getenv("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE") != NULL;
    NODE_SET_METHOD(target, "initializeClrFunc", initializeClrFunc);
}

NODE_MODULE(edge, init);

// vim: ts=4 sw=4 et:
