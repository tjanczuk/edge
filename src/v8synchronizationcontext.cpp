#include "edge.h"

void continueOnV8Thread(uv_async_t* handle, int status)
{
	// This executes on V8 thread

    DBG("continueOnV8Thread");
    HandleScope handleScope;
    uv_edge_async_t* uv_edge_async = CONTAINING_RECORD(handle, uv_edge_async_t, uv_async);
    System::Action^ action = uv_edge_async->action;
    V8SynchronizationContext::CancelAction(uv_edge_async);

    action();
}

void V8SynchronizationContext::Initialize() 
{
	// This executes on V8 thread

    V8SynchronizationContext::uv_edge_async = new uv_edge_async_t;
    uv_async_init(uv_default_loop(), &V8SynchronizationContext::uv_edge_async->uv_async, continueOnV8Thread);
    V8SynchronizationContext::Unref(V8SynchronizationContext::uv_edge_async);
    V8SynchronizationContext::funcWaitHandle = gcnew AutoResetEvent(true);
    V8SynchronizationContext::v8ThreadId = GetCurrentThreadId();
}

void V8SynchronizationContext::Unref(uv_edge_async_t* uv_edge_async)
{
#if UV_VERSION_MAJOR==0 && UV_VERSION_MINOR<8
    uv_unref(uv_default_loop());
#else
    uv_unref((uv_handle_t*)&uv_edge_async->uv_async);
#endif  
}

uv_edge_async_t* V8SynchronizationContext::RegisterAction(System::Action^ action)
{
    if (GetCurrentThreadId() == V8SynchronizationContext::v8ThreadId)
    {
        // This executes on V8 thread.
        // Allocate new uv_edge_async.

        uv_edge_async_t* uv_edge_async = new uv_edge_async_t;
        uv_edge_async->action = action;
        uv_async_init(uv_default_loop(), &uv_edge_async->uv_async, continueOnV8Thread);
        return uv_edge_async;
    }
    else 
    {
    	// This executes on CLR thread. 
    	// Acquire exlusive access to uv_edge_async previously initialized on V8 thread.

    	V8SynchronizationContext::funcWaitHandle->WaitOne();
    	V8SynchronizationContext::uv_edge_async->action = action;
    	return V8SynchronizationContext::uv_edge_async;
    }
}

void V8SynchronizationContext::ExecuteAction(uv_edge_async_t* uv_edge_async)
{
	// Transfer control to completeOnV8hread method executing on V8 thread

    BOOL ret = PostQueuedCompletionStatus(
        uv_default_loop()->iocp, 
        0, 
        (ULONG_PTR)NULL, 
        &uv_edge_async->uv_async.async_req.overlapped);	
}

void V8SynchronizationContext::CancelAction(uv_edge_async_t* uv_edge_async)
{
    if (uv_edge_async == V8SynchronizationContext::uv_edge_async)
    {
    	// This is a cancellation of an action registered in RegisterActionFromCLRThread.
    	// Release the wait handle to allow the uv_edge_async reuse by another CLR thread.
    	uv_edge_async->action = nullptr;
    	V8SynchronizationContext::funcWaitHandle->Set();
    }
    else
    {
    	// This is a cancellation of an action registered in RegisterActionFromV8Thread.
    	// Unref the handle to stop preventing the process from exiting.
    	V8SynchronizationContext::Unref(uv_edge_async);
    	delete uv_edge_async;
    }
}
