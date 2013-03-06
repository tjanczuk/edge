#include "owin.h"

Handle<v8::String> convert(System::String^ text)
{
    HandleScope scope;
    pin_ptr<const wchar_t> message = PtrToStringChars(text);
    return scope.Close(v8::String::New((uint16_t*)message));  
}

System::String^ convertV82CLR(Handle<v8::String> text)
{
    HandleScope scope;
    String::Utf8Value utf8text(text);
    return gcnew System::String(*utf8text);    
}

System::String^ v8ExceptionToCLRString(Handle<v8::Value> exception)
{
    HandleScope scope;
    if (exception->IsObject())
    {
        Handle<Value> stack = exception->ToObject()->Get(v8::String::NewSymbol("stack"));
        if (stack->IsString())
        {
            return gcnew System::String(convertV82CLR(stack->ToString()));
        }
    }

    return gcnew System::String(convertV82CLR(Handle<v8::String>::Cast(exception)));
}

Handle<String> createV8ExceptionString(System::Exception^ exception)
{
    HandleScope scope;
    if (exception == nullptr)
    {
        return scope.Close(v8::String::New("Unrecognized exception thrown by CLR."));
    }
    else
    {
        return scope.Close(convert(exception->ToString()));
    }
}

Handle<Value> createV8Exception(System::Exception^ exception)
{
    HandleScope scope;
    return scope.Close(ThrowException(createV8ExceptionString(exception)));
}

void completeOnV8Thread(uv_async_t* handle, int status)
{
    if (debugMode) 
        System::Console::WriteLine("completeOnV8Thread");

    HandleScope handleScope;
    uv_owin_async_t* uv_owin_async = CONTAINING_RECORD(handle, uv_owin_async_t, uv_async);
    System::Object^ context = uv_owin_async->context;
    (dynamic_cast<OwinAppInvokeContext^>(context))->CompleteOnV8Thread();
}

void callFuncOnV8Thread(uv_async_t* handle, int status)
{
    if (debugMode) 
        System::Console::WriteLine("continueOnCLRThread");

    HandleScope handleScope;
    uv_owin_async_t* uv_owin_async = CONTAINING_RECORD(handle, uv_owin_async_t, uv_async);
    System::Object^ context = uv_owin_async->context;
    (dynamic_cast<NodejsFunctionInvocationContext^>(context))->CallFuncOnV8Thread();
}

NodejsFunctionInvocationContext::NodejsFunctionInvocationContext(
        NodejsFunctionContext^ functionContext, System::Object^ payload)
{
    this->functionContext = functionContext;
    this->payload = payload;
    this->TaskCompletionSource = gcnew System::Threading::Tasks::TaskCompletionSource<System::Object^>();
    this->wrap = new NodejsFunctionInvocationContextWrap;
    this->wrap->context = this;
}

NodejsFunctionInvocationContext::~NodejsFunctionInvocationContext()
{
    this->!NodejsFunctionInvocationContext();
}

NodejsFunctionInvocationContext::!NodejsFunctionInvocationContext()
{
    if (this->wrap)
    {
        delete this->wrap;
        this->wrap = NULL;
    }
}

Handle<Value> v8FuncCallback(const v8::Arguments& args)
{
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_owinContext")));
    NodejsFunctionInvocationContextWrap* wrap = (NodejsFunctionInvocationContextWrap*)(correlator->Value());
    NodejsFunctionInvocationContext^ context = wrap->context;
    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
        context->CompleteWithError(gcnew System::Exception(v8ExceptionToCLRString(args[0])));
    }
    else 
    {
        context->CompleteWithResult(args[1]);
    }

    return scope.Close(Undefined());
}

void NodejsFunctionInvocationContext::CallFuncOnV8Thread()
{
    HandleScope scope;
    this->functionContext->AppInvokeContext->RecreateUvOwinAsyncFunc();
    try 
    {
        TryCatch try_catch;
        Handle<v8::Value> jspayload = OwinApp::MarshalCLRToV8(this->payload);
        if (try_catch.HasCaught()) 
        {
            throw gcnew System::Exception("Unable to convert CLR value to V8 value.");
        }

        Handle<v8::FunctionTemplate> callbackTemplate = v8::FunctionTemplate::New(v8FuncCallback);
        Handle<v8::Function> callback = callbackTemplate->GetFunction();
        callback->Set(v8::String::NewSymbol("_owinContext"), v8::External::New((void*)this->wrap));
        Handle<v8::Value> argv[] = { jspayload, callback };
        TryCatch tryCatch;
        (*(this->functionContext->Func))->Call(v8::Context::GetCurrent()->Global(), 2, argv);
        if (tryCatch.HasCaught()) 
        {
            this->CompleteWithError(gcnew System::Exception(v8ExceptionToCLRString(tryCatch.Exception())));
        }
    }
    catch (System::Exception^ ex)
    {
        this->CompleteWithError(ex);
    }
}

