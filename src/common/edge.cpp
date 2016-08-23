#if !defined(HAVE_CORECLR) && !defined(HAVE_NATIVECLR)
#error "CoreCLR and/or a native .NET runtime (native .NET on Windows or Mono on Linux) must be installed in order for Edge.js to compile."
#endif

#include "edge_common.h"

#ifdef HAVE_CORECLR
#include "../CoreCLREmbedding/edge.h"
#endif
#ifdef HAVE_NATIVECLR
#ifdef EDGE_PLATFORM_WINDOWS
#include "../dotnet/edge.h"
#else
#include "../mono/edge.h"
#endif
#endif

BOOL debugMode;
BOOL enableScriptIgnoreAttribute;
BOOL enableMarshalEnumAsInt;

NAN_METHOD(initializeClrFunc)
{
#ifdef HAVE_NATIVECLR
#ifdef HAVE_CORECLR
	if (HasEnvironmentVariable("EDGE_USE_CORECLR"))
	{
		CoreClrFunc::Initialize(info);
	}

#endif
	ClrFunc::Initialize(info);
#else
	CoreClrFunc::Initialize(info);
#endif
}

#ifdef EDGE_PLATFORM_WINDOWS
#pragma unmanaged
#endif
NAN_MODULE_INIT(init)
{
    debugMode = HasEnvironmentVariable("EDGE_DEBUG");
    DBG("edge::init");

    V8SynchronizationContext::Initialize();

#ifdef HAVE_CORECLR
    if (FAILED(CoreClrEmbedding::Initialize(debugMode)))
	{
		DBG("Error occurred during CoreCLR initialization");
		return;
	}
#else
#ifndef EDGE_PLATFORM_WINDOWS
    MonoEmbedding::Initialize();
#endif
#endif

    enableScriptIgnoreAttribute = HasEnvironmentVariable("EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE");
    enableMarshalEnumAsInt = HasEnvironmentVariable("EDGE_MARSHAL_ENUM_AS_INT");
    Nan::Set(target,
        Nan::New<v8::String>("initializeClrFunc").ToLocalChecked(),
        Nan::New<v8::FunctionTemplate>(initializeClrFunc)->GetFunction());
}

#ifdef EDGE_PLATFORM_WINDOWS
#pragma unmanaged
#endif
bool HasEnvironmentVariable(const char* variableName)
{
#ifdef EDGE_PLATFORM_WINDOWS
    return 0 < GetEnvironmentVariable(variableName, NULL, 0);
#else
    return getenv(variableName) != NULL;
#endif
}

#ifdef EDGE_PLATFORM_WINDOWS
#pragma unmanaged
#endif
#ifdef HAVE_CORECLR
NODE_MODULE(edge_coreclr, init);
#else
NODE_MODULE(edge_nativeclr, init);
#endif

// vim: ts=4 sw=4 et:
