#include "edge.h"

Handle<Value> v8FuncCallback(const v8::Arguments& args)
{
    DBG("v8FuncCallback");
    HandleScope scope;
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

    static Persistent<v8::Function> callbackFactory;
    static Persistent<v8::Function> callbackFunction;

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
        // See https://github.com/tjanczuk/edge/issues/125 for context
        
        if (callbackFactory.IsEmpty())
        {
            callbackFunction = Persistent<v8::Function>::New(
                FunctionTemplate::New(v8FuncCallback)->GetFunction());
            Handle<v8::String> code = v8::String::New(
                "(function (cb, ctx) { return function (e, d) { return cb(e, d, ctx); }; })");
            callbackFactory = Persistent<v8::Function>::New(
                Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
        }

        Handle<v8::Value> factoryArgv[] = { callbackFunction, v8::External::New((void*)ctx) };
        Handle<v8::Function> callback = Handle<v8::Function>::Cast(
            callbackFactory->Call(v8::Context::GetCurrent()->Global(), 2, factoryArgv));

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
