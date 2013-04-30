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

DWORD V8SynchronizationContext::v8ThreadId = 0;
uv_edge_async_t* V8SynchronizationContext::uv_edge_async = 0;
HANDLE V8SynchronizationContext::funcWaitHandle = 0;

void continueOnV8Thread(uv_async_t* handle, int status)
{
    // This executes on V8 thread

    DBG("continueOnV8Thread");
    HandleScope handleScope;
    uv_edge_async_t* uv_edge_async = CONTAINING_RECORD(handle, uv_edge_async_t, uv_async);
    //System::Action^ action = uv_edge_async->action;
    //MonoObject* action = mono_gc_handle_get_target (uv_edge_async->action);
    V8SynchronizationContext::CancelAction(uv_edge_async);

    //action();
    //mono_runtime_invoke (action, NULL);
}

void V8SynchronizationContext::Initialize() 
{
    // This executes on V8 thread

    DBG("V8SynchronizationContext::Initialize");
    V8SynchronizationContext::uv_edge_async = new uv_edge_async_t();
    uv_async_init(uv_default_loop(), &V8SynchronizationContext::uv_edge_async->uv_async, continueOnV8Thread);
    V8SynchronizationContext::Unref(V8SynchronizationContext::uv_edge_async);
    V8SynchronizationContext::funcWaitHandle = CreateEvent(NULL, FALSE, FALSE, NULL);
    V8SynchronizationContext::v8ThreadId = GetCurrentThreadId();
}

void V8SynchronizationContext::Unref(uv_edge_async_t* uv_edge_async)
{
    DBG("V8SynchronizationContext::Unref");
#if UV_VERSION_MAJOR==0 && UV_VERSION_MINOR<8
    uv_unref(uv_default_loop());
#else
    uv_unref((uv_handle_t*)&uv_edge_async->uv_async);
#endif  
}

uv_edge_async_t* V8SynchronizationContext::RegisterAction(MonoObject* action)
{
    DBG("V8SynchronizationContext::RegisterAction");

    if (GetCurrentThreadId() == V8SynchronizationContext::v8ThreadId)
    {
        // This executes on V8 thread.
        // Allocate new uv_edge_async.

        uv_edge_async_t* uv_edge_async = new uv_edge_async_t();
        uv_edge_async->action = mono_gchandle_new(action, FALSE);
        uv_async_init(uv_default_loop(), &uv_edge_async->uv_async, continueOnV8Thread);
        return uv_edge_async;
    }
    else 
    {
        // This executes on CLR thread. 
        // Acquire exlusive access to uv_edge_async previously initialized on V8 thread.

        WaitForSingleObject(V8SynchronizationContext::funcWaitHandle, INFINITE);
        //V8SynchronizationContext::funcWaitHandle->WaitOne();
        V8SynchronizationContext::uv_edge_async->action = mono_gchandle_new(action, FALSE);
        return V8SynchronizationContext::uv_edge_async;
    }
}

void V8SynchronizationContext::ExecuteAction(uv_edge_async_t* uv_edge_async)
{
    DBG("V8SynchronizationContext::ExecuteAction");
    // Transfer control to completeOnV8hread method executing on V8 thread
    BOOL ret = PostQueuedCompletionStatus(
        uv_default_loop()->iocp, 
        0, 
        (ULONG_PTR)NULL, 
        &uv_edge_async->uv_async.async_req.overlapped); 
}

void V8SynchronizationContext::CancelAction(uv_edge_async_t* uv_edge_async)
{
    DBG("V8SynchronizationContext::CancelAction");
    if (uv_edge_async == V8SynchronizationContext::uv_edge_async)
    {
        // This is a cancellation of an action registered in RegisterActionFromCLRThread.
        // Release the wait handle to allow the uv_edge_async reuse by another CLR thread.
        uv_edge_async->action = NULL;
        //V8SynchronizationContext::funcWaitHandle->Set();
        SetEvent(V8SynchronizationContext::funcWaitHandle);
    }
    else
    {
        // This is a cancellation of an action registered in RegisterActionFromV8Thread.
        // Unref the handle to stop preventing the process from exiting.
        V8SynchronizationContext::Unref(uv_edge_async);
        delete uv_edge_async;
    }
}

// vim: ts=4 sw=4 et: 
