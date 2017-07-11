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

#include "../common/edge_common.h"

#include <vcclr.h>

#using <system.dll>
#using <system.web.extensions.dll>

using namespace System::Collections::Generic;
using namespace System::Reflection;
using namespace System::Threading::Tasks;
using namespace System::Threading;
using namespace System::Web::Script::Serialization;

v8::Local<v8::String> stringCLR2V8(System::String^ text);
System::String^ stringV82CLR(v8::Local<v8::String> text);
System::String^ stringV82CLR(v8::String::Utf8Value& utf8text);
System::String^ exceptionV82stringCLR(v8::Local<v8::Value> exception);

typedef struct clrActionContext {
    gcroot<System::Action^> action;
    static void ActionCallback(void* data);
} ClrActionContext;

ref class ClrFuncInvokeContext {
private:
    Nan::Callback* callback;
    uv_edge_async_t* uv_edge_async;

    void DisposeCallback();

public:

    property System::Object^ Payload;
    property Task<System::Object^>^ Task;
    property bool Sync;

    ClrFuncInvokeContext(v8::Local<v8::Value> callbackOrSync);

    void CompleteOnCLRThread(System::Threading::Tasks::Task<System::Object^>^ task);
    void CompleteOnV8ThreadAsynchronous();
    v8::Local<v8::Value> CompleteOnV8Thread();
    void InitializeAsyncOperation();
};

ref class NodejsFunc {
public:

    property Nan::Persistent<v8::Function>* Func;

    NodejsFunc(v8::Local<v8::Function> function);
    ~NodejsFunc();
    !NodejsFunc();

    Task<System::Object^>^ FunctionWrapper(System::Object^ payload);
};

ref class PersistentDisposeContext {
private:
    System::IntPtr ptr;
public:
    PersistentDisposeContext(Nan::Persistent<v8::Value>* handle);
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
    void CompleteWithResult(v8::Local<v8::Value> result);
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

    static v8::Local<v8::Object> MarshalCLRObjectToV8(System::Object^ netdata);

public:
    static NAN_METHOD(Initialize);
    static v8::Local<v8::Function> Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func);
    v8::Local<v8::Value> Call(v8::Local<v8::Value> payload, v8::Local<v8::Value> callback);
    static v8::Local<v8::Value> MarshalCLRToV8(System::Object^ netdata);
    static v8::Local<v8::Value> MarshalCLRExceptionToV8(System::Exception^ exception);
    static System::Object^ MarshalV8ToCLR(v8::Local<v8::Value> jsdata);
};

typedef struct clrFuncWrap {
    gcroot<ClrFunc^> clrFunc;
} ClrFuncWrap;

#endif
