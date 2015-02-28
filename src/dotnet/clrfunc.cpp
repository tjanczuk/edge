#include "edge.h"

#using <System.Core.dll>

ClrFunc::ClrFunc()
{
    // empty
}

NAN_METHOD(clrFuncProxy)
{
    DBG("clrFuncProxy");
    NanEscapableScope();
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
    ClrFuncWrap* wrap = (ClrFuncWrap*)(correlator->Value());
    ClrFunc^ clrFunc = wrap->clrFunc;
    NanReturnValue(clrFunc->Call(args[0], args[1]));
}

NAN_WEAK_CALLBACK(clrFuncProxyNearDeath)
{
    DBG("clrFuncProxyNearDeath");
    ClrFuncWrap* wrap = (ClrFuncWrap*)(data.GetParameter());
    wrap->clrFunc = nullptr;
    delete wrap;
}

Handle<v8::Function> ClrFunc::Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func)
{
    DBG("ClrFunc::Initialize Func<object,Task<object>> wrapper");

    static Persistent<v8::Function> proxyFactory;
    static Persistent<v8::Function> proxyFunction;        

    NanEscapableScope();

    ClrFunc^ app = gcnew ClrFunc();
    app->func = func;
    ClrFuncWrap* wrap = new ClrFuncWrap;
    wrap->clrFunc = app;    

    // See https://github.com/tjanczuk/edge/issues/128 for context
    
    if (proxyFactory.IsEmpty())
    {
        NanAssignPersistent(
            proxyFunction, NanNew<FunctionTemplate>(clrFuncProxy)->GetFunction());
        Handle<v8::String> code = NanNew<v8::String>(
            "(function (f, ctx) { return function (d, cb) { return f(d, cb, ctx); }; })");
        NanAssignPersistent(
            proxyFactory, Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
    }

    Handle<v8::Value> factoryArgv[] = { NanNew(proxyFunction), NanNew<v8::External>((void*)wrap) };
    Local<v8::Function> funcProxy = 
            (NanNew(proxyFactory)->Call(NanGetCurrentContext()->Global(), 2, factoryArgv)).As<v8::Function>();
    NanMakeWeakPersistent(funcProxy, (void*)wrap, &clrFuncProxyNearDeath);

    return NanEscapeScope(funcProxy);
}

NAN_METHOD(ClrFunc::Initialize)
{
    DBG("ClrFunc::Initialize MethodInfo wrapper");

    NanEscapableScope();
    Handle<v8::Object> options = args[0]->ToObject();
    Assembly^ assembly;
    System::String^ typeName;
    System::String^ methodName;

    try 
    {
        Handle<v8::Function> result;

        Handle<v8::Value> jsassemblyFile = options->Get(NanNew<v8::String>("assemblyFile"));
        if (jsassemblyFile->IsString()) {
            // reference .NET code through pre-compiled CLR assembly 
            String::Utf8Value assemblyFile(jsassemblyFile);
            String::Utf8Value nativeTypeName(options->Get(NanNew<v8::String>("typeName")));
            String::Utf8Value nativeMethodName(options->Get(NanNew<v8::String>("methodName")));  
            typeName = gcnew System::String(*nativeTypeName);
            methodName = gcnew System::String(*nativeMethodName);      
            assembly = Assembly::UnsafeLoadFrom(gcnew System::String(*assemblyFile));
            ClrFuncReflectionWrap^ wrap = ClrFuncReflectionWrap::Create(assembly, typeName, methodName);
            result = ClrFunc::Initialize(
                gcnew System::Func<System::Object^,Task<System::Object^>^>(
                    wrap, &ClrFuncReflectionWrap::Call));
        }
        else {
            // reference .NET code throgh embedded source code that needs to be compiled
            String::Value compilerFile(options->Get(NanNew<v8::String>("compiler")));
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

        NanReturnValue(result);
    }
    catch (System::Exception^ e)
    {
        throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(e));
        NanReturnValue(NanUndefined());
    }
}

void edgeAppCompletedOnCLRThread(Task<System::Object^>^ task, System::Object^ state)
{
    DBG("edgeAppCompletedOnCLRThread");
    ClrFuncInvokeContext^ context = (ClrFuncInvokeContext^)state;
    context->CompleteOnCLRThread(task);
}

Handle<v8::Value> ClrFunc::MarshalCLRToV8(System::Object^ netdata)
{
    NanEscapableScope();
    Handle<v8::Value> jsdata;

    if (netdata == nullptr)
    {
        return NanEscapeScope(NanNull());
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
        jsdata = NanNew<v8::Boolean>((bool)netdata);
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
        jsdata = NanNew<v8::Date>((double)value);
    }
    else if (type == System::DateTimeOffset::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == System::Uri::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == int::typeid)
    {
        jsdata = NanNew<v8::Integer>((int)netdata);
    }
    else if (type == System::Int16::typeid)
    {
        jsdata = NanNew<v8::Integer>((int)netdata);
    }
    else if (type == System::Int64::typeid)
    {
        jsdata = NanNew<v8::Number>(((System::IConvertible^)netdata)->ToDouble(nullptr));
    }
    else if (type == double::typeid)
    {
        jsdata = NanNew<v8::Number>((double)netdata);
    }
    else if (type == float::typeid)
    {
        jsdata = NanNew<v8::Number>((float)netdata);
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
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == cli::array<byte>::typeid)
    {
        cli::array<byte>^ buffer = (cli::array<byte>^)netdata;
        if (buffer->Length > 0)
        {
            pin_ptr<unsigned char> pinnedBuffer = &buffer[0];
            jsdata = NanNewBufferHandle((char *)pinnedBuffer, buffer->Length);
        }
        else 
        {
            jsdata = NanNewBufferHandle(0);
        }
    }
    else if (dynamic_cast<System::Collections::Generic::IDictionary<System::String^,System::Object^>^>(netdata) != nullptr)
    {
        Handle<v8::Object> result = NanNew<v8::Object>();
        for each (System::Collections::Generic::KeyValuePair<System::String^,System::Object^>^ pair 
            in (System::Collections::Generic::IDictionary<System::String^,System::Object^>^)netdata)
        {
            result->Set(stringCLR2V8(pair->Key), ClrFunc::MarshalCLRToV8(pair->Value));
        }

        jsdata = result;
    }    
    else if (dynamic_cast<System::Collections::IDictionary^>(netdata) != nullptr)
    {
        Handle<v8::Object> result = NanNew<v8::Object>();
        for each (System::Collections::DictionaryEntry^ entry in (System::Collections::IDictionary^)netdata)
        {
            if (dynamic_cast<System::String^>(entry->Key) != nullptr)
            result->Set(stringCLR2V8((System::String^)entry->Key), ClrFunc::MarshalCLRToV8(entry->Value));
        }

        jsdata = result;
    }
    else if (dynamic_cast<System::Collections::IEnumerable^>(netdata) != nullptr)
    {
        Handle<v8::Array> result = NanNew<v8::Array>();
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

    return NanEscapeScope(jsdata);
}

Handle<v8::Value> ClrFunc::MarshalCLRExceptionToV8(System::Exception^ exception)
{
    DBG("ClrFunc::MarshalCLRExceptionToV8");
    NanEscapableScope();
    Handle<v8::Object> result;
    Handle<v8::String> message;
    Handle<v8::String> name;

    if (exception == nullptr)
    {
        result = NanNew<v8::Object>();
        message = NanNew<v8::String>("Unrecognized exception thrown by CLR.");
        name = NanNew<v8::String>("InternalException");
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
    result->Set(NanNew<v8::String>("message"), message);
    
    // Recording the actual type - 'name' seems to be the common used property
    result->Set(NanNew<v8::String>("name"), name);

    return NanEscapeScope(result);
}

Handle<v8::Object> ClrFunc::MarshalCLRObjectToV8(System::Object^ netdata)
{
    DBG("ClrFunc::MarshalCLRObjectToV8");
    NanEscapableScope();
    Handle<v8::Object> result = NanNew<v8::Object>();
    System::Type^ type = netdata->GetType();

    if (0 == System::String::Compare(type->FullName, "System.Reflection.RuntimeMethodInfo")) {
        // Avoid stack overflow due to self-referencing reflection elements
        return NanEscapeScope(result);
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

    return NanEscapeScope(result);
}

System::Object^ ClrFunc::MarshalV8ToCLR(Handle<v8::Value> jsdata)
{
    NanScope();

    if (jsdata->IsFunction()) 
    {
        NodejsFunc^ functionContext = gcnew NodejsFunc(Handle<v8::Function>::Cast(jsdata));
        System::Func<System::Object^,Task<System::Object^>^>^ netfunc = 
            gcnew System::Func<System::Object^,Task<System::Object^>^>(
                functionContext, &NodejsFunc::FunctionWrapper);

        return netfunc;
    }
    else if (node::Buffer::HasInstance(jsdata))
    {
        Handle<v8::Object> jsbuffer = jsdata->ToObject();
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
        Handle<v8::Array> jsarray = Handle<v8::Array>::Cast(jsdata);
        cli::array<System::Object^>^ netarray = gcnew cli::array<System::Object^>(jsarray->Length());
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            netarray[i] = ClrFunc::MarshalV8ToCLR(jsarray->Get(i));
        }

        return netarray;
    }
    else if (jsdata->IsDate())
    {
        Handle<v8::Date> jsdate = Handle<v8::Date>::Cast(jsdata);
        long long  ticks = (long long)jsdate->NumberValue();
        long long MinDateTimeTicks = 621355968000000000;// (new DateTime(1970, 1, 1, 0, 0, 0)).Ticks;
        System::DateTime ^netobject = gcnew System::DateTime(ticks * 10000 + MinDateTimeTicks, System::DateTimeKind::Utc);
        return netobject;
    }
    else if (jsdata->IsObject()) 
    {
        IDictionary<System::String^,System::Object^>^ netobject = gcnew System::Dynamic::ExpandoObject();
        Handle<v8::Object> jsobject = Handle<v8::Object>::Cast(jsdata);
        Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
            String::Utf8Value utf8name(name);
            System::String^ netname = gcnew System::String(*utf8name);
            System::Object^ netvalue = ClrFunc::MarshalV8ToCLR(jsobject->Get(name));
            netobject->Add(netname, netvalue);
        }

        return netobject;
    }
    else if (jsdata->IsString()) 
    {
        return stringV82CLR(Handle<v8::String>::Cast(jsdata));
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

Handle<v8::Value> ClrFunc::Call(Handle<v8::Value> payload, Handle<v8::Value> callback)
{
    DBG("ClrFunc::Call instance");
    NanEscapableScope();
    
    try 
    {
        ClrFuncInvokeContext^ context = gcnew ClrFuncInvokeContext(callback);
        context->Payload = ClrFunc::MarshalV8ToCLR(payload);
        Task<System::Object^>^ task = this->func(context->Payload);
        if (task->IsCompleted)
        {
            // Completed synchronously. Return a value or invoke callback based on call pattern.
            context->Task = task;
            return NanEscapeScope(context->CompleteOnV8Thread());
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
        return NanEscapeScope(throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(e)));
    }

    return NanEscapeScope(NanUndefined());    
}
