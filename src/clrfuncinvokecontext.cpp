#include "edge.h"

void completeOnV8Thread(uv_async_t* handle, int status)
{
    DBG("completeOnV8Thread");
    HandleScope handleScope;
    uv_edge_async_t* uv_edge_async = CONTAINING_RECORD(handle, uv_edge_async_t, uv_async);
    System::Object^ context = uv_edge_async->context;
    (dynamic_cast<ClrFuncInvokeContext^>(context))->CompleteOnV8Thread(TRUE);
}

void callFuncOnV8Thread(uv_async_t* handle, int status)
{
    DBG("continueOnCLRThread");
    HandleScope handleScope;
    uv_edge_async_t* uv_edge_async = CONTAINING_RECORD(handle, uv_edge_async_t, uv_async);
    System::Object^ context = uv_edge_async->context;
    (dynamic_cast<NodejsFuncInvokeContext^>(context))->CallFuncOnV8Thread();
}

ClrFuncInvokeContext::ClrFuncInvokeContext(Handle<Function> callback)
{
    DBG("ClrFuncInvokeContext::ClrFuncInvokeContext");
    this->callback = new Persistent<Function>;
    *(this->callback) = Persistent<Function>::New(callback);
    this->uv_edge_async = new uv_edge_async_t;
    this->uv_edge_async->context = this;
    uv_async_init(uv_default_loop(), &this->uv_edge_async->uv_async, completeOnV8Thread);
    this->funcWaitHandle = gcnew AutoResetEvent(false);
    this->uv_edge_async_func = NULL;
    this->RecreateUvEdgeAsyncFunc();
    this->persistentHandles = gcnew List<System::IntPtr>();
}

void ClrFuncInvokeContext::AddPersistentHandle(Persistent<Value>* handle)
{
    DBG("ClrFuncInvokeContext::AddPersistentHandle");
    this->persistentHandles->Add(System::IntPtr((void*)handle));
}

void ClrFuncInvokeContext::DisposePersistentHandles()
{
    DBG("ClrFuncInvokeContext::DisposePersistentHandles");

    for each (System::IntPtr wrap in this->persistentHandles)
    {
        DBG("ClrFuncInvokeContext::DisposePersistentHandles: dispose one");
        Persistent<Value>* handle = (Persistent<Value>*)wrap.ToPointer();
        (*handle).Dispose();
        (*handle).Clear();
        delete handle;
    }

    this->persistentHandles->Clear();
}

void ClrFuncInvokeContext::RecreateUvEdgeAsyncFunc()
{
    DBG("ClrFuncInvokeContext::RecreateUvEdgeAsyncFunc");
    this->DisposeUvEdgeAsyncFunc();
    this->uv_edge_async_func = new uv_edge_async_t;
    uv_async_init(uv_default_loop(), &this->uv_edge_async_func->uv_async, callFuncOnV8Thread);
    // release one CLR thread associated with this call from JS to CLR 
    // that waits to call back to an exported JS function 
    this->funcWaitHandle->Set(); 
}

uv_edge_async_t* ClrFuncInvokeContext::WaitForUvEdgeAsyncFunc()
{
    DBG("ClrFuncInvokeContext::WaitForUvEdgeAsyncFunc: start wait");
    this->funcWaitHandle->WaitOne();
    DBG("ClrFuncInvokeContext::WaitForUvEdgeAsyncFunc: end wait");
    return this->uv_edge_async_func;
}

void ClrFuncInvokeContext::DisposeCallback()
{
    if (this->callback)
    {
        DBG("ClrFuncInvokeContext::DisposeCallback");
        (*(this->callback)).Dispose();
        (*(this->callback)).Clear();
        delete this->callback;
        this->callback = NULL;        
    }
}

void ClrFuncInvokeContext::DisposeUvEdgeAsync()
{
    if (this->uv_edge_async)
    {
        DBG("ClrFuncInvokeContext::DisposeUvEdgeAsync");
        uv_unref((uv_handle_t*)&this->uv_edge_async->uv_async);
        delete this->uv_edge_async;
        this->uv_edge_async = NULL;
    }
}

void ClrFuncInvokeContext::DisposeUvEdgeAsyncFunc()
{
    if (this->uv_edge_async_func)
    {
        DBG("ClrFuncInvokeContext::DisposeUvEdgeAsyncFunc");
        uv_unref((uv_handle_t*)&this->uv_edge_async_func->uv_async);
        delete this->uv_edge_async_func;
        this->uv_edge_async_func = NULL;
    }
}

void ClrFuncInvokeContext::CompleteOnCLRThread(System::Threading::Tasks::Task<System::Object^>^ task)
{
    DBG("ClrFuncInvokeContext::CompleteOnCLRThread");
    this->Task = task;
    BOOL ret = PostQueuedCompletionStatus(
        uv_default_loop()->iocp, 
        0, 
        (ULONG_PTR)NULL, 
        &this->uv_edge_async->uv_async.async_req.overlapped);
}

Handle<v8::Value> ClrFuncInvokeContext::CompleteOnV8Thread(BOOL invokeCallback)
{
    DBG("ClrFuncInvokeContext::CompleteOnV8Thread");

    HandleScope handleScope;
    this->DisposeUvEdgeAsync();
    this->DisposeUvEdgeAsyncFunc();
    this->DisposePersistentHandles();

    if (invokeCallback && !this->callback)
    {
        return handleScope.Close(Undefined());
    }

    Handle<Value> argv[] = { Undefined(), Undefined() };
    int argc = 1;

    switch (this->Task->Status) {
        default:
            argv[0] = v8::String::New("The operation reported completion in an unexpected state.");
        break;
        case TaskStatus::Faulted:
            if (this->Task->Exception != nullptr) {
                argv[0] = exceptionCLR2stringV8(this->Task->Exception);
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
            argv[1] = ClrFunc::MarshalCLRToV8(this->Task->Result);
            if (try_catch.HasCaught()) 
            {
                argc = 1;
                argv[0] = try_catch.Exception();
            }
        break;
    };

    if (invokeCallback)
    {
        // complete the asynchronous call to C# by invoking a callback in JavaScript
        TryCatch try_catch;
        (*(this->callback))->Call(v8::Context::GetCurrent()->Global(), argc, argv);
        this->DisposeCallback();
        if (try_catch.HasCaught()) 
        {
            node::FatalException(try_catch);
        }        
    }
    else if (1 == argc) 
    {
        // complete the synchronous call to C# by re-throwing the resulting exception
        return handleScope.Close(ThrowException(argv[0]));
    }
    else
    {
        // complete the synchronous call to C# by returning the result
        return handleScope.Close(argv[1]);
    }
}
