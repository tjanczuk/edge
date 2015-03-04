#include "edge.h"

static MonoClass* GetClrFuncInvokeContextClass()
{
    static MonoClass* klass;

    if (!klass)
        klass = mono_class_from_name(MonoEmbedding::GetImage(), "", "ClrFuncInvokeContext");

    return klass;
}

ClrFuncInvokeContext::ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync) : _this(0), callback(0), uv_edge_async(0)
{
    static MonoClassField* field;
    static MonoClassField* syncField;

    if (!field)
        field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), "native");
    if (!syncField)
        syncField = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), "Sync");

    MonoObject* obj = mono_object_new(mono_domain_get(), GetClrFuncInvokeContextClass());

    ClrFuncInvokeContext* thisPointer = this;
    mono_field_set_value(obj, field, &thisPointer);
    this->_this = mono_gchandle_new(obj, FALSE); // released in destructor

    DBG("ClrFuncInvokeContext::ClrFuncInvokeContext");
    if (callbackOrSync->IsFunction())
    {
        this->callback = new Persistent<Function>(); // released in destructor
        NanAssignPersistent(
            *(this->callback),
            Handle<Function>::Cast(callbackOrSync));
        this->Sync(FALSE);
    }
    else 
    {
        this->Sync(callbackOrSync->BooleanValue());
    }
}

void ClrFuncInvokeContext::InitializeAsyncOperation()
{
    // Create a uv_edge_async instance representing V8 async operation that will complete 
    // when the CLR function completes. The ClrActionContext is used to ensure the ClrFuncInvokeContext
    // remains GC-rooted while the CLR function executes.

    static MonoMethod* getAction = NULL;
    if (!getAction)
        getAction = mono_class_get_method_from_name(
            GetClrFuncInvokeContextClass(), "GetCompleteOnV8ThreadAsynchronousAction", -1);
    
    ClrActionContext* data = new ClrActionContext;
    data->action = mono_gchandle_new( // released in ClrActionContext::ActionCallback
        mono_runtime_invoke(getAction, mono_gchandle_get_target(this->_this), NULL, NULL), FALSE);
    this->uv_edge_async = V8SynchronizationContext::RegisterAction(ClrActionContext::ActionCallback, data);
}

ClrFuncInvokeContext::~ClrFuncInvokeContext()
{
    if (this->callback)
    {
        DBG("ClrFuncInvokeContext::DisposeCallback");
        NanDisposePersistent(*(this->callback));
        delete this->callback;
        this->callback = NULL;        
    }
    mono_gchandle_free(this->_this);
}

void ClrFuncInvokeContext::CompleteOnCLRThread(ClrFuncInvokeContext *_this, MonoObject* task)
{
    DBG("ClrFuncInvokeContext::CompleteOnCLRThread");
    _this->Task(task);
    V8SynchronizationContext::ExecuteAction(_this->uv_edge_async);
}

void ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous(ClrFuncInvokeContext *_this)
{
    NanScope();
    _this->CompleteOnV8Thread(false);
}

Handle<v8::Value> ClrFuncInvokeContext::CompleteOnV8Thread(bool completedSynchronously)
{
    DBG("ClrFuncInvokeContext::CompleteOnV8Thread");

    NanEscapableScope();

    // The uv_edge_async was already cleaned up in V8SynchronizationContext::ExecuteAction
    this->uv_edge_async = NULL;

    if (!this->Sync() && !this->callback)
    {
        // this was an async call without callback specified
        delete this;
        return NanEscapeScope(NanUndefined());
    }

    Handle<Value> argv[] = { NanUndefined(), NanUndefined() };
    int argc = 1;

    switch (Task::Status(this->Task())) {
    default:
        argv[0] = NanNew<v8::String>("The operation reported completion in an unexpected state.");
        break;
    case Task::Faulted:
        if (Task::Exception(this->Task()) != NULL) {
            argv[0] = ClrFunc::MarshalCLRExceptionToV8(Task::Exception(this->Task()));
        }
        else {
            argv[0] = NanNew<v8::String>("The operation has failed with an undetermined error.");
        }
        break;
    case Task::Canceled:
        argv[0] = NanNew<v8::String>("The operation was cancelled.");
        break;
    case Task::RanToCompletion:
        argc = 2;
        MonoException* exc = NULL;
        argv[1] = ClrFunc::MarshalCLRToV8(Task::Result(this->Task()), &exc);
        if (exc) 
        {
            argc = 1;
            argv[0] = ClrFunc::MarshalCLRExceptionToV8(exc);
        }
        break;
    };

    if (!this->Sync())
    {
        // complete the asynchronous call to C# by invoking a callback in JavaScript
        TryCatch try_catch;
        NanNew<v8::Function>(*(this->callback))->Call(NanGetCurrentContext()->Global(), argc, argv);
        delete this;
        if (try_catch.HasCaught()) 
        {
            node::FatalException(try_catch);
        }        

        return NanEscapeScope(NanUndefined());
    }
    else {
        delete this;
        if (1 == argc) 
        {
            // complete the synchronous call to C# by re-throwing the resulting exception
            NanThrowError(argv[0]);
            return NanEscapeScope(argv[0]);
        }
        else
        {
            // complete the synchronous call to C# by returning the result
            return NanEscapeScope(argv[1]);
        }
    }
}

MonoObject* ClrFuncInvokeContext::GetMonoObject()
{
    return mono_gchandle_get_target(_this);
}

#define IMPLEMENT_REF_FIELD(T, Name)\
    static MonoClassField* Name ## Field;\
    T ClrFuncInvokeContext::Name()\
{\
    if (!Name ## Field)\
    Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
    T value;\
    mono_field_get_value(mono_gchandle_get_target(_this), Name ## Field, &value);\
    return value;\
}\
    void ClrFuncInvokeContext::Name(T value)\
{\
    if (!Name ## Field)\
    Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
    mono_field_set_value(mono_gchandle_get_target(_this), Name ## Field, value);\
}\

#define IMPLEMENT_FIELD(T, Name)\
    static MonoClassField* Name ## Field;\
    T ClrFuncInvokeContext::Name()\
{\
    if (!Name ## Field)\
    Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
    T value;\
    mono_field_get_value(mono_gchandle_get_target(_this), Name ## Field, &value);\
    return value;\
}\
    void ClrFuncInvokeContext::Name(T value)\
{\
    if (!Name ## Field)\
    Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
    mono_field_set_value(mono_gchandle_get_target(_this), Name ## Field, &value);\
}\

IMPLEMENT_REF_FIELD(MonoObject*, Payload)
    IMPLEMENT_REF_FIELD(MonoObject*, Task)
    IMPLEMENT_FIELD(bool, Sync)

    // vim: ts=4 sw=4 et: 
