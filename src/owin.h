#ifndef __OWIN_H
#define __OWIN_H

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
using namespace System::CodeDom::Compiler;
using namespace Microsoft::CSharp;

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

typedef struct uv_owin_async_s {
    uv_async_t uv_async;
    gcroot<System::Object^> context;
} uv_owin_async_t;

ref class ClrFuncInvokeContext {
private:
    Task<System::Object^>^ task;
    Persistent<Function>* callback;
    uv_owin_async_t* uv_owin_async;
    uv_owin_async_t* uv_owin_async_func;
    AutoResetEvent^ funcWaitHandle;
    List<System::IntPtr>^ persistentHandles;

    void DisposeCallback();
    void DisposeUvOwinAsync();

public:

    property System::Object^ Payload;

    ClrFuncInvokeContext(Handle<Function> callback);

    void CompleteOnCLRThread(Task<System::Object^>^ task);
    void CompleteOnV8Thread();
    void DisposeUvOwinAsyncFunc();
    void RecreateUvOwinAsyncFunc();
    uv_owin_async_t* WaitForUvOwinAsyncFunc();
    void AddPersistentHandle(Persistent<Value>* handle);
    void DisposePersistentHandles();
};

ref class NodejsFunc {
public:

    property ClrFuncInvokeContext^ ClrInvokeContext;
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

ref class ClrFunc {
private:
    System::Object^ instance;
    MethodInfo^ invokeMethod;
    static List<ClrFunc^>^ apps;

    ClrFunc();

    static BOOL TryCompile(System::String^ csx, System::String^% errors, Assembly^% assembly);

public:
    static ClrFunc();
    static Handle<Value> Initialize(const v8::Arguments& args);
    static Handle<Value> Call(const v8::Arguments& args);
    static Handle<v8::Value> MarshalCLRToV8(System::Object^ netdata);
    static System::Object^ MarshalV8ToCLR(ClrFuncInvokeContext^ context, Handle<v8::Value> jsdata);    
};

ref class OwinJavaScriptConverter: JavaScriptConverter {
private:
    static cli::array<System::Type^>^ supportedTypes = {  cli::array<byte>::typeid };

public:

    OwinJavaScriptConverter();

    property List<cli::array<byte>^>^ Buffers;

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

    Handle<v8::Value> FixupBuffers(Handle<v8::Value> data);
};

#endif