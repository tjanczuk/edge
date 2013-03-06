#include "owin.h"

Handle<Value> v8FuncCallback(const v8::Arguments& args)
{
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_owinContext")));
    NodejsFuncInvokeContextWrap* wrap = (NodejsFuncInvokeContextWrap*)(correlator->Value());
    NodejsFuncInvokeContext^ context = wrap->context;
    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
        context->CompleteWithError(gcnew System::Exception(exceptionV82stringCLR(args[0])));
    }
    else 
    {
        context->CompleteWithResult(args[1]);
    }

    return scope.Close(Undefined());
}

NodejsFuncInvokeContext::NodejsFuncInvokeContext(
        NodejsFunc^ functionContext, System::Object^ payload)
{
    this->functionContext = functionContext;
    this->payload = payload;
    this->TaskCompletionSource = gcnew System::Threading::Tasks::TaskCompletionSource<System::Object^>();
    this->wrap = new NodejsFuncInvokeContextWrap;
    this->wrap->context = this;
}

NodejsFuncInvokeContext::~NodejsFuncInvokeContext()
{
    this->!NodejsFuncInvokeContext();
}

NodejsFuncInvokeContext::!NodejsFuncInvokeContext()
{
    if (this->wrap)
    {
        delete this->wrap;
        this->wrap = NULL;
    }
}

void NodejsFuncInvokeContext::CallFuncOnV8Thread()
{
    HandleScope scope;
    this->functionContext->ClrInvokeContext->RecreateUvOwinAsyncFunc();
    try 
    {
        TryCatch try_catch;
        Handle<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(this->payload);
        if (try_catch.HasCaught()) 
        {
            throw gcnew System::Exception("Unable to convert CLR value to V8 value.");
        }

        Handle<v8::FunctionTemplate> callbackTemplate = v8::FunctionTemplate::New(v8FuncCallback);
        Handle<v8::Function> callback = callbackTemplate->GetFunction();
        callback->Set(v8::String::NewSymbol("_owinContext"), v8::External::New((void*)this->wrap));
        Handle<v8::Value> argv[] = { jspayload, callback };
        TryCatch tryCatch;
        (*(this->functionContext->Func))->Call(v8::Context::GetCurrent()->Global(), 2, argv);
        if (tryCatch.HasCaught()) 
        {
            this->CompleteWithError(gcnew System::Exception(exceptionV82stringCLR(tryCatch.Exception())));
        }
    }
    catch (System::Exception^ ex)
    {
        this->CompleteWithError(ex);
    }
}

void NodejsFuncInvokeContext::Complete()
{
    if (this->exception != nullptr)
    {
        this->TaskCompletionSource->SetException(this->exception);
    }
    else 
    {
        this->TaskCompletionSource->SetResult(this->result);
    }
}

void NodejsFuncInvokeContext::CompleteWithError(System::Exception^ exception)
{
    this->exception = exception;
    Task::Run(gcnew System::Action(this, &NodejsFuncInvokeContext::Complete));
}

void NodejsFuncInvokeContext::CompleteWithResult(Handle<v8::Value> result)
{
    try 
    {
        this->result = ClrFunc::MarshalV8ToCLR(nullptr, result);
        Task::Run(gcnew System::Action(this, &NodejsFuncInvokeContext::Complete));
    }
    catch (System::Exception^ e)
    {
        this->CompleteWithError(e);
    }
}
