#include "edge.h"

NAN_METHOD(v8FuncCallback)
{
    DBG("v8FuncCallback");
    Nan::HandleScope scope;
    v8::Local<v8::External> correlator = v8::Local<v8::External>::Cast(info[2]);
    NodejsFuncInvokeContext* context = (NodejsFuncInvokeContext*)(correlator->Value());
    if (!info[0]->IsUndefined() && !info[0]->IsNull())
    {
       context->Complete((MonoObject*)exceptionV82stringCLR(info[0]), NULL);
    }
    else 
    {
        // TODO add support for exceptions during marshaling:
        MonoObject* result = ClrFunc::MarshalV8ToCLR(info[1]);
        context->Complete(NULL, result);
    }
    info.GetReturnValue().SetUndefined();
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

    static Nan::Persistent<v8::Function> callbackFactory;
    static Nan::Persistent<v8::Function> callbackFunction;

    Nan::HandleScope scope;
    NodejsFuncInvokeContext* ctx = new NodejsFuncInvokeContext(_this);
    
    MonoException* exc = NULL;
    v8::Local<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(payload, &exc);
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
            v8::Local<v8::Function> v8FuncCallbackFunction = Nan::New<v8::FunctionTemplate>(v8FuncCallback)->GetFunction();
            callbackFunction.Reset(v8FuncCallbackFunction);
            v8::Local<v8::String> code = Nan::New<v8::String>(
                "(function (cb, ctx) { return function (e, d) { return cb(e, d, ctx); }; })").ToLocalChecked();
            v8::Local<v8::Function> callbackFactoryFunction = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
            callbackFactory.Reset(callbackFactoryFunction);
        }

        v8::Local<v8::Value> factoryArgv[] = { Nan::New(callbackFunction), Nan::New<v8::External>((void*)ctx) };
        v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(
            Nan::New(callbackFactory)->Call(Nan::GetCurrentContext()->Global(), 2, factoryArgv));

        v8::Local<v8::Value> argv[] = { jspayload, callback };
        Nan::TryCatch tryCatch;
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread calling JavaScript function");
        Nan::Call(Nan::New(*(nativeNodejsFunc->Func)), Nan::GetCurrentContext()->Global(), 2, argv);
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread called JavaScript function");
        if (tryCatch.HasCaught()) 
        {
            DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread caught JavaScript exception");
            ctx->Complete((MonoObject*)exceptionV82stringCLR(tryCatch.Exception()), NULL);
            // ctx deleted in Complete
        }
        else
        {
            // Kick the next tick
            CallbackHelper::KickNextTick();
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
