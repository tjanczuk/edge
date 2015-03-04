#include "edge.h"

NAN_METHOD(v8FuncCallback)
{
    DBG("v8FuncCallback");
    NanScope();
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
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
    NanReturnValue(NanUndefined());
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

    static Persistent<v8::Function> callbackFactory;
    static Persistent<v8::Function> callbackFunction;

    NanScope();
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
        // See https://github.com/tjanczuk/edge/issues/125 for context
        
        if (callbackFactory.IsEmpty())
        {
            NanAssignPersistent(
                callbackFunction,
                NanNew<FunctionTemplate>(v8FuncCallback)->GetFunction());
            Handle<v8::String> code = NanNew<v8::String>(
                "(function (cb, ctx) { return function (e, d) { return cb(e, d, ctx); }; })");
            NanAssignPersistent(
                callbackFactory,
                Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
        }

        Handle<v8::Value> factoryArgv[] = { NanNew(callbackFunction), NanNew<v8::External>((void*)ctx) };
        Handle<v8::Function> callback = Handle<v8::Function>::Cast(
            NanNew(callbackFactory)->Call(NanGetCurrentContext()->Global(), 2, factoryArgv));

        Handle<v8::Value> argv[] = { jspayload, callback };
        TryCatch tryCatch;
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread calling JavaScript function");
        NanNew(*(nativeNodejsFunc->Func))->Call(NanGetCurrentContext()->Global(), 2, argv);
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread called JavaScript function");
        if (tryCatch.HasCaught()) 
        {
            DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread caught JavaScript exception");
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
