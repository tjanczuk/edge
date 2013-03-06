#ifndef __OWIN_H
#define __OWIN_H

#include <node.h>
#include <node_buffer.h>
#include <v8.h>
#include <uv.h>
#include <vcclr.h>

#using <system.web.extensions.dll>

using namespace v8;
using namespace System::Collections::Generic;
using namespace System::Reflection;
using namespace System::Threading::Tasks;
using namespace System::Threading;
using namespace System::Web::Script::Serialization;

// Good explanation of native Buffers at 
// http://sambro.is-super-awesome.com/2011/03/03/creating-a-proper-buffer-in-a-node-c-addon/
Persistent<Function> bufferConstructor;
BOOL debugMode;

typedef struct uv_owin_async_s {
    uv_async_t uv_async;
    gcroot<System::Object^> context;
} uv_owin_async_t;

ref class OwinAppInvokeContext {
private:
    Dictionary<System::String^,System::Object^>^ netenv;
    Task^ task;
    Dictionary<System::String^,array<System::String^>^>^ responseHeaders;
    System::IO::MemoryStream^ responseBody;
    Persistent<Function>* callback;
    uv_owin_async_t* uv_owin_async;
    uv_owin_async_t* uv_owin_async_func;
    AutoResetEvent^ funcWaitHandle;

    void DisposeCallback();
    void DisposeUvOwinAsync();
    Handle<v8::Object> CreateResultObject();

public:
    OwinAppInvokeContext(Dictionary<System::String^,System::Object^>^ netenv, Handle<Function> callback);
    ~OwinAppInvokeContext();
    !OwinAppInvokeContext();

    void CompleteOnCLRThread(Task^ task);
    void CompleteOnV8Thread();
    void DisposeUvOwinAsyncFunc();
    void RecreateUvOwinAsyncFunc();
    uv_owin_async_t* WaitForUvOwinAsyncFunc();
};

ref class NodejsFunctionContext {
private:

    void DisposeFunction();

public:

    property OwinAppInvokeContext^ AppInvokeContext;
    property Persistent<Function>* Func;

    NodejsFunctionContext(OwinAppInvokeContext^ appInvokeContext, Handle<Function> function);
    ~NodejsFunctionContext();
    !NodejsFunctionContext();

    Task<System::Object^>^ FunctionWrapper(System::Object^ payload);
};

ref class NodejsFunctionInvocationContext;

typedef struct nodejsFunctionInvocationContextWrap {
    gcroot<NodejsFunctionInvocationContext^> context;
} NodejsFunctionInvocationContextWrap;

ref class NodejsFunctionInvocationContext {
private:
    NodejsFunctionContext^ functionContext;
    System::Object^ payload;
    System::Exception^ exception;
    System::Object^ result;
    NodejsFunctionInvocationContextWrap* wrap;

    void Complete();

public:

    property TaskCompletionSource<System::Object^>^ TaskCompletionSource;

    NodejsFunctionInvocationContext(
        NodejsFunctionContext^ functionContext, System::Object^ payload);
    ~NodejsFunctionInvocationContext();
    !NodejsFunctionInvocationContext();

    void CompleteWithError(System::Exception^ exception);
    void CompleteWithResult(Handle<v8::String> result);
    void CompleteEmpty();
    void CallFuncOnV8Thread();
};

ref class OwinApp {
private:
    System::Object^ instance;
    MethodInfo^ invokeMethod;
    static List<OwinApp^>^ apps;

    OwinApp();

public:
    static OwinApp();
    static Handle<Value> Initialize(const v8::Arguments& args);
    static Handle<Value> Call(const v8::Arguments& args);
};

#endif