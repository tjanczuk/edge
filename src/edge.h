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
#ifndef __EDGE_H
#define __EDGE_H

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include <vcclr.h>
#ifndef _WIN32
#include <pthread.h>
#endif

#using <system.dll>
#using <system.web.extensions.dll>

using namespace v8;
using namespace System::Collections::Generic;
using namespace System::Reflection;
using namespace System::Threading::Tasks;
using namespace System::Threading;
using namespace System::Web::Script::Serialization;

#define DBG(msg) if (debugMode) System::Console::WriteLine(msg);

// Good explanation of native Buffers at 
// http://sambro.is-super-awesome.com/2011/03/03/creating-a-proper-buffer-in-a-node-c-addon/
extern BOOL debugMode;
extern BOOL enableScriptIgnoreAttribute;
extern Persistent<Function> bufferConstructor;

Handle<v8::String> stringCLR2V8(System::String^ text);
System::String^ stringV82CLR(Handle<v8::String> text);
System::String^ exceptionV82stringCLR(Handle<v8::Value> exception);
Handle<String> exceptionCLR2stringV8(System::Exception^ exception);
Handle<Value> throwV8Exception(System::Exception^ exception);

typedef void (*uv_async_edge_cb)(void* data);

typedef struct uv_edge_async_s {
    uv_async_t uv_async;
    uv_async_edge_cb action;
    void* data;
    bool singleton;
} uv_edge_async_t;

typedef struct clrActionContext {
    gcroot<System::Action^> action;
    static void ActionCallback(void* data);
} ClrActionContext;

#if defined(_WIN32) && (UV_VERSION_MAJOR == 0) && (UV_VERSION_MINOR < 8)
#    define USE_WIN32_SYNCHRONIZATION
#endif

class V8SynchronizationContext {
private:

    static unsigned long v8ThreadId;
    static unsigned long GetCurrentThreadId();

public:

    // The node process will not exit until ExecuteAction or CancelAction had been called for all actions 
    // registered by calling RegisterAction on V8 thread. Actions registered by calling RegisterAction 
    // on CLR thread do not prevent the process from exiting.
    // Calls from JavaScript to .NET always call RegisterAction on V8 thread before invoking .NET code.
    // Calls from .NET to JavaScript call RegisterAction either on CLR or V8 thread, depending on
    // whether .NET code executes synchronously on V8 thread it strarted running on.
    // This means that if any call of a .NET function from JavaScript is in progress, the process won't exit.
    // It also means that existence of .NET proxies to JavaScript functions in the CLR does not prevent the 
    // process from exiting.
    // In this model, JavaScript owns the lifetime of the process.

    static uv_edge_async_t* uv_edge_async;
#ifdef USE_WIN32_SYNCHRONIZATION
    static HANDLE funcWaitHandle;
#else
    static uv_sem_t* funcWaitHandle;
#endif

    static void Initialize();
    static uv_edge_async_t* RegisterAction(uv_async_edge_cb action, void* data);
    static void ExecuteAction(uv_edge_async_t* uv_edge_async);
    static void CancelAction(uv_edge_async_t* uv_edge_async);
    static void Unref(uv_edge_async_t* uv_edge_async);
};

ref class ClrFuncInvokeContext {
private:
    Persistent<Function>* callback;
    uv_edge_async_t* uv_edge_async;

    void DisposeCallback();

public:

    property System::Object^ Payload;
    property Task<System::Object^>^ Task;
    property bool Sync;

    ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync);

    void CompleteOnCLRThread(System::Threading::Tasks::Task<System::Object^>^ task);
    void CompleteOnV8ThreadAsynchronous();
    Handle<v8::Value> CompleteOnV8Thread(bool completedSynchronously);
};

ref class NodejsFunc {
public:

    property Persistent<Function>* Func;

    NodejsFunc(Handle<Function> function);
    ~NodejsFunc();
    !NodejsFunc();

    Task<System::Object^>^ FunctionWrapper(System::Object^ payload);
};

ref class PersistentDisposeContext {
private:
    System::IntPtr ptr;
public:
    PersistentDisposeContext(Persistent<Value>* handle);
    void CallDisposeOnV8Thread();
};

ref class NodejsFuncInvokeContext;

typedef struct nodejsFuncInvokeContextWrap {
    gcroot<NodejsFuncInvokeContext^> context;
} NodejsFuncInvokeContextWrap;

ref class NodejsFuncInvokeContext {
private:
    NodejsFunc^ functionContext;
    System::Object^ payload;
    System::Exception^ exception;
    System::Object^ result;
    NodejsFuncInvokeContextWrap* wrap;

    void Complete();

public:

    property TaskCompletionSource<System::Object^>^ TaskCompletionSource;

    NodejsFuncInvokeContext(
        NodejsFunc^ functionContext, System::Object^ payload);
    ~NodejsFuncInvokeContext();
    !NodejsFuncInvokeContext();

    void CompleteWithError(System::Exception^ exception);
    void CompleteWithResult(Handle<v8::Value> result);
    void CallFuncOnV8Thread();
};

ref class ClrFuncReflectionWrap {
private:
    System::Object^ instance;
    MethodInfo^ invokeMethod;    

    ClrFuncReflectionWrap();

public:

    static ClrFuncReflectionWrap^ Create(Assembly^ assembly, System::String^ typeName, System::String^ methodName);
    Task<System::Object^>^ Call(System::Object^ payload);
};

ref class ClrFunc {
private:
    System::Func<System::Object^,Task<System::Object^>^>^ func;

    ClrFunc();

    static Handle<v8::Value> MarshalCLRObjectToV8(System::Object^ netdata);

public:
    static Handle<v8::Value> Initialize(const v8::Arguments& args);
    static Handle<v8::Function> Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func);
    Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callback);
    static Handle<v8::Value> MarshalCLRToV8(System::Object^ netdata);
    static System::Object^ MarshalV8ToCLR(Handle<v8::Value> jsdata);    
};

typedef struct clrFuncWrap {
    gcroot<ClrFunc^> clrFunc;
} ClrFuncWrap;

#endif
