#ifndef __EDGE_H
#define __EDGE_H

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include <vcclr.h>

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
extern Persistent<Function> bufferConstructor;
extern Persistent<v8::Object> json;
extern Persistent<Function> jsonParse;

Handle<v8::String> stringCLR2V8(System::String^ text);
System::String^ stringV82CLR(Handle<v8::String> text);
System::String^ exceptionV82stringCLR(Handle<v8::Value> exception);
Handle<String> exceptionCLR2stringV8(System::Exception^ exception);
Handle<Value> throwV8Exception(System::Exception^ exception);

typedef struct uv_edge_async_s {
    uv_async_t uv_async;
    gcroot<System::Action^> action;
} uv_edge_async_t;

ref class V8SynchronizationContext {
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
    static AutoResetEvent^ funcWaitHandle;

    static void Initialize();
    static uv_edge_async_t* RegisterAction(System::Action^ action);
    static void ExecuteAction(uv_edge_async_t* uv_edge_async);
    static void CancelAction(uv_edge_async_t* uv_edge_async);
    static void Unref(uv_edge_async_t* uv_edge_async);
};

ref class ClrFuncInvokeContext {
private:
    Persistent<Function>* callback;
    uv_edge_async_t* uv_edge_async;
    List<System::IntPtr>^ persistentHandles;

    void DisposeCallback();

public:

    property System::Object^ Payload;
    property Task<System::Object^>^ Task;
    property bool Sync;

    ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync);

    void CompleteOnCLRThread(System::Threading::Tasks::Task<System::Object^>^ task);
    void CompleteOnV8ThreadAsynchronous();
    Handle<v8::Value> CompleteOnV8Thread(bool completedSynchronously);
    void AddPersistentHandle(Persistent<Value>* handle);
    void DisposePersistentHandles();
};

ref class NodejsFunc {
public:

    property Persistent<Function>* Func;

    NodejsFunc(ClrFuncInvokeContext^ appInvokeContext, Handle<Function> function);

    Task<System::Object^>^ FunctionWrapper(System::Object^ payload);
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

public:
    static Handle<v8::Value> Initialize(const v8::Arguments& args);
    static Handle<v8::Function> Initialize(System::Func<System::Object^,Task<System::Object^>^>^ func);
    Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callback);
    static Handle<v8::Value> MarshalCLRToV8(System::Object^ netdata);
    static System::Object^ MarshalV8ToCLR(ClrFuncInvokeContext^ context, Handle<v8::Value> jsdata);    
};

typedef struct clrFuncWrap {
    gcroot<ClrFunc^> clrFunc;
} ClrFuncWrap;

ref class EdgeJavaScriptConverter: JavaScriptConverter {
private:
    static cli::array<System::Type^>^ supportedTypes = { 
        cli::array<byte>::typeid,
        System::Func<System::Object^,Task<System::Object^>^>::typeid
    };

public:

    EdgeJavaScriptConverter();

    property List<System::Object^>^ Objects;

    property IEnumerable<System::Type^>^ SupportedTypes {
        IEnumerable<System::Type^>^ get () override 
        {
            return supportedTypes; 
        }
    };

    System::Object^ Deserialize(
        IDictionary<System::String^, System::Object^>^ dictionary, 
        System::Type^ type, 
        JavaScriptSerializer^ serializer) override;

    IDictionary<System::String^, System::Object^>^ Serialize(
        System::Object^ obj, 
        JavaScriptSerializer^ serializer) override;

    Handle<v8::Value> FixupResult(Handle<v8::Value> data);
};

#endif
