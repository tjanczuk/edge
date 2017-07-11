#include "edge.h"

#using <System.Core.dll>

ClrFunc::ClrFunc()
{
    // empty
}

NAN_METHOD(clrFuncProxy)
{
    DBG("clrFuncProxy");
    Nan::HandleScope scope;
    v8::Local<v8::External> correlator = v8::Local<v8::External>::Cast(info[2]);
    ClrFuncWrap* wrap = (ClrFuncWrap*)(correlator->Value());
    ClrFunc^ clrFunc = wrap->clrFunc;
    info.GetReturnValue().Set(clrFunc->Call(info[0], info[1]));
}

template<typename T>
void clrFuncProxyNearDeath(const Nan::WeakCallbackInfo<T> &data)
{
    DBG("clrFuncProxyNearDeath");
    ClrFuncWrap* wrap = (ClrFuncWrap*)(data.GetParameter());
    wrap->clrFunc = nullptr;
    delete wrap;
}

v8::Local<v8::Function> ClrFunc::Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func)
{
    DBG("ClrFunc::Initialize Func<object,Task<object>> wrapper");

    static Nan::Persistent<v8::Function> proxyFactory;
    static Nan::Persistent<v8::Function> proxyFunction;

    Nan::EscapableHandleScope scope;

    ClrFunc^ app = gcnew ClrFunc();
    app->func = func;
    ClrFuncWrap* wrap = new ClrFuncWrap;
    wrap->clrFunc = app;

    // See https://github.com/tjanczuk/edge/issues/128 for context

    if (proxyFactory.IsEmpty())
    {
        v8::Local<v8::Function> clrFuncProxyFunction = Nan::New<v8::FunctionTemplate>(clrFuncProxy)->GetFunction();
        proxyFunction.Reset(clrFuncProxyFunction);
        v8::Local<v8::String> code = Nan::New<v8::String>(
            "(function (f, ctx) { return function (d, cb) { return f(d, cb, ctx); }; })").ToLocalChecked();
        v8::Local<v8::Function> codeFunction = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
        proxyFactory.Reset(codeFunction);
    }

    v8::Local<v8::Value> factoryArgv[] = { Nan::New(proxyFunction), Nan::New<v8::External>((void*)wrap) };
    v8::Local<v8::Function> funcProxy =
        (Nan::Call(Nan::New(proxyFactory), Nan::GetCurrentContext()->Global(), 2, factoryArgv)).ToLocalChecked().As<v8::Function>();
    Nan::Persistent<v8::Function> funcProxyPersistent(funcProxy);
    funcProxyPersistent.SetWeak((void*)wrap, &clrFuncProxyNearDeath, Nan::WeakCallbackType::kParameter);

    return scope.Escape(funcProxy);
}

