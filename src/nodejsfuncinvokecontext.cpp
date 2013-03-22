#include "edge.h"

Handle<Value> v8FuncCallback(const v8::Arguments& args)
{
    DBG("v8FuncCallback");
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_edgeContext")));
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
    DBG("NodejsFuncInvokeContext::NodejsFuncInvokeContext");
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
    DBG("NodejsFuncInvokeContext::!NodejsFuncInvokeContext");
    if (this->wrap)
    {
        delete this->wrap;
        this->wrap = NULL;
    }
}

void NodejsFuncInvokeContext::CallFuncOnV8Thread()
{
    DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread");
    HandleScope scope;
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
        callback->Set(v8::String::NewSymbol("_edgeContext"), v8::External::New((void*)this->wrap));
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
    DBG("NodejsFuncInvokeContext::Complete");
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
    DBG("NodejsFuncInvokeContext::CompleteWithError");
    this->exception = exception;
    Task::Run(gcnew System::Action(this, &NodejsFuncInvokeContext::Complete));
}

void NodejsFuncInvokeContext::CompleteWithResult(Handle<v8::Value> result)
{
    DBG("NodejsFuncInvokeContext::CompleteWithResult");
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
