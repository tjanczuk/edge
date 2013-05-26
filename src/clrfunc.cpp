#include "edge.h"

#using <System.Core.dll>

ClrFunc::ClrFunc()
{
    // empty
}

Handle<v8::Value> clrFuncProxy(const v8::Arguments& args)
{
    DBG("clrFuncProxy");
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_edgeContext")));
    ClrFuncWrap* wrap = (ClrFuncWrap*)(correlator->Value());
    ClrFunc^ clrFunc = wrap->clrFunc;
    return scope.Close(clrFunc->Call(args[0], args[1]));
}

void clrFuncProxyNearDeath(v8::Persistent<v8::Value> object, void* parameters)
{
    DBG("clrFuncProxyNearDeath");
    ClrFuncWrap* wrap = (ClrFuncWrap*)parameters;
    object.Dispose();
    object.Clear();
    wrap->clrFunc = nullptr;
    delete wrap;
}

Handle<v8::Function> ClrFunc::Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func)
{
    DBG("ClrFunc::Initialize Func<object,Task<object>> wrapper");

    HandleScope scope;

    ClrFunc^ app = gcnew ClrFunc();
    app->func = func;
    ClrFuncWrap* wrap = new ClrFuncWrap;
    wrap->clrFunc = app;    
    v8::Persistent<v8::Function> funcProxy = v8::Persistent<v8::Function>::New(
        FunctionTemplate::New(clrFuncProxy)->GetFunction());
    funcProxy->Set(v8::String::NewSymbol("_edgeContext"), v8::External::New((void*)wrap));
    funcProxy.MakeWeak((void*)wrap, clrFuncProxyNearDeath);
    return scope.Close(funcProxy);
}

Handle<v8::Value> ClrFunc::Initialize(const v8::Arguments& args)
{
    DBG("ClrFunc::Initialize MethodInfo wrapper");

    HandleScope scope;
    Handle<v8::Object> options = args[0]->ToObject();
    Assembly^ assembly;
    System::String^ typeName;
    System::String^ methodName;

    try 
    {
        Handle<v8::Function> result;

        Handle<v8::Value> jsassemblyFile = options->Get(String::NewSymbol("assemblyFile"));
        if (jsassemblyFile->IsString()) {
            // reference .NET code through pre-compiled CLR assembly 
            String::Utf8Value assemblyFile(jsassemblyFile);
            String::Utf8Value nativeTypeName(options->Get(String::NewSymbol("typeName")));
            String::Utf8Value nativeMethodName(options->Get(String::NewSymbol("methodName")));  
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
            String::Utf8Value compilerFile(options->Get(String::NewSymbol("compiler")));
            assembly = Assembly::UnsafeLoadFrom(gcnew System::String(*compilerFile));
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

        return scope.Close(result);
    }
    catch (System::Exception^ e)
    {
        return scope.Close(throwV8Exception(e));
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
    HandleScope scope;
    Handle<v8::Value> jsdata;

    if (netdata == nullptr)
    {
        return scope.Close(Null());
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
        jsdata = v8::Boolean::New((bool)netdata);
    }
    else if (type == System::Guid::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (type == System::DateTime::typeid)
    {
        jsdata = stringCLR2V8(netdata->ToString());
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
        jsdata = v8::Integer::New((int)netdata);
    }
    else if (type == System::Int64::typeid)
    {
        jsdata = v8::Number::New(((System::IConvertible^)netdata)->ToDouble(nullptr));
    }
    else if (type == double::typeid)
    {
        jsdata = v8::Number::New((double)netdata);
    }
    else if (type == float::typeid)
    {
        jsdata = v8::Number::New((float)netdata);
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
        pin_ptr<unsigned char> pinnedBuffer = &buffer[0];
        node::Buffer* slowBuffer = node::Buffer::New(buffer->Length);
        memcpy(node::Buffer::Data(slowBuffer), pinnedBuffer, buffer->Length);
        Handle<v8::Value> args[] = { 
            slowBuffer->handle_, 
            v8::Integer::New(buffer->Length), 
            v8::Integer::New(0) 
        };
        jsdata = bufferConstructor->NewInstance(3, args);    
    }
    else if (dynamic_cast<System::Collections::IDictionary^>(netdata) != nullptr)
    {
        Handle<v8::Object> result = v8::Object::New();
        for each (System::Collections::DictionaryEntry^ entry in (System::Collections::IDictionary^)netdata)
        {
            if (dynamic_cast<System::String^>(entry->Key) != nullptr)
            result->Set(stringCLR2V8((System::String^)entry->Key), ClrFunc::MarshalCLRToV8(entry->Value));
        }

        jsdata = result;
    }
    else if (dynamic_cast<System::Collections::IEnumerable^>(netdata) != nullptr)
    {
        Handle<v8::Array> result = v8::Array::New();
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
    else
    {
        jsdata = ClrFunc::MarshalCLRObjectToV8(netdata);
    }

    return scope.Close(jsdata);
}

Handle<v8::Value> ClrFunc::MarshalCLRObjectToV8(System::Object^ netdata)
{
    HandleScope scope;
    Handle<v8::Object> result = v8::Object::New();
    System::Type^ type = netdata->GetType();

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

    return scope.Close(result);
}

System::Object^ ClrFunc::MarshalV8ToCLR(Handle<v8::Value> jsdata)
{
    HandleScope scope;

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
        pin_ptr<byte> pinnedNetbuffer = &netbuffer[0];
        memcpy(pinnedNetbuffer, node::Buffer::Data(jsbuffer), netbuffer->Length);

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
    HandleScope scope;
    
    try 
    {
        ClrFuncInvokeContext^ context = gcnew ClrFuncInvokeContext(callback);
        context->Payload = ClrFunc::MarshalV8ToCLR(payload);
        Task<System::Object^>^ task = this->func(context->Payload);
        if (task->IsCompleted)
        {
            // Completed synchronously. Return a value or invoke callback based on call pattern.
            context->Task = task;
            return scope.Close(context->CompleteOnV8Thread(true));
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
            // Will complete asynchronously. Schedule continuation to finish processing.
            task->ContinueWith(gcnew System::Action<Task<System::Object^>^,System::Object^>(
                edgeAppCompletedOnCLRThread), context);
        }
    }
    catch (System::Exception^ e)
    {
        return scope.Close(throwV8Exception(e));
    }

    return scope.Close(Undefined());    
}