void NodejsFunctionInvocationContext::Complete()
{
    if (this->exception != nullptr)
    {
        this->TaskCompletionSource->SetException(this->exception);
    }
    else 
    {
        this->TaskCompletionSource->SetResult(this->result);
    }
}

void NodejsFunctionInvocationContext::CompleteWithError(System::Exception^ exception)
{
    this->exception = exception;
    Task::Run(gcnew System::Action(this, &NodejsFunctionInvocationContext::Complete));
}

void NodejsFunctionInvocationContext::CompleteWithResult(Handle<v8::Value> result)
{
    try 
    {
        this->result = OwinApp::MarshalV8ToCLR(nullptr, result);
        Task::Run(gcnew System::Action(this, &NodejsFunctionInvocationContext::Complete));
    }
    catch (System::Exception^ e)
    {
        this->CompleteWithError(e);
    }
}

NodejsFunctionContext::NodejsFunctionContext(OwinAppInvokeContext^ appInvokeContext, Handle<Function> function)
{
    this->AppInvokeContext = appInvokeContext;
    this->Func = new Persistent<Function>;
    *(this->Func) = Persistent<Function>::New(function);
}

NodejsFunctionContext::~NodejsFunctionContext()
{
    if (debugMode)
        System::Console::WriteLine("~NodejsFunctionContext");
    
    this->!NodejsFunctionContext();
}

NodejsFunctionContext::!NodejsFunctionContext()
{
    if (debugMode)
        System::Console::WriteLine("!NodejsFunctionContext");

    this->DisposeFunction();
}

void NodejsFunctionContext::DisposeFunction()
{
    if (this->Func)
    {
        if (debugMode)
            System::Console::WriteLine("DisposeFunction");

        (*(this->Func)).Dispose();
        (*(this->Func)).Clear();
        delete this->Func;
        this->Func = NULL;        
    }
}

Task<System::Object^>^ NodejsFunctionContext::FunctionWrapper(System::Object^ payload)
{
    uv_owin_async_t* uv_owin_async = this->AppInvokeContext->WaitForUvOwinAsyncFunc();
    NodejsFunctionInvocationContext^ context = gcnew NodejsFunctionInvocationContext(this, payload);
    uv_owin_async->context = context;
    BOOL ret = PostQueuedCompletionStatus(
        uv_default_loop()->iocp, 
        0, 
        (ULONG_PTR)NULL, 
        &uv_owin_async->uv_async.async_req.overlapped);

    return context->TaskCompletionSource->Task;
}

OwinAppInvokeContext::OwinAppInvokeContext(Handle<Function> callback)
{
    this->callback = new Persistent<Function>;
    *(this->callback) = Persistent<Function>::New(callback);
    this->uv_owin_async = new uv_owin_async_t;
    this->uv_owin_async->context = this;
    uv_async_init(uv_default_loop(), &this->uv_owin_async->uv_async, completeOnV8Thread);
    this->funcWaitHandle = gcnew AutoResetEvent(false);
    this->uv_owin_async_func = NULL;
    this->RecreateUvOwinAsyncFunc();
}

OwinAppInvokeContext::~OwinAppInvokeContext()
{
    if (debugMode)
        System::Console::WriteLine("~OwinAppInvokeContext");
    
    this->!OwinAppInvokeContext();
}

OwinAppInvokeContext::!OwinAppInvokeContext()
{
    if (debugMode)
        System::Console::WriteLine("!OwinAppInvokeContext");

    this->DisposeCallback();
    this->DisposeUvOwinAsync();
    this->DisposeUvOwinAsyncFunc();
}

void OwinAppInvokeContext::RecreateUvOwinAsyncFunc()
{
    this->DisposeUvOwinAsyncFunc();
    this->uv_owin_async_func = new uv_owin_async_t;
    uv_async_init(uv_default_loop(), &this->uv_owin_async_func->uv_async, callFuncOnV8Thread);
    // release one CLR thread associated with this call from JS to CLR 
    // that waits to call back to an exported JS function 
    this->funcWaitHandle->Set(); 
}

