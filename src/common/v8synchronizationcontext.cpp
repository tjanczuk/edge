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
#include "edge_common.h"

void continueOnV8Thread(uv_async_t* handle, int status)
{
    // This executes on V8 thread

    DBG("continueOnV8Thread");
    Nan::HandleScope scope;
    uv_edge_async_t* uv_edge_async = (uv_edge_async_t*)handle;
    uv_async_edge_cb action = uv_edge_async->action;
    void* data = uv_edge_async->data;
    V8SynchronizationContext::CancelAction(uv_edge_async);
    action(data);
}

unsigned long V8SynchronizationContext::v8ThreadId;
uv_sem_t* V8SynchronizationContext::funcWaitHandle;
uv_edge_async_t* V8SynchronizationContext::uv_edge_async;

void V8SynchronizationContext::Initialize()
{
    // This executes on V8 thread

    DBG("V8SynchronizationContext::Initialize");
    V8SynchronizationContext::uv_edge_async = new uv_edge_async_t;
    uv_edge_async->singleton = TRUE;
    uv_async_init(uv_default_loop(), &V8SynchronizationContext::uv_edge_async->uv_async, (uv_async_cb)continueOnV8Thread);
    V8SynchronizationContext::Unref(V8SynchronizationContext::uv_edge_async);
    V8SynchronizationContext::funcWaitHandle = new uv_sem_t;
    uv_sem_init(V8SynchronizationContext::funcWaitHandle, 1);
    V8SynchronizationContext::v8ThreadId = V8SynchronizationContext::GetCurrentThreadId();
}

void V8SynchronizationContext::Unref(uv_edge_async_t* uv_edge_async)
{
    DBG("V8SynchronizationContext::Unref");
    uv_unref((uv_handle_t*)&uv_edge_async->uv_async);
}

uv_edge_async_t* V8SynchronizationContext::RegisterAction(uv_async_edge_cb action, void* data)
{
    DBG("V8SynchronizationContext::RegisterAction");

    if (V8SynchronizationContext::GetCurrentThreadId() == V8SynchronizationContext::v8ThreadId)
    {
        // This executes on V8 thread.
        // Allocate new uv_edge_async.
        DBG("V8SynchronizationContext::RegisterAction on v8 thread");
        uv_edge_async_t* uv_edge_async = new uv_edge_async_t;
        uv_edge_async->action = action;
        uv_edge_async->data = data;
        uv_edge_async->singleton = FALSE;
        uv_async_init(uv_default_loop(), &uv_edge_async->uv_async, (uv_async_cb)continueOnV8Thread);
        return uv_edge_async;
    }
    else
    {
        // This executes on CLR thread.
        // Acquire exlusive access to uv_edge_async previously initialized on V8 thread.
        DBG("V8SynchronizationContext::RegisterAction on CLR thread");
        uv_sem_wait(V8SynchronizationContext::funcWaitHandle);
        V8SynchronizationContext::uv_edge_async->action = action;
        V8SynchronizationContext::uv_edge_async->data = data;
        return V8SynchronizationContext::uv_edge_async;
    }
}

void V8SynchronizationContext::ExecuteAction(uv_edge_async_t* uv_edge_async)
{
    DBG("V8SynchronizationContext::ExecuteAction");
    // Transfer control to continueOnV8Thread method executing on V8 thread
    uv_async_send(&uv_edge_async->uv_async);
}

void close_uv_edge_async_cb(uv_handle_t* handle) {
    uv_edge_async_t* uv_edge_async = (uv_edge_async_t*)handle;
    delete uv_edge_async;
}

void V8SynchronizationContext::CancelAction(uv_edge_async_t* uv_edge_async)
{
    DBG("V8SynchronizationContext::CancelAction");
    if (uv_edge_async->singleton)
    {
        // This is a cancellation of an action registered on CLR thread.
        // Release the wait handle to allow the uv_edge_async reuse by another CLR thread.
        uv_edge_async->action = NULL;
        uv_edge_async->data = NULL;
        uv_sem_post(V8SynchronizationContext::funcWaitHandle);
    }
    else
    {
        // This is a cancellation of an action registered on V8 thread.
        // Unref the handle to stop preventing the process from exiting.
        // V8SynchronizationContext::Unref(uv_edge_async);
        uv_close((uv_handle_t*)&uv_edge_async->uv_async, close_uv_edge_async_cb);
    }
}

unsigned long V8SynchronizationContext::GetCurrentThreadId()
{
#ifdef _WIN32
    return (unsigned long)::GetCurrentThreadId();
#else
    return (unsigned long)pthread_self();
#endif
}
