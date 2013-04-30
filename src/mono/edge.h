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

#include "mono/metadata/object.h"
#include "mono/metadata/appdomain.h"
typedef int GCHandle;

using namespace v8;

#define DBG(msg) if (debugMode) printf(msg "\n");
#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x) 
#define ABORT_TODO() do { printf(__FILE__ "(" TOSTRING(__LINE__) "): " __FUNCTION__ "\n"); abort(); } while (0)

// Good explanation of native Buffers at 
// http://sambro.is-super-awesome.com/2011/03/03/creating-a-proper-buffer-in-a-node-c-addon/
extern bool debugMode;
extern bool enableScriptIgnoreAttribute;
extern Persistent<Function> bufferConstructor;

Handle<v8::String> stringCLR2V8(MonoString* text);
MonoString* stringV82CLR(Handle<v8::String> text);
MonoString* exceptionV82stringCLR(Handle<v8::Value> exception);
Handle<String> exceptionCLR2stringV8(MonoException* exception);
Handle<Value> throwV8Exception(MonoException* exception);

class MonoEmbedding
{
    static MonoAssembly* assembly;
public:
    static void Initialize();
    static MonoAssembly* GetAssembly();
    static MonoImage* GetImage();
    static MonoClass* GetClass();
    static MonoObject* GetClrFuncReflectionWrapFunc(const char* assembly, const char* typeName, const char* methodName, MonoException ** exc);
    static MonoObject* CreateDictionary();
};

typedef struct uv_edge_async_s {
    uv_async_t uv_async;
    GCHandle action;
} uv_edge_async_t;

class V8SynchronizationContext {
private:

    static DWORD v8ThreadId;

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
    static HANDLE funcWaitHandle;

    static void Initialize();
    static uv_edge_async_t* RegisterAction(MonoObject* action);
    static void ExecuteAction(uv_edge_async_t* uv_edge_async);
    static void CancelAction(uv_edge_async_t* uv_edge_async);
    static void Unref(uv_edge_async_t* uv_edge_async);
};

// wrapper for System.Threading.Task

class Task
{
public: 
    enum TaskStatus
    {
        Created = 0,
        WaitingForActivation = 1,
        WaitingToRun = 2,
        Running = 3,
        WaitingForChildrenToComplete = 4,
        RanToCompletion = 5,
        Canceled = 6,
        Faulted = 7
    };
    static TaskStatus Status(MonoObject* _this);
    static MonoException* Exception(MonoObject* _this);
    static MonoObject* Result(MonoObject* _this);
};

// wrapper for System.Collections.Generic.Dictionary

class Dictionary
{
public: 
    static void Add(MonoObject* _this, const char* name, MonoObject* value);
};

class ClrFuncInvokeContext {
private:
    GCHandle _this;
    Persistent<Function>* callback;
    uv_edge_async_t* uv_edge_async;

    void DisposeCallback();

public:
    MonoObject* Payload();
    void Payload(MonoObject* value);
    MonoObject* Task();
    void Task(MonoObject* value);
    bool Sync();
    void Sync(bool value);

    ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync);

    void CompleteOnCLRThread(MonoObject* task);
    static void __cdecl CompleteOnV8ThreadAsynchronous(ClrFuncInvokeContext *_this);
    Handle<v8::Value> CompleteOnV8Thread(bool completedSynchronously);
};

//ref class NodejsFunc {
//public:
//
//    property Persistent<Function>* Func;
//
//    NodejsFunc(ClrFuncInvokeContext^ appInvokeContext, Handle<Function> function);
//    ~NodejsFunc();
//    !NodejsFunc();
//
//    Task<System::Object^>^ FunctionWrapper(System::Object^ payload);
//};
//
//ref class PersistentDisposeContext {
//private:
//    System::IntPtr ptr;
//public:
//    PersistentDisposeContext(Persistent<Value>* handle);
//    void CallDisposeOnV8Thread();
//};
//
//ref class NodejsFuncInvokeContext;
//
//typedef struct nodejsFuncInvokeContextWrap {
//    gcroot<NodejsFuncInvokeContext^> context;
//} NodejsFuncInvokeContextWrap;
//
//ref class NodejsFuncInvokeContext {
//private:
//    NodejsFunc^ functionContext;
//    System::Object^ payload;
//    System::Exception^ exception;
//    System::Object^ result;
//    NodejsFuncInvokeContextWrap* wrap;
//
//    void Complete();
//
//public:
//
//    property TaskCompletionSource<System::Object^>^ TaskCompletionSource;
//
//    NodejsFuncInvokeContext(
//        NodejsFunc^ functionContext, System::Object^ payload);
//    ~NodejsFuncInvokeContext();
//    !NodejsFuncInvokeContext();
//
//    void CompleteWithError(System::Exception^ exception);
//    void CompleteWithResult(Handle<v8::Value> result);
//    void CallFuncOnV8Thread();
//};

class ClrFunc {
private:
    //System::Func<System::Object^,Task<System::Object^>^>^ func;
    GCHandle func;

    ClrFunc();

    static Handle<v8::Value> MarshalCLRObjectToV8(MonoObject* netdata);

public:
    static Handle<v8::Value> Initialize(const v8::Arguments& args);
    static Handle<v8::Function> Initialize(/*System::Func<System::Object^,Task<System::Object^>^>^*/ MonoObject* func);
    Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callback);
    static Handle<v8::Value> MarshalCLRToV8(MonoObject* netdata);
    static MonoObject* MarshalV8ToCLR(ClrFuncInvokeContext* context, Handle<v8::Value> jsdata);    
};

typedef struct clrFuncWrap {
    ClrFunc* clrFunc;
} ClrFuncWrap;

#endif

// vim: ts=4 sw=4 et: 
