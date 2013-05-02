/**
 * Portions Copyright (c) Microsoft Corporation. All rights reserved. 
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0  
 *
 * THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION 
 * ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR 
 * PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
 *
 * See the Apache Version 2.0 License for specific language governing 
 * permissions and limitations under the License.
 */
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

    ClrActionContext* data = new ClrActionContext;
    data->action = gcnew System::Action(this, &ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous);
    this->uv_edge_async = V8SynchronizationContext::RegisterAction(ClrActionContext::ActionCallback, data);
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
            try {
                argv[1] = ClrFunc::MarshalCLRToV8(this->Task->Result);
            }
            catch (System::Exception^ e) {
                argc = 1;
                argv[0] = exceptionCLR2stringV8(e);
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
