#include "edge.h"

Handle<Value> v8FuncCallback(const v8::Arguments& args)
{
    DBG("v8FuncCallback");
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_edgeContext")));
    NodejsFuncInvokeContext* context = (NodejsFuncInvokeContext*)(correlator->Value());
    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
       context->Complete(exceptionV82stringCLR(args[0]), NULL);
    }
    else 
    {
        // TODO add support for exceptions during marshaling:
        MonoObject* result = ClrFunc::MarshalV8ToCLR(args[1]);
        context->Complete(NULL, result);
    }
    return scope.Close(Undefined());
}


NodejsFuncInvokeContext::NodejsFuncInvokeContext(MonoObject* _this) 
{
    DBG("NodejsFuncInvokeContext::NodejsFuncInvokeContext");

    this->_this = mono_gchandle_new(_this, FALSE); // released in Complete
}

NodejsFuncInvokeContext::~NodejsFuncInvokeContext() 
{
    mono_gchandle_free(this->_this);
}

NodejsFuncInvokeContext* NodejsFuncInvokeContext::CallFuncOnV8Thread(MonoObject* _this, NodejsFunc* nativeNodejsFunc, MonoObject* payload, MonoString** exc)
{
    DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread");

    NodejsFuncInvokeContext* result = NULL;
    *exc = NULL; 

    HandleScope scope;
    TryCatch try_catch;
    Handle<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(payload);
    if (try_catch.HasCaught()) 
    {
        *exc = mono_string_new(mono_domain_get(), "Unable to convert CLR value to V8 value.");
        return result;
    }

    result = new NodejsFuncInvokeContext(_this);

    Handle<v8::FunctionTemplate> callbackTemplate = v8::FunctionTemplate::New(v8FuncCallback);
    Handle<v8::Function> callback = callbackTemplate->GetFunction();
    callback->Set(v8::String::NewSymbol("_edgeContext"), v8::External::New((void*)result));
    Handle<v8::Value> argv[] = { jspayload, callback };
    TryCatch tryCatch;
    (*(nativeNodejsFunc->Func))->Call(v8::Context::GetCurrent()->Global(), 2, argv);
    if (tryCatch.HasCaught()) 
    {
        result->Complete(exceptionV82stringCLR(tryCatch.Exception()), NULL);
    }    

    return result;
}

void NodejsFuncInvokeContext::Complete(MonoString* exception, MonoObject* result)
{
    DBG("NodejsFuncInvokeContext::Complete");

    static MonoMethod* method;

    if (!method)
    {
        MonoClass* klass = mono_class_from_name(MonoEmbedding::GetImage(), "", "NodejsFuncInvokeContext");
        method = mono_class_get_method_from_name(klass, "Complete", 2);
    }

    void* args[] = { exception, result };
    mono_runtime_invoke(method, mono_gchandle_get_target(this->_this), args, NULL);
    delete this;
}

// vim: ts=4 sw=4 et: 
