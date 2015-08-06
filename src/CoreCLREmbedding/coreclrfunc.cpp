#include "edge.h"

CoreClrFunc::CoreClrFunc()
{
	functionHandle = NULL;
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

Handle<v8::Function> CoreClrFunc::InitializeInstance(CoreClrGcHandle functionHandle)
{
    DBG("CoreClrFunc::InitializeInstance - Started");

    static Persistent<v8::Function> proxyFactory;
    static Persistent<v8::Function> proxyFunction;

    NanEscapableScope();

    CoreClrFunc* app = new CoreClrFunc();
    app->functionHandle = functionHandle;
    CoreClrFuncWrap* wrap = new CoreClrFuncWrap();
    wrap->clrFunc = app;

    // See https://github.com/tjanczuk/edge/issues/128 for context

    if (proxyFactory.IsEmpty())
    {
    	DBG("CoreClrFunc::InitializeInstance - Creating proxy factory");

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

    DBG("CoreClrFunc::InitializeInstance - Finished");

    return NanEscapeScope(funcProxy);
}

Handle<v8::Value> CoreClrFunc::Call(Handle<v8::Value> payload, Handle<v8::Value> callbackOrSync)
{
	DBG("CoreClrFunc::Call - Started");
	NanEscapableScope();

	void* marshalData;
	int payloadType;
	int taskState;
	void* result;
	int resultType;

	DBG("CoreClrFunc::Call - Marshalling data in preparation for calling the CLR");

	MarshalV8ToCLR(payload, &marshalData, &payloadType);
	DBG("CoreClrFunc::Call - Object type of %d is being marshalled", payloadType);

	DBG("CoreClrFunc::Call - Calling CoreClrEmbedding::CallClrFunc()");
	CoreClrEmbedding::CallClrFunc(functionHandle, marshalData, payloadType, &taskState, &result, &resultType);
	DBG("CoreClrFunc::Call - CoreClrEmbedding::CallClrFunc() returned a task state of %d", taskState);

	DBG("CoreClrFunc::Call - Freeing the data marshalled to the CLR");
	FreeMarshalData(marshalData, payloadType);

	if (taskState == TaskStatus::RanToCompletion || taskState == TaskStatus::Faulted)
	{
		if (taskState == TaskStatus::RanToCompletion)
		{
			DBG("CoreClrFunc::Call - Task ran synchronously, marshalling CLR data to V8");
		}

		else
		{
			DBG("CoreClrFunc::Call - Task threw an exception, marshalling CLR exception data to V8");
		}

		if (callbackOrSync->IsBoolean() || taskState == TaskStatus::Faulted)
		{
			if (taskState == TaskStatus::RanToCompletion)
			{
				return NanEscapeScope(CoreClrFunc::MarshalCLRToV8(result, resultType));
			}

			else
			{
				throwV8Exception(CoreClrFunc::MarshalCLRToV8(result, resultType));
			}
		}

		else
		{
			CoreClrFuncInvokeContext::TaskCompleteSynchronous(result, resultType, taskState, callbackOrSync);
		}
	}

	else if (callbackOrSync->IsBoolean())
	{
		DBG("CoreClrFunc::Call - Task was expected to run synchronously, but did not run to completion");
		throwV8Exception("The JavaScript function was called synchronously "
            "but the underlying CLR function returned without completing the Task. Call the "
            "JavaScript function asynchronously.");
	}

	else
	{
		DBG("CoreClrFunc::Call - Task running asynchronously, registering callback");

		CoreClrGcHandle taskHandle = result;
		CoreClrFuncInvokeContext* invokeContext = new CoreClrFuncInvokeContext(callbackOrSync, taskHandle);

		invokeContext->InitializeAsyncOperation();

		void* exception;
		CoreClrEmbedding::ContinueTask(taskHandle, invokeContext, CoreClrFuncInvokeContext::TaskComplete, &exception);

		if (exception)
		{
			CoreClrFuncInvokeContext::TaskComplete(exception, V8Type::PropertyTypeException, TaskStatus::Faulted, invokeContext);
		}
	}

	DBG("CoreClrFunc::Call - Finished");

	return NanEscapeScope(NanUndefined());
}

NAN_METHOD(CoreClrFunc::Initialize)
{
	DBG("CoreClrFunc::Initialize - Starting");

	NanEscapableScope();
	Handle<v8::Object> options = args[0]->ToObject();
	Handle<v8::Function> result;

	Handle<v8::Value> assemblyFileArgument = options->Get(NanNew<v8::String>("assemblyFile"));

	if (assemblyFileArgument->IsString())
	{
		v8::String::Utf8Value assemblyFile(assemblyFileArgument);
		v8::String::Utf8Value typeName(options->Get(NanNew<v8::String>("typeName")));
		v8::String::Utf8Value methodName(options->Get(NanNew<v8::String>("methodName")));
		v8::Handle<v8::Value> exception;

		DBG("CoreClrFunc::Initialize - Loading %s.%s() from %s", *typeName, *methodName, *assemblyFile);
		CoreClrGcHandle functionHandle = CoreClrEmbedding::GetClrFuncReflectionWrapFunc(*assemblyFile, *typeName, *methodName, &exception);

		if (functionHandle)
		{
			DBG("CoreClrFunc::Initialize - Function loaded successfully")

			result = CoreClrFunc::InitializeInstance(functionHandle);
			DBG("CoreClrFunc::Initialize - Callback initialized successfully");
		}

		else
		{
			DBG("CoreClrFunc::Initialize - Error loading function, V8 exception being thrown");
			throwV8Exception(exception);
		}
	}

	else
	{
		// TODO: support compilation from source once the Roslyn C# compiler is made available on CoreCLR
		throwV8Exception("Compiling .NET methods from source is not yet supported with CoreCLR, you must provide an assembly path, type name, and method name as arguments to edge.initializeClrFunction().");
	}

	DBG("CoreClrFunc::Initialize - Finished");

	NanReturnValue(result);
}

void CoreClrFunc::FreeMarshalData(void* marshalData, int payloadType)
{
	switch (payloadType)
	{
		case V8Type::PropertyTypeString:
			delete ((char*)marshalData);
			break;

		case V8Type::PropertyTypeObject:
			delete ((V8ObjectData*)marshalData);
			break;

		case V8Type::PropertyTypeBoolean:
			delete ((bool*)marshalData);
			break;

		case V8Type::PropertyTypeNumber:
		case V8Type::PropertyTypeDate:
			delete ((double*)marshalData);
			break;

		case V8Type::PropertyTypeInt32:
			delete ((int32_t*)marshalData);
			break;

		case V8Type::PropertyTypeUInt32:
			delete ((uint32_t*)marshalData);
			break;

		case V8Type::PropertyTypeNull:
			break;

		case V8Type::PropertyTypeArray:
			delete ((V8ArrayData*)marshalData);
			break;
	}
}

char* CoreClrFunc::CopyV8StringBytes(Handle<v8::String> v8String)
{
	String::Utf8Value utf8String(v8String);
	char* sourceBytes = *utf8String;
#ifdef EDGE_PLATFORM_WINDOWS
	size_t sourceLength = strlen(sourceBytes);
#else
	int sourceLength = strlen(sourceBytes);
#endif
	char* destinationBytes = new char[sourceLength + 1];

	strncpy(destinationBytes, sourceBytes, sourceLength);
	destinationBytes[sourceLength] = '\0';

	return destinationBytes;
}

void CoreClrFunc::MarshalV8ExceptionToCLR(Handle<v8::Value> exception, void** marshalData)
{
    NanScope();

    int payloadType;

    if (exception->IsObject())
    {
        Handle<Value> stack = exception->ToObject()->Get(NanNew<v8::String>("stack"));

        if (stack->IsString())
        {
        	MarshalV8ToCLR(stack, marshalData, &payloadType);
        	return;
        }
    }

    *marshalData = CopyV8StringBytes(Handle<v8::String>::Cast(exception));
}

void CoreClrFunc::MarshalV8ToCLR(Handle<v8::Value> jsdata, void** marshalData, int* payloadType)
{
	if (jsdata->IsString())
	{
		*marshalData = CopyV8StringBytes(Handle<v8::String>::Cast(jsdata));
		*payloadType = V8Type::PropertyTypeString;
	}

	else if (jsdata->IsFunction())
	{
		*marshalData = new CoreClrNodejsFunc(Handle<v8::Function>::Cast(jsdata));
		*payloadType = V8Type::PropertyTypeFunction;
	}

	else if (node::Buffer::HasInstance(jsdata))
	{
		Handle<v8::Object> jsBuffer = jsdata->ToObject();
		V8BufferData* bufferData = new V8BufferData();

		bufferData->bufferLength = (int)node::Buffer::Length(jsBuffer);
		bufferData->buffer = new char[bufferData->bufferLength];

		memcpy(bufferData->buffer, node::Buffer::Data(jsBuffer), bufferData->bufferLength);

		*marshalData = bufferData;
		*payloadType = V8Type::PropertyTypeBuffer;
	}

	else if (jsdata->IsArray())
	{
		Handle<v8::Array> jsarray = Handle<v8::Array>::Cast(jsdata);
		V8ArrayData* arrayData = new V8ArrayData();

		arrayData->arrayLength = jsarray->Length();
		arrayData->itemTypes = new int[arrayData->arrayLength];
		arrayData->itemValues = new void*[arrayData->arrayLength];

		for (int i = 0; i < arrayData->arrayLength; i++)
		{
			MarshalV8ToCLR(jsarray->Get(i), &arrayData->itemValues[i], &arrayData->itemTypes[i]);
		}

		*marshalData = arrayData;
		*payloadType = V8Type::PropertyTypeArray;
	}

	else if (jsdata->IsDate())
	{
		Handle<v8::Date> jsdate = Handle<v8::Date>::Cast(jsdata);
		double* ticks = new double();

		*ticks = jsdate->NumberValue();
		*marshalData = ticks;
		*payloadType = V8Type::PropertyTypeDate;
	}

	else if (jsdata->IsBoolean())
	{
		bool* value = new bool();
		*value = jsdata->BooleanValue();

		*marshalData = value;
		*payloadType = V8Type::PropertyTypeBoolean;
	}

	else if (jsdata->IsInt32())
	{
		int32_t* value = new int32_t();
		*value = jsdata->Int32Value();

		*marshalData = value;
		*payloadType = V8Type::PropertyTypeInt32;
	}

	else if (jsdata->IsUint32())
	{
		uint32_t* value = new uint32_t();
		*value = jsdata->Uint32Value();

		*marshalData = value;
		*payloadType = V8Type::PropertyTypeUInt32;
	}

	else if (jsdata->IsNumber())
	{
		double* value = new double();
		*value = jsdata->NumberValue();

		*marshalData = value;
		*payloadType = V8Type::PropertyTypeNumber;
	}

	else if (jsdata->IsUndefined() || jsdata->IsNull())
	{
		*payloadType = V8Type::PropertyTypeNull;
	}

	else if (jsdata->IsObject())
	{
		V8ObjectData* objectData = new V8ObjectData();

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
		*payloadType = V8Type::PropertyTypeObject;
	}
}

Handle<v8::Value> CoreClrFunc::MarshalCLRToV8(void* marshalData, int payloadType)
{
	NanEscapableScope();

	if (payloadType == V8Type::PropertyTypeString)
	{
		return NanEscapeScope(NanNew<v8::String>((char*) marshalData));
	}

	else if (payloadType == V8Type::PropertyTypeInt32)
	{
		return NanEscapeScope(NanNew<v8::Integer>(*(int*) marshalData));
	}

	else if (payloadType == V8Type::PropertyTypeNumber)
	{
		return NanEscapeScope(NanNew<v8::Number>(*(double*) marshalData));
	}

	else if (payloadType == V8Type::PropertyTypeDate)
	{
		return NanEscapeScope(NanNew<v8::Date>(*(double*) marshalData));
	}

	else if (payloadType == V8Type::PropertyTypeBoolean)
	{
		bool value = (*(int*) marshalData) != 0;
		return NanEscapeScope(NanNew<v8::Boolean>(value));
	}

	else if (payloadType == V8Type::PropertyTypeArray)
	{
		V8ArrayData* arrayData = (V8ArrayData*) marshalData;
		Handle<v8::Array> result = NanNew<v8::Array>();

		for (int i = 0; i < arrayData->arrayLength; i++)
		{
			result->Set(i, MarshalCLRToV8(arrayData->itemValues[i], arrayData->itemTypes[i]));
		}

		return NanEscapeScope(result);
	}

	else if (payloadType == V8Type::PropertyTypeObject || payloadType == V8Type::PropertyTypeException)
	{
		V8ObjectData* objectData = (V8ObjectData*) marshalData;
		Handle<v8::Object> result = NanNew<v8::Object>();

		for (int i = 0; i < objectData->propertiesCount; i++)
		{
			result->Set(NanNew<v8::String>(objectData->propertyNames[i]), MarshalCLRToV8(objectData->propertyData[i], objectData->propertyTypes[i]));
		}

		if (payloadType == V8Type::PropertyTypeException)
		{
			Handle<v8::String> name = Handle<v8::String>::Cast(result->Get(NanNew<v8::String>("Name")));
			Handle<v8::String> message = Handle<v8::String>::Cast(result->Get(NanNew<v8::String>("Message")));

			result->SetPrototype(v8::Exception::Error(message));
			result->Set(NanNew<v8::String>("message"), message);
			result->Set(NanNew<v8::String>("name"), name);
		}

		return NanEscapeScope(result);
	}

	else if (payloadType == V8Type::PropertyTypeNull)
	{
		return NanEscapeScope(NanNull());
	}

	else if (payloadType == V8Type::PropertyTypeBuffer)
	{
		V8BufferData* bufferData = (V8BufferData*) marshalData;

		if (bufferData->bufferLength > 0)
		{
			return NanEscapeScope(NanNewBufferHandle(bufferData->buffer, bufferData->bufferLength));
		}

		else
		{
			return NanEscapeScope(NanNewBufferHandle(0));
		}
	}

	else if (payloadType == V8Type::PropertyTypeFunction)
	{
		return NanEscapeScope(InitializeInstance(marshalData));
	}

	else
	{
		throwV8Exception("Unsupported object type received from the CLR: %d", payloadType);
		return NanEscapeScope(NanUndefined());
	}
}