NAN_METHOD(ClrFunc::Initialize)
{
    DBG("ClrFunc::Initialize MethodInfo wrapper");

    Nan::EscapableHandleScope scope;
    v8::Local<v8::Object> options = info[0]->ToObject();
    Assembly^ assembly;
    System::String^ typeName;
    System::String^ methodName;

    try
    {
        v8::Local<v8::Function> result;

        v8::Local<v8::Value> jsassemblyFile = options->Get(Nan::New<v8::String>("assemblyFile").ToLocalChecked());
        if (jsassemblyFile->IsString()) {
            // reference .NET code through pre-compiled CLR assembly
            v8::String::Utf8Value assemblyFile(jsassemblyFile);
            v8::String::Utf8Value nativeTypeName(options->Get(Nan::New<v8::String>("typeName").ToLocalChecked()));
            v8::String::Utf8Value nativeMethodName(options->Get(Nan::New<v8::String>("methodName").ToLocalChecked()));

            typeName = stringV82CLR(nativeTypeName);
            methodName = stringV82CLR(nativeMethodName);
            assembly = Assembly::UnsafeLoadFrom(stringV82CLR(assemblyFile));

            ClrFuncReflectionWrap^ wrap = ClrFuncReflectionWrap::Create(assembly, typeName, methodName);
            result = ClrFunc::Initialize(
                gcnew System::Func<System::Object^,Task<System::Object^>^>(
                    wrap, &ClrFuncReflectionWrap::Call));
        }
        else {
            // reference .NET code throgh embedded source code that needs to be compiled
            v8::String::Value compilerFile(options->Get(Nan::New<v8::String>("compiler").ToLocalChecked()));
            cli::array<unsigned char>^ buffer = gcnew cli::array<unsigned char>(compilerFile.length() * 2);
            for (int k = 0; k < compilerFile.length(); k++)
            {
                buffer[k * 2] = (*compilerFile)[k] & 255;
                buffer[k * 2 + 1] = (*compilerFile)[k] >> 8;
            }
            assembly = Assembly::UnsafeLoadFrom(System::Text::Encoding::Unicode->GetString(buffer));
            System::Type^ compilerType = assembly->GetType("EdgeCompiler", true, true);
            System::Object^ compilerInstance = System::Activator::CreateInstance(compilerType, false);
            MethodInfo^ compileFunc = compilerType->GetMethod("CompileFunc", BindingFlags::Instance | BindingFlags::Public);
            if (compileFunc == nullptr)
            {
                throw gcnew System::InvalidOperationException(
                    "Unable to access the CompileFunc method of the EdgeCompiler class in the edge.js compiler assembly.");
            }

            System::Object^ parameters = ClrFunc::MarshalV8ToCLR(options);
            System::Func<System::Object^,Task<System::Object^>^>^ func =
                (System::Func<System::Object^,Task<System::Object^>^>^)compileFunc->Invoke(
                    compilerInstance, gcnew array<System::Object^> { parameters });
            result = ClrFunc::Initialize(func);
        }

        info.GetReturnValue().Set(result);
    }
    catch (System::Exception^ e)
    {
        return Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(e));
    }
}

void edgeAppCompletedOnCLRThread(Task<System::Object^>^ task, System::Object^ state)
{
    DBG("edgeAppCompletedOnCLRThread");
    ClrFuncInvokeContext^ context = (ClrFuncInvokeContext^)state;
    context->CompleteOnCLRThread(task);
}

