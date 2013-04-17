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

NodejsFunc::NodejsFunc(Handle<Function> function)
{
    DBG("NodejsFunc::NodejsFunc");
    this->Func = new Persistent<Function>;
    *(this->Func) = Persistent<Function>::New(function);
}

NodejsFunc::~NodejsFunc()
{
    this->!NodejsFunc();
}

NodejsFunc::!NodejsFunc()
{
    DBG("NodejsFunc::!NodejsFunc");
    PersistentDisposeContext^ context = gcnew PersistentDisposeContext((Persistent<Value>*)this->Func);
    uv_edge_async_t* uv_edge_async = V8SynchronizationContext::RegisterAction(
        gcnew System::Action(context, &PersistentDisposeContext::CallDisposeOnV8Thread));
    V8SynchronizationContext::ExecuteAction(uv_edge_async);
}

Task<System::Object^>^ NodejsFunc::FunctionWrapper(System::Object^ payload)
{
    DBG("NodejsFunc::FunctionWrapper");
    NodejsFuncInvokeContext^ context = gcnew NodejsFuncInvokeContext(this, payload);
    uv_edge_async_t* uv_edge_async = V8SynchronizationContext::RegisterAction(
        gcnew System::Action(context, &NodejsFuncInvokeContext::CallFuncOnV8Thread));
    V8SynchronizationContext::ExecuteAction(uv_edge_async);

    return context->TaskCompletionSource->Task;
}
