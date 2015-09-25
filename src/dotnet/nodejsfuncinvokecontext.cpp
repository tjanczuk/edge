/**
 * Portions Copyright (c) Microsoft Corporation. All rights reserved. 
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0  
 *
 * THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION 
 * ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR 
 * PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
 *
 * See the Apache Version 2.0 License for specific language governing 
 * permissions and limitations under the License.
 */
#include "edge.h"

NAN_METHOD(v8FuncCallback)
{
    DBG("v8FuncCallback");
    Nan::HandleScope scope;
    v8::Local<v8::External> correlator = v8::Local<v8::External>::Cast(info[2]);
    NodejsFuncInvokeContextWrap* wrap = (NodejsFuncInvokeContextWrap*)(correlator->Value());
    NodejsFuncInvokeContext^ context = wrap->context;    
    wrap->context = nullptr;
    if (!info[0]->IsUndefined() && !info[0]->IsNull())
    {
        DBG("v8FuncCallback error");
        context->CompleteWithError(gcnew System::Exception(exceptionV82stringCLR(info[0])));
    }
    else 
    {
        DBG("v8FuncCallback success");
        context->CompleteWithResult(info[1]);
    }
    info.GetReturnValue().SetUndefined();
}

NodejsFuncInvokeContext::NodejsFuncInvokeContext(
        NodejsFunc^ functionContext, System::Object^ payload)
{
    DBG("NodejsFuncInvokeContext::NodejsFuncInvokeContext");
    this->functionContext = functionContext;
    this->payload = payload;
    this->TaskCompletionSource = gcnew System::Threading::Tasks::TaskCompletionSource<System::Object^>();
    this->wrap = NULL;
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

    static Nan::Persistent<v8::Function> callbackFactory;
    static Nan::Persistent<v8::Function> callbackFunction;

    Nan::HandleScope scope;
    try 
    {
        v8::Local<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(this->payload);

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

        this->wrap = new NodejsFuncInvokeContextWrap;
        this->wrap->context = this;
        v8::Local<v8::Value> factoryArgv[] = { Nan::New(callbackFunction), Nan::New<v8::External>((void*)this->wrap) };
        v8::Local<v8::Function> callback = v8::Local<v8::Function>::Cast(
            Nan::New(callbackFactory)->Call(Nan::GetCurrentContext()->Global(), 2, factoryArgv));        

        v8::Local<v8::Value> argv[] = { jspayload, callback };
        Nan::TryCatch tryCatch;
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread calling JavaScript function");
        Nan::New(*(this->functionContext->Func))->Call(Nan::GetCurrentContext()->Global(), 2, argv);
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread called JavaScript function");
        if (tryCatch.HasCaught()) 
        {
            DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread caught JavaScript exception");
            this->wrap->context = nullptr;
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

void NodejsFuncInvokeContext::CompleteWithResult(v8::Local<v8::Value> result)
{
    DBG("NodejsFuncInvokeContext::CompleteWithResult");
    try 
    {
        this->result = ClrFunc::MarshalV8ToCLR(result);
        Task::Run(gcnew System::Action(this, &NodejsFuncInvokeContext::Complete));
    }
    catch (System::Exception^ e)
    {
        this->CompleteWithError(e);
    }
}
