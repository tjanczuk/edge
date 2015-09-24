#include "edge.h"

static MonoClass* GetNodejsFuncClass()
{
    static MonoClass* klass;

    if (!klass)
        klass = mono_class_from_name(MonoEmbedding::GetImage(), "", "NodejsFunc");

    return klass;
}

NodejsFunc::NodejsFunc(v8::Local<v8::Function> function)
{
    DBG("NodejsFunc::NodejsFunc");

    static MonoMethod* ctor;
    if (!ctor)
        ctor = mono_class_get_method_from_name(GetNodejsFuncClass(), ".ctor", -1);

    this->Func = new Nan::Persistent<v8::Function>;
    (this->Func)->Reset(function);

    MonoObject* thisObj = mono_object_new(mono_domain_get(), GetNodejsFuncClass());
    MonoException* exc = NULL;
    void *thisPtr = this;
    void *args[] = { &thisPtr };
    mono_runtime_invoke(ctor, thisObj, args, (MonoObject**)&exc);
    _this = mono_gchandle_new_weakref(thisObj, FALSE);
}

NodejsFunc::~NodejsFunc() 
{
    DBG("NodejsFunc::~NodejsFunc");
    this->Func->Reset();
    delete this->Func;
    this->Func = NULL;
}

void NodejsFunc::Release(NodejsFunc* _this)
{
    delete _this;
}

MonoObject* NodejsFunc::GetFunc()
{
    static MonoMethod* method;

    if (!method)
        method = mono_class_get_method_from_name(GetNodejsFuncClass(), "GetFunc", -1);

    MonoException* exc = NULL;
    MonoObject* func = mono_runtime_invoke(method, mono_gchandle_get_target(_this), NULL, (MonoObject**)&exc);

    return func;
}

void  NodejsFunc::ExecuteActionOnV8Thread(MonoObject* action)
{
    ClrActionContext* data = new ClrActionContext;
    data->action = mono_gchandle_new(action, FALSE); // released in ClrActionContext::ActionCallback
    uv_edge_async_t* uv_edge_async = V8SynchronizationContext::RegisterAction(ClrActionContext::ActionCallback, data);
    V8SynchronizationContext::ExecuteAction(uv_edge_async);    
}