v8::Local<v8::Value> ClrFunc::MarshalCLRToV8(System::Object^ netdata)
{
    Nan::EscapableHandleScope scope;
    v8::Local<v8::Value> jsdata;

    if (netdata == nullptr)
    {
        return scope.Escape(Nan::Null());
    }

    System::Type^ type = netdata->GetType();
    if (type == System::String::typeid)
    {
        jsdata = stringCLR2V8((System::String^)netdata);
    }
    else if (type == System::Char::typeid)
    {
        jsdata = stringCLR2V8(((System::Char^)netdata)->ToString());
    }
    else if (type == bool::typeid)
    {
        jsdata = Nan::New<v8::Boolean>((bool)netdata);
    }
    else if (type == System::Guid::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == System::DateTime::typeid)
    {
        System::DateTime ^dt = (System::DateTime^)netdata;
        if (dt->Kind == System::DateTimeKind::Local)
            dt = dt->ToUniversalTime();
        else if (dt->Kind == System::DateTimeKind::Unspecified)
            dt = gcnew System::DateTime(dt->Ticks, System::DateTimeKind::Utc);
        long long MinDateTimeTicks = 621355968000000000; // new DateTime(1970, 1, 1, 0, 0, 0).Ticks;
        long long value = ((dt->Ticks - MinDateTimeTicks) / 10000);
        jsdata = Nan::New<v8::Date>((double)value).ToLocalChecked();
    }
    else if (type == System::DateTimeOffset::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == System::Uri::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == System::Int16::typeid)
    {
        jsdata = Nan::New<v8::Integer>((short)netdata);
    }
    else if (type == System::UInt16::typeid)
    {
        jsdata = Nan::New<v8::Integer>((unsigned short)netdata);
    }
    else if (type == System::Int32::typeid)
    {
        jsdata = Nan::New<v8::Integer>((int)netdata);
    }
    else if (type == System::UInt32::typeid)
    {
        jsdata = Nan::New<v8::Integer>((unsigned int)netdata);
    }
    else if (type == System::Int64::typeid)
    {
        jsdata = Nan::New<v8::Number>(((System::IConvertible^)netdata)->ToDouble(nullptr));
    }
    else if (type == double::typeid)
    {
        jsdata = Nan::New<v8::Number>((double)netdata);
    }
    else if (type == float::typeid)
    {
        jsdata = Nan::New<v8::Number>((float)netdata);
    }
    else if (type->IsPrimitive || type == System::Decimal::typeid)
    {
        System::IConvertible^ convertible = dynamic_cast<System::IConvertible^>(netdata);
        if (convertible != nullptr)
        {
            jsdata = stringCLR2V8(convertible->ToString());
        }
        else
        {
            jsdata = stringCLR2V8(netdata->ToString());
        }
    }
    else if (type->IsEnum)
    {
        if (enableMarshalEnumAsInt)
        {
            jsdata = Nan::New<v8::Integer>((int)netdata);
        }
        else
        {
            jsdata = stringCLR2V8(netdata->ToString());
        }
    }
    else if (type == cli::array<byte>::typeid)
    {
        cli::array<byte>^ buffer = (cli::array<byte>^)netdata;
        if (buffer->Length > 0)
        {
            pin_ptr<unsigned char> pinnedBuffer = &buffer[0];
            jsdata = Nan::CopyBuffer((char *)pinnedBuffer, buffer->Length).ToLocalChecked();
        }
        else
        {
            jsdata = Nan::NewBuffer(0).ToLocalChecked();
        }
    }
    else if (dynamic_cast<System::Collections::Generic::IDictionary<System::String^,System::Object^>^>(netdata) != nullptr)
    {
        v8::Local<v8::Object> result = Nan::New<v8::Object>();
        for each (System::Collections::Generic::KeyValuePair<System::String^,System::Object^>^ pair
            in (System::Collections::Generic::IDictionary<System::String^,System::Object^>^)netdata)
        {
            result->Set(stringCLR2V8(pair->Key), ClrFunc::MarshalCLRToV8(pair->Value));
        }

        jsdata = result;
    }
    else if (dynamic_cast<System::Collections::IDictionary^>(netdata) != nullptr)
    {
        v8::Local<v8::Object> result = Nan::New<v8::Object>();
        for each (System::Collections::DictionaryEntry^ entry in (System::Collections::IDictionary^)netdata)
        {
            if (dynamic_cast<System::String^>(entry->Key) != nullptr)
            result->Set(stringCLR2V8((System::String^)entry->Key), ClrFunc::MarshalCLRToV8(entry->Value));
        }

        jsdata = result;
    }
    else if (dynamic_cast<System::Collections::IEnumerable^>(netdata) != nullptr)
    {
        v8::Local<v8::Array> result = Nan::New<v8::Array>();
        unsigned int i = 0;
        for each (System::Object^ entry in (System::Collections::IEnumerable^)netdata)
        {
            result->Set(i++, ClrFunc::MarshalCLRToV8(entry));
        }

        jsdata = result;
    }
    else if (type == System::Func<System::Object^,Task<System::Object^>^>::typeid)
    {
        jsdata = ClrFunc::Initialize((System::Func<System::Object^,Task<System::Object^>^>^)netdata);
    }
    else if (System::Exception::typeid->IsAssignableFrom(type))
    {
        jsdata = ClrFunc::MarshalCLRExceptionToV8((System::Exception^)netdata);
    }
    else
    {
        jsdata = ClrFunc::MarshalCLRObjectToV8(netdata);
    }

    return scope.Escape(jsdata);
}

