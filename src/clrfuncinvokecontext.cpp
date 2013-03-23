#include "edge.h"

ClrFuncInvokeContext::ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync)
{
    DBG("ClrFuncInvokeContext::ClrFuncInvokeContext");
    if (callbackOrSync->IsFunction())
    {
        this->callback = new Persistent<Function>;
        *(this->callback) = Persistent<Function>::New(Handle<Function>::Cast(callbackOrSync));
        this->Sync = false;
    }
    else 
    {
        this->Sync = callbackOrSync->BooleanValue();
    }

    this->uv_edge_async = V8SynchronizationContext::RegisterAction(
        gcnew System::Action(this, &ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous));
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

void ClrFuncInvokeContext::CompleteOnCLRThread(System::Threading::Tasks::Task<System::Object^>^ task)
{
    DBG("ClrFuncInvokeContext::CompleteOnCLRThread");
    this->Task = task;
    V8SynchronizationContext::ExecuteAction(this->uv_edge_async);
}

void ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous()
{
    HandleScope scope;
    this->CompleteOnV8Thread(false);
}

Handle<v8::Value> ClrFuncInvokeContext::CompleteOnV8Thread(bool completedSynchronously)
{
    DBG("ClrFuncInvokeContext::CompleteOnV8Thread");

    HandleScope handleScope;
    if (completedSynchronously)
    {
        V8SynchronizationContext::CancelAction(this->uv_edge_async);
    }

    this->DisposePersistentHandles();

    if (!this->Sync && !this->callback)
    {
        // this was an async call without callback specified
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

    if (!this->Sync)
    {
        // complete the asynchronous call to C# by invoking a callback in JavaScript
        TryCatch try_catch;
        (*(this->callback))->Call(v8::Context::GetCurrent()->Global(), argc, argv);
        this->DisposeCallback();
        if (try_catch.HasCaught()) 
        {
            node::FatalException(try_catch);
        }        

        return handleScope.Close(Undefined());
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
