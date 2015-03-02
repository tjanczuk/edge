#ifndef __EDGE_H
#define __EDGE_H

#include "../common/edge_common.h"

#include <pthread.h>
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"
#include "mono/metadata/appdomain.h"

typedef int GCHandle;

using namespace v8;

Handle<v8::String> stringCLR2V8(MonoString* text);
MonoString* stringV82CLR(Handle<v8::String> text);
MonoString* exceptionV82stringCLR(Handle<v8::Value> exception);
Handle<Value> throwV8Exception(Handle<Value> exception);

class MonoEmbedding
{
    static MonoAssembly* assembly;
public:
    static void Initialize();
    static void NormalizeException(MonoException** e);
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

typedef struct clrActionContext {
    GCHandle action;
    static void ActionCallback(void* data);
} ClrActionContext;

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

    static Handle<v8::Object> MarshalCLRObjectToV8(MonoObject* netdata, MonoException** exc);

public:
    static NAN_METHOD(Initialize);
    static Handle<v8::Function> Initialize(MonoObject* func);
    Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callback);
    static Handle<v8::Value> MarshalCLRToV8(MonoObject* netdata, MonoException** exc);
    static Handle<v8::Object> MarshalCLRExceptionToV8(MonoException* exception);
    static MonoObject* MarshalV8ToCLR(Handle<v8::Value> jsdata);    
};

typedef struct clrFuncWrap {
    ClrFunc* clrFunc;
} ClrFuncWrap;

#endif
