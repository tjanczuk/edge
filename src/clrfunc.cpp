#include "edge.h"

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
            assembly = Assembly::LoadFrom(gcnew System::String(*assemblyFile));
            ClrFuncReflectionWrap^ wrap = ClrFuncReflectionWrap::Create(assembly, typeName, methodName);
            result = ClrFunc::Initialize(
                gcnew System::Func<System::Object^,Task<System::Object^>^>(
                    wrap, &ClrFuncReflectionWrap::Call));
        }
        else {
            // reference .NET code throgh embedded source code that needs to be compiled
            String::Utf8Value compilerFile(options->Get(String::NewSymbol("compiler")));
            assembly = Assembly::LoadFrom(gcnew System::String(*compilerFile));
            System::Type^ compilerType = assembly->GetType("EdgeCompiler", true, true);
            System::Object^ compilerInstance = System::Activator::CreateInstance(compilerType, false);
            MethodInfo^ compileFunc = compilerType->GetMethod("CompileFunc", BindingFlags::Instance | BindingFlags::Public);
            if (compileFunc == nullptr) 
            {
                throw gcnew System::InvalidOperationException(
                    "Unable to access the CompileFunc method of the EdgeCompiler class in the edge.js compiler assembly.");
            }

            System::Object^ parameters = ClrFunc::MarshalV8ToCLR(nullptr, options);
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
    Handle<v8::String> serialized;
    EdgeJavaScriptConverter^ converter = gcnew EdgeJavaScriptConverter();
    Handle<v8::Value> jsdata;

    try 
    {
        JavaScriptSerializer^ serializer = gcnew JavaScriptSerializer();
        serializer->RegisterConverters(gcnew cli::array<JavaScriptConverter^> { converter });
        serialized = stringCLR2V8(serializer->Serialize(netdata));
        Handle<v8::Value> argv[] = { serialized };
        jsdata = jsonParse->Call(json, 1, argv);
        if (converter->Objects->Count > 0)
        {
            // fixup object graph to replace buffer placeholders with buffers
            jsdata = converter->FixupResult(jsdata);
        }
    }
    catch (System::Exception^ e)
    {
        return scope.Close(throwV8Exception(e));
    }

    return scope.Close(jsdata);
}

System::Object^ ClrFunc::MarshalV8ToCLR(ClrFuncInvokeContext^ context, Handle<v8::Value> jsdata)
{
    HandleScope scope;

    if (jsdata->IsFunction() && context != nullptr) 
    {
        NodejsFunc^ functionContext = gcnew NodejsFunc(context, Handle<v8::Function>::Cast(jsdata));
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
            netarray[i] = ClrFunc::MarshalV8ToCLR(context, jsarray->Get(i));
        }

        return netarray;
    }
    else if (jsdata->IsObject()) 
    {
        Dictionary<System::String^,System::Object^>^ netobject = gcnew Dictionary<System::String^,System::Object^>();
        Handle<v8::Object> jsobject = Handle<v8::Object>::Cast(jsdata);
        Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
            String::Utf8Value utf8name(name);
            System::String^ netname = gcnew System::String(*utf8name);
            System::Object^ netvalue = ClrFunc::MarshalV8ToCLR(context, jsobject->Get(name));
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
        context->Payload = ClrFunc::MarshalV8ToCLR(context, payload);
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
