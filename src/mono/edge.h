#ifndef __EDGE_H
#define __EDGE_H

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <uv.h>

#ifdef _MSC_VER
#define EDGE_PLATFORM_WINDOWS 1
#endif

#include "mono/metadata/object.h"
#include "mono/metadata/appdomain.h"
typedef int GCHandle;

using namespace v8;

#define DBG(msg) if (debugMode) printf(msg "\n");
#ifdef EDGE_PLATFORM_WINDOWS
#define ABORT_TODO() do { printf("%s (%d): %s\n", __FILE__, __LINE__, __FUNCTION__); abort(); } while (0)
#else
#define ABORT_TODO() do { printf("%s (%d): %s\n", __FILE__, __LINE__, __func__); abort(); } while (0)
#endif

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
    static MonoObject* CreateDateTime(double ticks);
    static MonoClass* GetIDictionaryStringObjectClass(MonoException** exc);
    static MonoClass* GetUriClass(MonoException** exc);
    static MonoObject* CreateExpandoObject();
    static MonoClass* GetFuncClass();
    static MonoArray* IEnumerableToArray(MonoObject* ienumerable, MonoException** exc);
    static MonoArray* IDictionaryToFlatArray(MonoObject* dictionary, MonoException** exc);
    static void ContinueTask(MonoObject* task, MonoObject* state, MonoException** exc);
    static double GetDateValue(MonoObject* dt, MonoException** exc);
    static MonoString* ToString(MonoObject* o, MonoException** exc);
    static double Int64ToDouble(MonoObject* i64, MonoException** exc);
    static MonoString* TryConvertPrimitiveOrDecimal(MonoObject* obj, MonoException** exc);
};

typedef void (*uv_async_edge_cb)(void* data);

typedef struct uv_edge_async_s {
    uv_async_t uv_async;
    uv_async_edge_cb action;
    void* data;
    bool singleton;
} uv_edge_async_t;

typedef struct clrActionContext {
    GCHandle action;
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

public:
    MonoObject* Payload();
    void Payload(MonoObject* value);
    MonoObject* Task();
    void Task(MonoObject* value);
    bool Sync();
    void Sync(bool value);

    MonoObject* GetMonoObject();

    ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync);
    ~ClrFuncInvokeContext();

    void InitializeAsyncOperation();

    static void __cdecl CompleteOnCLRThread(ClrFuncInvokeContext *_this, MonoObject* task);
    static void __cdecl CompleteOnV8ThreadAsynchronous(ClrFuncInvokeContext *_this);
    Handle<v8::Value> CompleteOnV8Thread(bool completedSynchronously);
};

class NodejsFunc {
    GCHandle _this;
public:
    Persistent<Function>* Func;

    NodejsFunc(Handle<Function> function);
    ~NodejsFunc();

    MonoObject* GetFunc(); // returns Func<object,Task<object>>

    static void __cdecl ExecuteActionOnV8Thread(MonoObject* action);
    static void __cdecl Release(NodejsFunc* _this);
};

class NodejsFuncInvokeContext {
    GCHandle _this;
public:
    NodejsFuncInvokeContext(MonoObject* _this);
    ~NodejsFuncInvokeContext();

    static void __cdecl CallFuncOnV8Thread(MonoObject* _this, NodejsFunc* nativeNodejsFunc, MonoObject* payload);
    void Complete(MonoObject* exception, MonoObject* result);
};

class ClrFunc {
private:
    //System::Func<System::Object^,Task<System::Object^>^>^ func;
    GCHandle func;

    ClrFunc();

    static Handle<v8::Value> MarshalCLRObjectToV8(MonoObject* netdata, MonoException** exc);

public:
    static Handle<v8::Value> Initialize(const v8::Arguments& args);
    static Handle<v8::Function> Initialize(/*System::Func<System::Object^,Task<System::Object^>^>^*/ MonoObject* func);
    Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callback);
    static Handle<v8::Value> MarshalCLRToV8(MonoObject* netdata, MonoException** exc);
    static MonoObject* MarshalV8ToCLR(Handle<v8::Value> jsdata);    
};

typedef struct clrFuncWrap {
    ClrFunc* clrFunc;
} ClrFuncWrap;

#endif

// vim: ts=4 sw=4 et: 
