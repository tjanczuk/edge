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
    NanScope();
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
    NodejsFuncInvokeContextWrap* wrap = (NodejsFuncInvokeContextWrap*)(correlator->Value());
    NodejsFuncInvokeContext^ context = wrap->context;    
    wrap->context = nullptr;
    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
        DBG("v8FuncCallback error");
        context->CompleteWithError(gcnew System::Exception(exceptionV82stringCLR(args[0])));
    }
    else 
    {
        DBG("v8FuncCallback success");
        context->CompleteWithResult(args[1]);
    }
    NanReturnValue(NanUndefined());
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

    static Persistent<v8::Function> callbackFactory;
    static Persistent<v8::Function> callbackFunction;

    NanScope();
    try 
    {
        Handle<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(this->payload);

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

        this->wrap = new NodejsFuncInvokeContextWrap;
        this->wrap->context = this;
        Handle<v8::Value> factoryArgv[] = { NanNew(callbackFunction), NanNew<v8::External>((void*)this->wrap) };
        Handle<v8::Function> callback = Handle<v8::Function>::Cast(
            NanNew(callbackFactory)->Call(NanGetCurrentContext()->Global(), 2, factoryArgv));        

        Handle<v8::Value> argv[] = { jspayload, callback };
        TryCatch tryCatch;
        DBG("NodejsFuncInvokeContext::CallFuncOnV8Thread calling JavaScript function");
        NanNew(*(this->functionContext->Func))->Call(NanGetCurrentContext()->Global(), 2, argv);
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

void NodejsFuncInvokeContext::CompleteWithResult(Handle<v8::Value> result)
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