v8::Local<v8::Value> ClrFunc::MarshalCLRExceptionToV8(System::Exception^ exception)
{
    DBG("ClrFunc::MarshalCLRExceptionToV8");
    Nan::EscapableHandleScope scope;
    v8::Local<v8::Object> result;
    v8::Local<v8::String> message;
    v8::Local<v8::String> name;

    if (exception == nullptr)
    {
        result = Nan::New<v8::Object>();
        message = Nan::New<v8::String>("Unrecognized exception thrown by CLR.").ToLocalChecked();
        name = Nan::New<v8::String>("InternalException").ToLocalChecked();
    }
    else
    {
        // Remove AggregateException wrapper from around singleton InnerExceptions
        if (System::AggregateException::typeid->IsAssignableFrom(exception->GetType()))
        {
            System::AggregateException^ aggregate = (System::AggregateException^)exception;
            if (aggregate->InnerExceptions->Count == 1)
                exception = aggregate->InnerExceptions[0];
        }
        else if (System::Reflection::TargetInvocationException::typeid->IsAssignableFrom(exception->GetType())
            && exception->InnerException != nullptr)
        {
            exception = exception->InnerException;
        }

        result = ClrFunc::MarshalCLRObjectToV8(exception);
        message = stringCLR2V8(exception->Message);
        name = stringCLR2V8(exception->GetType()->FullName);
    }

    // Construct an error that is just used for the prototype - not verify efficient
    // but 'typeof Error' should work in JavaScript
    result->SetPrototype(v8::Exception::Error(message));
    result->Set(Nan::New<v8::String>("message").ToLocalChecked(), message);

    // Recording the actual type - 'name' seems to be the common used property
    result->Set(Nan::New<v8::String>("name").ToLocalChecked(), name);

    return scope.Escape(result);
}

v8::Local<v8::Object> ClrFunc::MarshalCLRObjectToV8(System::Object^ netdata)
{
    DBG("ClrFunc::MarshalCLRObjectToV8");
    Nan::EscapableHandleScope scope;
    v8::Local<v8::Object> result = Nan::New<v8::Object>();
    System::Type^ type = netdata->GetType();

    if (0 == System::String::Compare(type->FullName, "System.Reflection.RuntimeMethodInfo")) {
        // Avoid stack overflow due to self-referencing reflection elements
        return scope.Escape(result);
    }

    for each (FieldInfo^ field in type->GetFields(BindingFlags::Public | BindingFlags::Instance))
    {
        result->Set(
            stringCLR2V8(field->Name),
            ClrFunc::MarshalCLRToV8(field->GetValue(netdata)));
    }

    for each (PropertyInfo^ property in type->GetProperties(BindingFlags::GetProperty | BindingFlags::Public | BindingFlags::Instance))
    {
        if (enableScriptIgnoreAttribute)
        {
            if (property->IsDefined(System::Web::Script::Serialization::ScriptIgnoreAttribute::typeid, true))
            {
                continue;
            }

            System::Web::Script::Serialization::ScriptIgnoreAttribute^ attr =
                (System::Web::Script::Serialization::ScriptIgnoreAttribute^)System::Attribute::GetCustomAttribute(
                    property,
                    System::Web::Script::Serialization::ScriptIgnoreAttribute::typeid,
                    true);

            if (attr != nullptr && attr->ApplyToOverrides)
            {
                continue;
            }
        }

        MethodInfo^ getMethod = property->GetGetMethod();
        if (getMethod != nullptr && getMethod->GetParameters()->Length <= 0)
        {
            result->Set(
                stringCLR2V8(property->Name),
                ClrFunc::MarshalCLRToV8(getMethod->Invoke(netdata, nullptr)));
        }
    }

    return scope.Escape(result);
}

