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

// TJF was Handle<Value> 

void v8FuncCallback(const v8::FunctionCallbackInfo<Value>& args)
{
    DBG("v8FuncCallback");
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
	Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
    NodejsFuncInvokeContextWrap* wrap = (NodejsFuncInvokeContextWrap*)(correlator->Value());
    NodejsFuncInvokeContext^ context = wrap->context;    
    wrap->context = nullptr;
    if (!args[0]->IsUndefined() && !args[0]->IsNull())
    {
        context->CompleteWithError(gcnew System::Exception(exceptionV82stringCLR(args[0])));
    }
    else 
    {
        context->CompleteWithResult(args[1]);
    }
	args.GetReturnValue().Set(Undefined( isolate ));
//  return scope.Close(Undefined());
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

    static Persistent<v8::Function, CopyablePersistentTraits<Function>> callbackFactory;
    static Persistent<v8::Function, CopyablePersistentTraits<Function>> callbackFunction;

	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
    try 
    {
        Handle<v8::Value> jspayload = ClrFunc::MarshalCLRToV8(this->payload);

        // See https://github.com/tjanczuk/edge/issues/125 for context
        
        if (callbackFactory.IsEmpty())
        {
			//... TJF -- the use of the Persistent constructor involves a copy; and the default
			//...        trait is the NonCopyable treat.  It may be a problem.

			callbackFunction = Persistent<v8::Function, CopyablePersistentTraits<Function>>(
				isolate, FunctionTemplate::New(isolate, v8FuncCallback)->GetFunction());
				
			Handle<v8::String> code = v8::String::NewFromUtf8( isolate,
                "(function (cb, ctx) { return function (e, d) { return cb(e, d, ctx); }; })");

			callbackFactory = Persistent<v8::Function, CopyablePersistentTraits<Function>>( isolate,
                Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
        }

        this->wrap = new NodejsFuncInvokeContextWrap;
        this->wrap->context = this;

		Handle<v8::Value> factoryArgv[] = {
			v8::Local<Function>::New(isolate, callbackFunction),
			v8::External::New(isolate, (void*)this->wrap)
		};

		Local<Function> localCallbackFactory = v8::Local<Function>::New(isolate, callbackFactory);
		Local<Value> globalToLocalValue = Local<Value>::New(isolate, isolate->GetCurrentContext()->Global());
		Handle<v8::Function> callback = Handle<v8::Function>::Cast(localCallbackFactory->Call(globalToLocalValue, 2, factoryArgv));

        Handle<v8::Value> argv[] = { jspayload, callback };

		TryCatch tryCatch;
        
		Local<Function> localFunctionContextFunc = Local<Function>::New(isolate, *(this->functionContext->Func));
		localFunctionContextFunc->Call(globalToLocalValue, 2, argv);

// TJF	(*(this->functionContext->Func))->Call(v8::Context::GetCurrent()->Global(), 2, argv);

		if (tryCatch.HasCaught()) 
        {
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
