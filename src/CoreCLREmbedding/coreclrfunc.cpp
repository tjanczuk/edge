#include "edge.h"

CoreClrFunc::CoreClrFunc()
{
	functionHandle = NULL;
}

NAN_METHOD(coreClrFuncProxy)
{
    DBG("coreClrFuncProxy");
    Nan::EscapableHandleScope scope;
    v8::Local<v8::External> correlator = v8::Local<v8::External>::Cast(info[2]);
    CoreClrFuncWrap* wrap = (CoreClrFuncWrap*)(correlator->Value());
    CoreClrFunc* clrFunc = wrap->clrFunc;
    info.GetReturnValue().Set(clrFunc->Call(info[0], info[1]));
}

template<typename T>
void coreClrFuncProxyNearDeath(const Nan::WeakCallbackInfo<T> &data)
{
    DBG("coreClrFuncProxyNearDeath");
    CoreClrFuncWrap* wrap = (CoreClrFuncWrap*)(data.GetParameter());
	delete wrap->clrFunc;
    wrap->clrFunc = NULL;
    delete wrap;
}

v8::Local<v8::Function> CoreClrFunc::InitializeInstance(CoreClrGcHandle functionHandle)
{
    DBG("CoreClrFunc::InitializeInstance - Started");

    static Nan::Persistent<v8::Function> proxyFactory;
    static Nan::Persistent<v8::Function> proxyFunction;

    Nan::EscapableHandleScope scope;

    CoreClrFunc* app = new CoreClrFunc();
    app->functionHandle = functionHandle;
    CoreClrFuncWrap* wrap = new CoreClrFuncWrap();
    wrap->clrFunc = app;

    // See https://github.com/tjanczuk/edge/issues/128 for context

    if (proxyFactory.IsEmpty())
    {
    	DBG("CoreClrFunc::InitializeInstance - Creating proxy factory");

		v8::Local<v8::Function> clrFuncProxyFunction = Nan::New<v8::FunctionTemplate>(coreClrFuncProxy)->GetFunction();
		proxyFunction.Reset(clrFuncProxyFunction);
		v8::Local<v8::String> code = Nan::New<v8::String>(
			"(function (f, ctx) { return function (d, cb) { return f(d, cb, ctx); }; })").ToLocalChecked();
		v8::Local<v8::Function> codeFunction = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
		proxyFactory.Reset(codeFunction);
    }

	v8::Local<v8::Value> factoryArgv[] = { Nan::New(proxyFunction), Nan::New<v8::External>((void*)wrap) };
	v8::Local<v8::Function> funcProxy =
		(Nan::New(proxyFactory)->Call(Nan::GetCurrentContext()->Global(), 2, factoryArgv)).As<v8::Function>();
	Nan::Persistent<v8::Function> funcProxyPersistent(funcProxy);
	funcProxyPersistent.SetWeak((void*)wrap, &coreClrFuncProxyNearDeath, Nan::WeakCallbackType::kParameter);

    DBG("CoreClrFunc::InitializeInstance - Finished");

    return scope.Escape(funcProxy);
}