uv_owin_async_t* OwinAppInvokeContext::WaitForUvOwinAsyncFunc()
{
    this->funcWaitHandle->WaitOne();
    return this->uv_owin_async_func;
}

void OwinAppInvokeContext::DisposeCallback()
{
    if (this->callback)
    {
        if (debugMode)
            System::Console::WriteLine("Disposing callback");

        (*(this->callback)).Dispose();
        (*(this->callback)).Clear();
        delete this->callback;
        this->callback = NULL;        
    }
}

void OwinAppInvokeContext::DisposeUvOwinAsync()
{
    if (this->uv_owin_async)
    {
        if (debugMode)
            System::Console::WriteLine("Disposing uv_owin_async");

        uv_unref((uv_handle_t*)&this->uv_owin_async->uv_async);
        delete this->uv_owin_async;
        this->uv_owin_async = NULL;
    }
}

void OwinAppInvokeContext::DisposeUvOwinAsyncFunc()
{
    if (this->uv_owin_async_func)
    {
        if (debugMode)
            System::Console::WriteLine("Disposing uv_owin_async_func");

        uv_unref((uv_handle_t*)&this->uv_owin_async_func->uv_async);
        delete this->uv_owin_async_func;
        this->uv_owin_async_func = NULL;
    }
}

void OwinAppInvokeContext::CompleteOnCLRThread(Task<System::Object^>^ task)
{
    if (debugMode)
        System::Console::WriteLine("CompleteOnCLRThread");

    // TODO: what prevents GC collection of "this" during the thread switch? 
    // Does the gcroot in uv_owin_async->context ensure that?
    this->task = task;
    BOOL ret = PostQueuedCompletionStatus(
        uv_default_loop()->iocp, 
        0, 
        (ULONG_PTR)NULL, 
        &this->uv_owin_async->uv_async.async_req.overlapped);
}

void OwinAppInvokeContext::CompleteOnV8Thread()
{
    if (debugMode)
        System::Console::WriteLine("CompleteOnV8Thread");

    HandleScope handleScope;

    this->DisposeUvOwinAsync();

    if (this->callback) 
    {
        Handle<Value> argv[] = { Undefined(), Undefined() };
        int argc = 1;

        switch (this->task->Status) {
            default:
                argv[0] = v8::String::New("The operation reported completion in an unexpected state.");
            break;
            case TaskStatus::Faulted:
                if (this->task->Exception != nullptr) {
                    argv[0] = createV8ExceptionString(this->task->Exception);
                }
                else {
                    argv[0] = v8::String::New("The operation has failed with an undetermined error.");
                }
            break;
            case TaskStatus::Canceled:
                argv[0] = v8::String::New("The operation was cancelled.");
            break;
            case TaskStatus::RanToCompletion:
                argc = 2;
                TryCatch try_catch;
                argv[1] = OwinApp::MarshalCLRToV8(this->task->Result);
                if (try_catch.HasCaught()) 
                {
                    argc = 1;
                    argv[0] = try_catch.Exception();
                }
            break;
        };

        TryCatch try_catch;
        (*(this->callback))->Call(v8::Context::GetCurrent()->Global(), argc, argv);
        this->DisposeCallback();
        if (try_catch.HasCaught()) 
        {
            node::FatalException(try_catch);
        }        
    }
}

static OwinApp::OwinApp()
{
    OwinApp::apps = gcnew List<OwinApp^>();
}

OwinApp::OwinApp()
{
    // empty
}

Handle<Value> OwinApp::Initialize(const v8::Arguments& args)
{
    HandleScope scope;
    Handle<v8::Object> options = args[0]->ToObject();
    try 
    {
        String::Utf8Value assemblyFile(options->Get(String::NewSymbol("assemblyFile")));
        String::Utf8Value typeName(options->Get(String::NewSymbol("typeName")));
        String::Utf8Value methodName(options->Get(String::NewSymbol("methodName")));
        Assembly^ assembly = Assembly::LoadFrom(gcnew System::String(*assemblyFile));
        System::Type^ startupType = assembly->GetType(gcnew System::String(*typeName), true, true);
        OwinApp^ app = gcnew OwinApp();
        app->instance = System::Activator::CreateInstance(startupType, false);
        app->invokeMethod = startupType->GetMethod(gcnew System::String(*methodName), 
            BindingFlags::Instance | BindingFlags::Public);
		if (app->invokeMethod == nullptr) 
		{
			throw gcnew System::InvalidOperationException("Unable to access to the CLR method to wrap through reflection. Make sure it is a public instance method.");
		}

        OwinApp::apps->Add(app);
    }
    catch (System::Exception^ e)
    {
        return scope.Close(createV8Exception(e));
    }

    return scope.Close(Integer::New(OwinApp::apps->Count));
}

