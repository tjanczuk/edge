#include "edge.h"

Handle<Value> v8FuncCallback(const v8::Arguments& args)
{
    DBG("v8FuncCallback");
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_edgeContext")));
    NodejsFuncInvokeContext* context = (NodejsFuncInvokeContext*)(correlator->Value());
    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
       context->Complete((MonoObject*)exceptionV82stringCLR(args[0]), NULL);
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

void NodejsFuncInvokeContext::CallFuncOnV8Thread(MonoObject* _this, NodejsFunc* nativeNodejsFunc, MonoObject* payload)
{
    DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread");

    HandleScope scope;
    NodejsFuncInvokeContext* ctx = new NodejsFuncInvokeContext(_this);
    
    MonoException* exc = NULL;
    Handle<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(payload, &exc);
    if (exc) 
    {
        ctx->Complete((MonoObject*)exc, NULL);
        // ctx deleted in Complete
    }
    else 
    {
        Handle<v8::FunctionTemplate> callbackTemplate = v8::FunctionTemplate::New(v8FuncCallback);
        Handle<v8::Function> callback = callbackTemplate->GetFunction();
        callback->Set(v8::String::NewSymbol("_edgeContext"), v8::External::New((void*)ctx));
        Handle<v8::Value> argv[] = { jspayload, callback };
        TryCatch tryCatch;
        (*(nativeNodejsFunc->Func))->Call(v8::Context::GetCurrent()->Global(), 2, argv);
        if (tryCatch.HasCaught()) 
        {
            ctx->Complete((MonoObject*)exceptionV82stringCLR(tryCatch.Exception()), NULL);
            // ctx deleted in Complete
        }

        // In the absence of exception, processing resumes in v8FuncCallback
    }
}

void NodejsFuncInvokeContext::Complete(MonoObject* exception, MonoObject* result)
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