v8::Local<v8::Value> CoreClrFunc::Call(v8::Local<v8::Value> payload, v8::Local<v8::Value> callbackOrSync)
{
	DBG("CoreClrFunc::Call - Started");
	Nan::EscapableHandleScope scope;

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

	if (taskState == TaskStatusRanToCompletion || taskState == TaskStatusFaulted)
	{
		if (taskState == TaskStatusRanToCompletion)
		{
			DBG("CoreClrFunc::Call - Task ran synchronously, marshalling CLR data to V8");
		}

		else
		{
			DBG("CoreClrFunc::Call - Task threw an exception, marshalling CLR exception data to V8");
		}

		if (callbackOrSync->IsBoolean() || taskState == TaskStatusFaulted)
		{
			if (taskState == TaskStatusRanToCompletion)
			{
				return scope.Escape(CoreClrFunc::MarshalCLRToV8(result, resultType));
			}

			else
			{
				Nan::ThrowError(CoreClrFunc::MarshalCLRToV8(result, resultType));
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
			CoreClrFuncInvokeContext::TaskComplete(exception, V8TypeException, TaskStatusFaulted, invokeContext);
		}
	}

	DBG("CoreClrFunc::Call - Finished");

	return scope.Escape(Nan::Undefined());
}

NAN_METHOD(CoreClrFunc::Initialize)
{
	DBG("CoreClrFunc::Initialize - Starting");

	Nan::EscapableHandleScope scope;
	v8::Local<v8::Object> options = info[0]->ToObject();
	v8::Local<v8::Function> result;

	v8::Local<v8::Value> assemblyFileArgument = options->Get(Nan::New<v8::String>("assemblyFile").ToLocalChecked());

	if (assemblyFileArgument->IsString())
	{
		v8::String::Utf8Value assemblyFile(assemblyFileArgument);
		v8::String::Utf8Value typeName(options->Get(Nan::New<v8::String>("typeName").ToLocalChecked()));
		v8::String::Utf8Value methodName(options->Get(Nan::New<v8::String>("methodName").ToLocalChecked()));
		v8::Local<v8::Value> exception;

		DBG("CoreClrFunc::Initialize - Loading %s.%s() from %s", *typeName, *methodName, *assemblyFile);
		CoreClrGcHandle functionHandle = CoreClrEmbedding::GetClrFuncReflectionWrapFunc(*assemblyFile, *typeName, *methodName, &exception);

		if (functionHandle)
		{
			DBG("CoreClrFunc::Initialize - Function loaded successfully");

			result = CoreClrFunc::InitializeInstance(functionHandle);
			DBG("CoreClrFunc::Initialize - Callback initialized successfully");
		}

		else
		{
			DBG("CoreClrFunc::Initialize - Error loading function, V8 exception being thrown");
			return Nan::ThrowError(exception);
		}
	}

	else
	{
		v8::Local<v8::Value> exception;
		void* marshalledOptionsData;
		int payloadType;

		MarshalV8ToCLR(options, &marshalledOptionsData, &payloadType);

		DBG("CoreClrFunc::Initialize - Compiling dynamic .NET function");
		CoreClrGcHandle functionHandle = CoreClrEmbedding::CompileFunc(marshalledOptionsData, payloadType, &exception);

		if (functionHandle)
		{
			DBG("CoreClrFunc::Initialize - Function compiled successfully");

			result = CoreClrFunc::InitializeInstance(functionHandle);
			DBG("CoreClrFunc::Initialize - Callback initialized successfully");
		}

		else
		{
			DBG("CoreClrFunc::Initialize - Error loading function, V8 exception being thrown");
			return Nan::ThrowError(exception);
		}
	}

	DBG("CoreClrFunc::Initialize - Finished");

	info.GetReturnValue().Set(result);
}

void CoreClrFunc::FreeMarshalData(void* marshalData, int payloadType)
{
	switch (payloadType)
	{
		case V8TypeString:
			delete ((char*)marshalData);
			break;

		case V8TypeObject:
			delete ((V8ObjectData*)marshalData);
			break;

		case V8TypeBoolean:
			delete ((bool*)marshalData);
			break;

		case V8TypeNumber:
		case V8TypeDate:
			delete ((double*)marshalData);
			break;

		case V8TypeInt32:
			delete ((int32_t*)marshalData);
			break;

		case V8TypeUInt32:
			delete ((uint32_t*)marshalData);
			break;

		case V8TypeNull:
			break;

		case V8TypeArray:
			delete ((V8ArrayData*)marshalData);
			break;

        case V8TypeBuffer:
            delete ((V8BufferData*)marshalData);
            break;
	}
}

char* CoreClrFunc::CopyV8StringBytes(v8::Local<v8::String> v8String)
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

void CoreClrFunc::MarshalV8ExceptionToCLR(v8::Local<v8::Value> exception, void** marshalData)
{
	Nan::HandleScope scope;

    int payloadType;

    if (exception->IsObject())
    {
        v8::Local<Value> stack = exception->ToObject()->Get(Nan::New<v8::String>("stack").ToLocalChecked());

        if (stack->IsString())
        {
        	MarshalV8ToCLR(stack, marshalData, &payloadType);
        	return;
        }
    }

    *marshalData = CopyV8StringBytes(v8::Local<v8::String>::Cast(exception));
}

void CoreClrFunc::MarshalV8ToCLR(v8::Local<v8::Value> jsdata, void** marshalData, int* payloadType)
{
	if (jsdata->IsString())
	{
		*marshalData = CopyV8StringBytes(v8::Local<v8::String>::Cast(jsdata));
		*payloadType = V8TypeString;
	}

	else if (jsdata->IsFunction())
	{
		*marshalData = new CoreClrNodejsFunc(v8::Local<v8::Function>::Cast(jsdata));
		*payloadType = V8TypeFunction;
	}

	else if (node::Buffer::HasInstance(jsdata))
	{
		v8::Local<v8::Object> jsBuffer = jsdata->ToObject();
		V8BufferData* bufferData = new V8BufferData();

		bufferData->bufferLength = (int)node::Buffer::Length(jsBuffer);
		bufferData->buffer = new char[bufferData->bufferLength];

		memcpy(bufferData->buffer, node::Buffer::Data(jsBuffer), bufferData->bufferLength);

		*marshalData = bufferData;
		*payloadType = V8TypeBuffer;
	}

	else if (jsdata->IsArray())
	{
		v8::Local<v8::Array> jsarray = v8::Local<v8::Array>::Cast(jsdata);
		V8ArrayData* arrayData = new V8ArrayData();

		arrayData->arrayLength = jsarray->Length();
		arrayData->itemTypes = new int[arrayData->arrayLength];
		arrayData->itemValues = new void*[arrayData->arrayLength];

		for (int i = 0; i < arrayData->arrayLength; i++)
		{
			MarshalV8ToCLR(jsarray->Get(i), &arrayData->itemValues[i], &arrayData->itemTypes[i]);
		}

		*marshalData = arrayData;
		*payloadType = V8TypeArray;
	}

	else if (jsdata->IsDate())
	{
		v8::Local<v8::Date> jsdate = v8::Local<v8::Date>::Cast(jsdata);
		double* ticks = new double();

		*ticks = jsdate->NumberValue();
		*marshalData = ticks;
		*payloadType = V8TypeDate;
	}

	else if (jsdata->IsBoolean())
	{
		bool* value = new bool();
		*value = jsdata->BooleanValue();

		*marshalData = value;
		*payloadType = V8TypeBoolean;
	}

	else if (jsdata->IsInt32())
	{
		int32_t* value = new int32_t();
		*value = jsdata->Int32Value();

		*marshalData = value;
		*payloadType = V8TypeInt32;
	}

	else if (jsdata->IsUint32())
	{
		uint32_t* value = new uint32_t();
		*value = jsdata->Uint32Value();

		*marshalData = value;
		*payloadType = V8TypeUInt32;
	}

	else if (jsdata->IsNumber())
	{
		double* value = new double();
		*value = jsdata->NumberValue();

		*marshalData = value;
		*payloadType = V8TypeNumber;
	}

	else if (jsdata->IsUndefined() || jsdata->IsNull())
	{
		*payloadType = V8TypeNull;
	}

	else if (jsdata->IsObject())
	{
		V8ObjectData* objectData = new V8ObjectData();

		v8::Local<v8::Object> jsobject = v8::Local<v8::Object>::Cast(jsdata);
		v8::Local<v8::Array> propertyNames = jsobject->GetPropertyNames();

		objectData->propertiesCount = propertyNames->Length();
		objectData->propertyData = new void*[objectData->propertiesCount];
		objectData->propertyNames = new char*[objectData->propertiesCount];
		objectData->propertyTypes = new int[objectData->propertiesCount];

		for (unsigned int i = 0; i < propertyNames->Length(); i++)
		{
			v8::Local<v8::String> name = v8::Local<v8::String>::Cast(propertyNames->Get(i));

			objectData->propertyNames[i] = CopyV8StringBytes(name);
			MarshalV8ToCLR(jsobject->Get(name), &objectData->propertyData[i], &objectData->propertyTypes[i]);
		}

		*marshalData = objectData;
		*payloadType = V8TypeObject;
	}
}

v8::Local<v8::Value> CoreClrFunc::MarshalCLRToV8(void* marshalData, int payloadType)
{
	Nan::EscapableHandleScope scope;

	if (payloadType == V8TypeString)
	{
		return scope.Escape(Nan::New<v8::String>((char*)marshalData).ToLocalChecked());
	}

	else if (payloadType == V8TypeInt32)
	{
		return scope.Escape(Nan::New<v8::Integer>(*(int*) marshalData));
	}

	else if (payloadType == V8TypeNumber)
	{
		return scope.Escape(Nan::New<v8::Number>(*(double*) marshalData));
	}

	else if (payloadType == V8TypeDate)
	{
		return scope.Escape(Nan::New<v8::Date>(*(double*) marshalData).ToLocalChecked());
	}

	else if (payloadType == V8TypeBoolean)
	{
		bool value = (*(int*) marshalData) != 0;
		return scope.Escape(Nan::New<v8::Boolean>(value));
	}

	else if (payloadType == V8TypeArray)
	{
		V8ArrayData* arrayData = (V8ArrayData*) marshalData;
		v8::Local<v8::Array> result = Nan::New<v8::Array>();

		for (int i = 0; i < arrayData->arrayLength; i++)
		{
			result->Set(i, MarshalCLRToV8(arrayData->itemValues[i], arrayData->itemTypes[i]));
		}

		return scope.Escape(result);
	}

	else if (payloadType == V8TypeObject || payloadType == V8TypeException)
	{
		V8ObjectData* objectData = (V8ObjectData*) marshalData;
		v8::Local<v8::Object> result = Nan::New<v8::Object>();

		for (int i = 0; i < objectData->propertiesCount; i++)
		{
			result->Set(Nan::New<v8::String>(objectData->propertyNames[i]).ToLocalChecked(), MarshalCLRToV8(objectData->propertyData[i], objectData->propertyTypes[i]));
		}

		if (payloadType == V8TypeException)
		{
			v8::Local<v8::String> name = v8::Local<v8::String>::Cast(result->Get(Nan::New<v8::String>("Name").ToLocalChecked()));
			v8::Local<v8::String> message = v8::Local<v8::String>::Cast(result->Get(Nan::New<v8::String>("Message").ToLocalChecked()));

			result->SetPrototype(v8::Exception::Error(message));
			result->Set(Nan::New<v8::String>("message").ToLocalChecked(), message);
			result->Set(Nan::New<v8::String>("name").ToLocalChecked(), name);
		}

		return scope.Escape(result);
	}

	else if (payloadType == V8TypeNull)
	{
		return scope.Escape(Nan::Null());
	}

	else if (payloadType == V8TypeBuffer)
	{
		V8BufferData* bufferData = (V8BufferData*) marshalData;

		if (bufferData->bufferLength > 0)
		{
			return scope.Escape(Nan::CopyBuffer(bufferData->buffer, bufferData->bufferLength).ToLocalChecked());
		}

		else
		{
			return scope.Escape(Nan::NewBuffer(0).ToLocalChecked());
		}
	}

	else if (payloadType == V8TypeFunction)
	{
		return scope.Escape(InitializeInstance(marshalData));
	}

	else
	{
		throwV8Exception("Unsupported object type received from the CLR: %d", payloadType);
		return scope.Escape(Nan::Undefined());
	}
}
