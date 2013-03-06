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
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[0]);
    NodejsFunctionInvocationContextWrap* wrap = (NodejsFunctionInvocationContextWrap*)(correlator->Value());
    NodejsFunctionInvocationContext^ context = wrap->context;
    if (args.Length() > 1 && !args[1]->IsUndefined() && !args[1]->IsNull())
    {
        context->CompleteWithError(gcnew System::Exception(
            convertV82CLR(Handle<v8::String>::Cast(args[1]))));
    }
    else if (args.Length() > 2)
    {
        context->CompleteWithResult(Handle<v8::String>::Cast(args[2]));
    }
    else 
    {
        context->CompleteEmpty();
    }

    return scope.Close(Undefined());
}

void NodejsFunctionInvocationContext::CallFuncOnV8Thread()
{
    HandleScope scope;
    this->functionContext->AppInvokeContext->RecreateUvOwinAsyncFunc();
    try 
    {
        JavaScriptSerializer^ serializer = gcnew JavaScriptSerializer();
        Handle<v8::String> payload = convert(serializer->Serialize(this->payload));
        Handle<v8::FunctionTemplate> callbackTemplate = v8::FunctionTemplate::New(v8FuncCallback);
        Handle<v8::Function> callback = callbackTemplate->GetFunction();
        Handle<v8::Value> argv[] = { v8::External::New((void*)this->wrap), payload, callback };
        TryCatch tryCatch;
        (*(this->functionContext->Func))->Call(v8::Context::GetCurrent()->Global(), 3, argv);
        if (tryCatch.HasCaught()) {
            this->CompleteWithError(gcnew System::Exception(
                convertV82CLR(Handle<v8::String>::Cast(tryCatch.Exception()))));
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

void NodejsFunctionInvocationContext::CompleteEmpty()
{
    Task::Run(gcnew System::Action(this, &NodejsFunctionInvocationContext::Complete));
}

void NodejsFunctionInvocationContext::CompleteWithError(System::Exception^ exception)
{
    this->exception = exception;
    Task::Run(gcnew System::Action(this, &NodejsFunctionInvocationContext::Complete));
}

void NodejsFunctionInvocationContext::CompleteWithResult(Handle<v8::String> result)
{
    HandleScope scope;
    JavaScriptSerializer^ serializer = gcnew JavaScriptSerializer();
    try 
    {
        this->result = serializer->DeserializeObject(convertV82CLR(result));
    }
    catch (System::Exception^ ex)
    {
        this->CompleteWithError(ex);
    }

    if (this->result != nullptr)
    {
        Task::Run(gcnew System::Action(this, &NodejsFunctionInvocationContext::Complete));
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

OwinAppInvokeContext::OwinAppInvokeContext(Dictionary<System::String^,System::Object^>^ netenv, Handle<Function> callback)
{
    this->responseHeaders = gcnew Dictionary<System::String^,array<System::String^>^>();
    this->responseBody = gcnew System::IO::MemoryStream();
    this->netenv = netenv;
    this->netenv->Add(gcnew System::String("owin.ResponseHeaders"), this->responseHeaders);
    this->netenv->Add(gcnew System::String("owin.ResponseBody"), this->responseBody);
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
    // release one CLR thread associated with this OWIN call that waits to call a JS function 
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

void OwinAppInvokeContext::CompleteOnCLRThread(Task^ task)
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

Handle<v8::Object> OwinAppInvokeContext::CreateResultObject()
{
    HandleScope scope;
    Handle<v8::Object> result = v8::Object::New();
    for each (KeyValuePair<System::String^,System::Object^>^ entry in this->netenv) {
        if (entry->Value->GetType() == System::String::typeid) 
        {
            result->Set(convert(entry->Key), convert((System::String^)entry->Value));
        }
        else if (entry->Value->GetType() == System::Int32::typeid) 
        {
            result->Set(convert(entry->Key), v8::Integer::New((int)entry->Value));            
        }
    }

    Handle<v8::Object> headers = v8::Object::New();
    for each (KeyValuePair<System::String^,array<System::String^>^>^ netheader in 
        (Dictionary<System::String^,array<System::String^>^>^)this->responseHeaders) 
    {
        // TODO: support for multiple value of a single HTTP header
        headers->Set(convert(netheader->Key), convert(netheader->Value[0]));
    }

    result->Set(v8::String::New("owin.ResponseHeaders"), headers);            

    if (this->responseBody->Length > 0) 
    {        
        pin_ptr<unsigned char> pinnedBuffer = &this->responseBody->GetBuffer()[0];
        node::Buffer* slowBuffer = node::Buffer::New(this->responseBody->Length);
        memcpy(node::Buffer::Data(slowBuffer), pinnedBuffer, this->responseBody->Length);
        Handle<v8::Value> args[] = { 
            slowBuffer->handle_, 
            v8::Integer::New(this->responseBody->Length), 
            Integer::New(0) 
        };
        Handle<v8::Object> fastBuffer = bufferConstructor->NewInstance(3, args);
        this->responseBody->Close();
        result->Set(v8::String::New("owin.ResponseBody"), fastBuffer);            
    }

    return scope.Close(result);
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
                argv[1] = this->CreateResultObject();
            break;
        };

        TryCatch try_catch;
        (*(this->callback))->Call(v8::Context::GetCurrent()->Global(), argc, argv);
        this->DisposeCallback();
        if (try_catch.HasCaught()) {
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
        Assembly^ assembly = Assembly::LoadFrom(gcnew System::String(*assemblyFile));
        System::Type^ startupType = assembly->GetType(gcnew System::String(*typeName), true, true);
        OwinApp^ app = gcnew OwinApp();
        app->instance = System::Activator::CreateInstance(startupType, false);
        app->invokeMethod = startupType->GetMethod("Invoke", BindingFlags::Instance | BindingFlags::Public);
        OwinApp::apps->Add(app);
    }
    catch (System::Exception^ e)
    {
        return scope.Close(createV8Exception(e));
    }

    return scope.Close(Integer::New(OwinApp::apps->Count));
}

void owinAppCompletedOnCLRThread(Task^ task, System::Object^ state)
{
    OwinAppInvokeContext^ context = (OwinAppInvokeContext^)state;
    context->CompleteOnCLRThread(task);
}

Handle<Value> OwinApp::Call(const Arguments& args) 
{
    HandleScope scope;
    int appId = args[0]->Int32Value();
    Handle<v8::Object> jsenv = Handle<v8::Object>::Cast(args[1]);
    Handle<v8::Function> callback = Handle<v8::Function>::Cast(args[2]);
    Dictionary<System::String^,System::Object^>^ netenv = gcnew Dictionary<System::String^,System::Object^>();
    OwinAppInvokeContext^ context = gcnew OwinAppInvokeContext(netenv, callback);
    Handle<v8::Array> propertyNames = jsenv->GetPropertyNames();
    for (unsigned int i = 0; i < propertyNames->Length(); i++)
    {
        Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
        String::Utf8Value utf8name(name);
        System::String^ netname = gcnew System::String(*utf8name);
        System::Object^ netvalue;

        if (netname == "owin.RequestHeaders") 
        {
            Handle<v8::Object> headers = Handle<v8::Object>::Cast(jsenv->Get(name));
            Dictionary<System::String^,array<System::String^>^>^ netheaders = 
                gcnew Dictionary<System::String^,array<System::String^>^>();
            Handle<v8::Array> headerNames = headers->GetPropertyNames();
            for (unsigned int j = 0; j < headerNames->Length(); j++)
            {
                Handle<v8::String> headerName = Handle<v8::String>::Cast(headerNames->Get(j));
                String::Utf8Value utf8headerName(headerName);
                String::Utf8Value utf8headerValue(Handle<v8::String>::Cast(headers->Get(headerName)));
                System::String^ netHeaderName = gcnew System::String(*utf8headerName);
                System::String^ netHeaderValue = gcnew System::String(*utf8headerValue);
                netheaders->Add(netHeaderName, gcnew array<System::String^> { netHeaderValue });
            }

            netvalue = netheaders;
        }
        else if (netname == "nodejs.Functions")
        {
            Handle<v8::Object> functions = Handle<v8::Object>::Cast(jsenv->Get(name));
            Dictionary<System::String^,System::Func<System::Object^,Task<System::Object^>^>^>^ netfunctions = 
                gcnew Dictionary<System::String^,System::Func<System::Object^,Task<System::Object^>^>^>();
            Handle<v8::Array> functionNames = functions->GetPropertyNames();
            for (unsigned int j = 0; j < functionNames->Length(); j++)
            {
                Handle<v8::String> functionName = Handle<v8::String>::Cast(functionNames->Get(j));
                String::Utf8Value utf8functionName(functionName);
                NodejsFunctionContext^ functionContext = gcnew NodejsFunctionContext(
                    context, Handle<v8::Function>::Cast(functions->Get(functionName)));
                System::String^ netFunctionName = gcnew System::String(*utf8functionName);
                netfunctions->Add(netFunctionName, gcnew System::Func<System::Object^,Task<System::Object^>^>(
                    functionContext, &NodejsFunctionContext::FunctionWrapper));
            }

            netvalue = netfunctions;
        }
        else 
        {
            String::Utf8Value utf8value(Handle<v8::String>::Cast(jsenv->Get(name)));
            netvalue = gcnew System::String(*utf8value);
            if (netname == "owin.RequestBody") 
            {
                netvalue = gcnew System::IO::MemoryStream(
                    System::Text::Encoding::UTF8->GetBytes((System::String^)netvalue));
            }
        }
        
        netenv->Add(netname, netvalue);
    }

    try 
    {
        OwinApp^ app = OwinApp::apps->default[appId - 1];
        Task^ task = (Task^)app->invokeMethod->Invoke(app->instance, gcnew array<System::Object^> { netenv });
        task->ContinueWith(gcnew System::Action<Task^,System::Object^>(owinAppCompletedOnCLRThread), context);
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
    debugMode = (0 < GetEnvironmentVariable("OWIN_DEBUG", NULL, 0));
    NODE_SET_METHOD(target, "initializeOwinApp", initializeOwinApp);
    NODE_SET_METHOD(target, "callOwinApp", callOwinApp);
}

NODE_MODULE(owin, init);
