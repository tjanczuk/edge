#include "edge.h"

CoreClrFunc::CoreClrFunc()
{
	func = NULL;
}

NAN_METHOD(coreClrFuncProxy)
{
    DBG("coreClrFuncProxy");
    NanEscapableScope();
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
    CoreClrFuncWrap* wrap = (CoreClrFuncWrap*)(correlator->Value());
    CoreClrFunc* clrFunc = wrap->clrFunc;
    NanReturnValue(clrFunc->Call(args[0], args[1]));
}

NAN_WEAK_CALLBACK(coreClrFuncProxyNearDeath)
{
    DBG("coreClrFuncProxyNearDeath");
    CoreClrFuncWrap* wrap = (CoreClrFuncWrap*)(data.GetParameter());
    wrap->clrFunc = NULL;
    delete wrap;
}

Handle<v8::Function> CoreClrFunc::InitializeInstance(InvokeFuncFunction func)
{
    DBG("CoreClrFunc::InitializeInstance");

    static Persistent<v8::Function> proxyFactory;
    static Persistent<v8::Function> proxyFunction;

    NanEscapableScope();

    CoreClrFunc* app = new CoreClrFunc();
    app->func = func;
    CoreClrFuncWrap* wrap = new CoreClrFuncWrap;
    wrap->clrFunc = app;

    // See https://github.com/tjanczuk/edge/issues/128 for context

    if (proxyFactory.IsEmpty())
    {
        NanAssignPersistent(
            proxyFunction, NanNew<FunctionTemplate>(coreClrFuncProxy)->GetFunction());
        Handle<v8::String> code = NanNew<v8::String>(
            "(function (f, ctx) { return function (d, cb) { return f(d, cb, ctx); }; })");
        NanAssignPersistent(
            proxyFactory, Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
    }

    Handle<v8::Value> factoryArgv[] = { NanNew(proxyFunction), NanNew<v8::External>((void*)wrap) };
    Local<v8::Function> funcProxy =
            (NanNew(proxyFactory)->Call(NanGetCurrentContext()->Global(), 2, factoryArgv)).As<v8::Function>();
    NanMakeWeakPersistent(funcProxy, (void*)wrap, &coreClrFuncProxyNearDeath);

    return NanEscapeScope(funcProxy);
}

Handle<v8::Value> CoreClrFunc::Call(Handle<v8::Value> payload, Handle<v8::Value> callback)
{
	DBG("CoreClrFunc::Call instance");
	NanEscapableScope();

	void* marshalData;
	int payloadType;

	MarshalV8ToCLR(payload, &marshalData, &payloadType);
	CoreClrEmbedding::CallClrFunc(func, marshalData, payloadType);
	FreeMarshalData(marshalData, payloadType);

	return NanEscapeScope(NanUndefined());
}

NAN_METHOD(CoreClrFunc::Initialize)
{
	NanEscapableScope();
	Handle<v8::Object> options = args[0]->ToObject();
	Handle<v8::Function> result;

	Handle<v8::Value> assemblyFileArgument = options->Get(NanNew<v8::String>("assemblyFile"));

	if (assemblyFileArgument->IsString())
	{
		v8::String::Utf8Value assemblyFile(assemblyFileArgument);
		v8::String::Utf8Value typeName(options->Get(NanNew<v8::String>("typeName")));
		v8::String::Utf8Value methodName(options->Get(NanNew<v8::String>("methodName")));

		DBG("Loading %s.%s() from %s", *typeName, *methodName, *assemblyFile);
		InvokeFuncFunction func = CoreClrEmbedding::GetClrFuncReflectionWrapFunc(*assemblyFile, *typeName, *methodName);
		DBG("Function loaded successfully")

		result = CoreClrFunc::InitializeInstance(func);
		DBG("Callback initialized successfully")
	}

	else
	{
		// TODO: support compilation from source once the Roslyn C# compiler is made available on CoreCLR
		NanThrowError("Compiling .NET methods from source is not yet supported with CoreCLR, you must provide an assembly path, type name, and method name as arguments to edge.initializeClrFunction().");
	}

	NanReturnValue(result);
}

void CoreClrFunc::FreeMarshalData(void* marshalData, int payloadType)
{
	switch (payloadType)
	{
		case JsPropertyType::PropertyTypeString:
			delete ((char*)marshalData);
			break;

		case JsPropertyType::PropertyTypeObject:
			delete ((JsObjectData*)marshalData);
			break;

		case JsPropertyType::PropertyTypeBoolean:
			delete ((bool*)marshalData);
			break;

		case JsPropertyType::PropertyTypeNumber:
			delete ((double*)marshalData);
			break;
	}
}

char* CoreClrFunc::CopyV8StringBytes(Handle<v8::String> v8String)
{
	String::Utf8Value utf8String(v8String);
	char* sourceBytes = *utf8String;
	int sourceLength = strlen(sourceBytes);
	char* destinationBytes = new char[sourceLength + 1];

	strncpy(destinationBytes, sourceBytes, sourceLength);
	destinationBytes[sourceLength] = '\0';

	return destinationBytes;
}

void CoreClrFunc::MarshalV8ToCLR(Handle<v8::Value> jsdata, void** marshalData, int* payloadType)
{
	if (jsdata->IsString())
	{
		*marshalData = CopyV8StringBytes(Handle<v8::String>::Cast(jsdata));
		*payloadType = JsPropertyType::PropertyTypeString;
	}

	else if (jsdata->IsFunction())
	{
		// TODO: implement
	}

	else if (node::Buffer::HasInstance(jsdata))
	{
		// TODO: implement
	}

	else if (jsdata->IsArray())
	{
		// TODO: implement
	}

	else if (jsdata->IsDate())
	{
		// TODO: implement
	}

	else if (jsdata->IsBoolean())
	{
		bool* value = new bool();
		*value = jsdata->BooleanValue();

		*marshalData = value;
		*payloadType = JsPropertyType::PropertyTypeBoolean;
	}

	else if (jsdata->IsInt32())
	{
		// TODO: implement
	}

	else if (jsdata->IsUint32())
	{
		// TODO: implement
	}

	else if (jsdata->IsNumber())
	{
		double* value = new double();
		*value = jsdata->NumberValue();

		*marshalData = value;
		*payloadType = JsPropertyType::PropertyTypeNumber;
	}

	else if (jsdata->IsUndefined())
	{
		// TODO: implement
	}

	else if (jsdata->IsObject())
	{
		JsObjectData* objectData = new JsObjectData();

		Handle<v8::Object> jsobject = Handle<v8::Object>::Cast(jsdata);
		Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();

		objectData->propertiesCount = propertyNames->Length();
		objectData->propertyData = new void*[objectData->propertiesCount];
		objectData->propertyNames = new char*[objectData->propertiesCount];
		objectData->propertyTypes = new int[objectData->propertiesCount];

		for (unsigned int i = 0; i < propertyNames->Length(); i++)
		{
			Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));

			objectData->propertyNames[i] = CopyV8StringBytes(name);
			MarshalV8ToCLR(jsobject->Get(name), &objectData->propertyData[i], &objectData->propertyTypes[i]);
		}

		*marshalData = objectData;
		*payloadType = JsPropertyType::PropertyTypeObject;
	}
}