void owinAppCompletedOnCLRThread(Task<System::Object^>^ task, System::Object^ state)
{
    OwinAppInvokeContext^ context = (OwinAppInvokeContext^)state;
    context->CompleteOnCLRThread(task);
}

Handle<v8::Value> OwinApp::MarshalCLRToV8(System::Object^ netdata)
{
    HandleScope scope;
    Handle<v8::String> serialized;

    try 
    {
        JavaScriptSerializer^ serializer = gcnew JavaScriptSerializer();
        serialized = convert(serializer->Serialize(netdata));
    }
    catch (System::Exception^ e)
    {
        return scope.Close(createV8Exception(e));
    }

    Handle<v8::Value> argv[] = { serialized };

    return scope.Close(jsonParse->Call(json, 1, argv));
}

System::Object^ OwinApp::MarshalV8ToCLR(OwinAppInvokeContext^ context, Handle<v8::Value> jsdata)
{
    HandleScope scope;

    if (jsdata->IsFunction() && context != nullptr) 
    {
        NodejsFunctionContext^ functionContext = gcnew NodejsFunctionContext(
            context, Handle<v8::Function>::Cast(jsdata));
        System::Func<System::Object^,Task<System::Object^>^>^ netfunc = 
            gcnew System::Func<System::Object^,Task<System::Object^>^>(
                functionContext, &NodejsFunctionContext::FunctionWrapper);

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
            System::Object^ netvalue = OwinApp::MarshalV8ToCLR(context, jsobject->Get(name));
            netobject->Add(netname, netvalue);
        }

        return netobject;
    }
    else if (jsdata->IsArray())
    {
        Handle<v8::Array> jsarray = Handle<v8::Array>::Cast(jsdata);
        cli::array<System::Object^>^ netarray = gcnew cli::array<System::Object^>(jsarray->Length());
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            netarray[i] = OwinApp::MarshalV8ToCLR(context, jsarray->Get(i));
        }

        return netarray;
    }
    else if (jsdata->IsString()) 
    {
        return convertV82CLR(Handle<v8::String>::Cast(jsdata));
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

Handle<Value> OwinApp::Call(const Arguments& args) 
{
    HandleScope scope;
    
    try 
    {
        int appId = args[0]->Int32Value();
        OwinAppInvokeContext^ context = gcnew OwinAppInvokeContext(Handle<v8::Function>::Cast(args[2]));
        context->Payload = OwinApp::MarshalV8ToCLR(context, args[1]);
        OwinApp^ app = OwinApp::apps->default[appId - 1];
        Task<System::Object^>^ task = (Task<System::Object^>^)app->invokeMethod->Invoke(
            app->instance, gcnew array<System::Object^> { context->Payload });
        task->ContinueWith(gcnew System::Action<Task<System::Object^>^,System::Object^>(owinAppCompletedOnCLRThread), context);
    }
    catch (System::Exception^ e)
    {
        return scope.Close(createV8Exception(e));
    }

    return scope.Close(Undefined());
}

Handle<Value> initializeOwinApp(const v8::Arguments& args)
{
    return OwinApp::Initialize(args);
}

Handle<Value> callOwinApp(const v8::Arguments& args)
{
    return OwinApp::Call(args);
}

void init(Handle<Object> target) 
{
    bufferConstructor = Persistent<Function>::New(Handle<Function>::Cast(
        Context::GetCurrent()->Global()->Get(String::New("Buffer")))); 
    json = Persistent<v8::Object>::New(Context::GetCurrent()->Global()->Get(String::New("JSON"))->ToObject());
    jsonParse = Persistent<Function>::New(Handle<Function>::Cast(json->Get(String::New("parse"))));
    debugMode = (0 < GetEnvironmentVariable("OWIN_DEBUG", NULL, 0));
    NODE_SET_METHOD(target, "initializeOwinApp", initializeOwinApp);
    NODE_SET_METHOD(target, "callOwinApp", callOwinApp);
}

NODE_MODULE(owin, init);