System::Object^ ClrFunc::MarshalV8ToCLR(v8::Local<v8::Value> jsdata)
{
    Nan::HandleScope scope;

    if (jsdata->IsFunction())
    {
        NodejsFunc^ functionContext = gcnew NodejsFunc(v8::Local<v8::Function>::Cast(jsdata));
        System::Func<System::Object^,Task<System::Object^>^>^ netfunc =
            gcnew System::Func<System::Object^,Task<System::Object^>^>(
                functionContext, &NodejsFunc::FunctionWrapper);

        return netfunc;
    }
    else if (node::Buffer::HasInstance(jsdata))
    {
        v8::Local<v8::Object> jsbuffer = jsdata->ToObject();
        cli::array<byte>^ netbuffer = gcnew cli::array<byte>((int)node::Buffer::Length(jsbuffer));
        if (netbuffer->Length > 0)
        {
            pin_ptr<byte> pinnedNetbuffer = &netbuffer[0];
            memcpy(pinnedNetbuffer, node::Buffer::Data(jsbuffer), netbuffer->Length);
        }

        return netbuffer;
    }
    else if (jsdata->IsArray())
    {
        v8::Local<v8::Array> jsarray = v8::Local<v8::Array>::Cast(jsdata);
        cli::array<System::Object^>^ netarray = gcnew cli::array<System::Object^>(jsarray->Length());
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            netarray[i] = ClrFunc::MarshalV8ToCLR(jsarray->Get(i));
        }

        return netarray;
    }
    else if (jsdata->IsDate())
    {
        v8::Local<v8::Date> jsdate = v8::Local<v8::Date>::Cast(jsdata);
        long long  ticks = (long long)jsdate->NumberValue();
        long long MinDateTimeTicks = 621355968000000000;// (new DateTime(1970, 1, 1, 0, 0, 0)).Ticks;
        System::DateTime ^netobject = gcnew System::DateTime(ticks * 10000 + MinDateTimeTicks, System::DateTimeKind::Utc);
        return netobject;
    }
    else if (jsdata->IsObject())
    {
        IDictionary<System::String^,System::Object^>^ netobject = gcnew System::Dynamic::ExpandoObject();
        v8::Local<v8::Object> jsobject = v8::Local<v8::Object>::Cast(jsdata);
        v8::Local<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            v8::Local<v8::String> name = v8::Local<v8::String>::Cast(propertyNames->Get(i));
            v8::String::Utf8Value utf8name(name);
            System::String^ netname = gcnew System::String(*utf8name);
            System::Object^ netvalue = ClrFunc::MarshalV8ToCLR(jsobject->Get(name));
            netobject->Add(netname, netvalue);
        }

        return netobject;
    }
    else if (jsdata->IsString())
    {
        return stringV82CLR(v8::Local<v8::String>::Cast(jsdata));
    }
    else if (jsdata->IsBoolean())
    {
        return jsdata->BooleanValue();
    }
    else if (jsdata->IsInt32())
    {
        return jsdata->Int32Value();
    }
    else if (jsdata->IsUint32())
    {
        return jsdata->Uint32Value();
    }
    else if (jsdata->IsNumber())
    {
        return jsdata->NumberValue();
    }
    else if (jsdata->IsUndefined() || jsdata->IsNull())
    {
        return nullptr;
    }
    else
    {
        throw gcnew System::Exception("Unable to convert V8 value to CLR value.");
    }
}

v8::Local<v8::Value> ClrFunc::Call(v8::Local<v8::Value> payload, v8::Local<v8::Value> callback)
{
    DBG("ClrFunc::Call instance");
    Nan::EscapableHandleScope scope;

    try
    {
        ClrFuncInvokeContext^ context = gcnew ClrFuncInvokeContext(callback);
        context->Payload = ClrFunc::MarshalV8ToCLR(payload);
        Task<System::Object^>^ task = this->func(context->Payload);
        if (task->IsCompleted)
        {
            // Completed synchronously. Return a value or invoke callback based on call pattern.
            context->Task = task;
            return scope.Escape(context->CompleteOnV8Thread());
        }
        else if (context->Sync)
        {
            // Will complete asynchronously but was called as a synchronous function.
            throw gcnew System::InvalidOperationException("The JavaScript function was called synchronously "
                + "but the underlying CLR function returned without completing the Task. Call the "
                + "JavaScript function asynchronously.");
        }
        else
        {
            // Create a GC root around the ClrFuncInvokeContext to ensure it is not garbage collected
            // while the CLR function executes asynchronously.
            context->InitializeAsyncOperation();

            // Will complete asynchronously. Schedule continuation to finish processing.
            task->ContinueWith(gcnew System::Action<Task<System::Object^>^,System::Object^>(
                edgeAppCompletedOnCLRThread), context);
        }
    }
    catch (System::Exception^ e)
    {
        Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(e));
    }

    return scope.Escape(Nan::Undefined());
}
