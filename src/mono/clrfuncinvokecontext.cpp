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

static MonoClass* GetClrFuncInvokeContextClass()
{
    static MonoClass* klass;

    if (!klass)
        klass = mono_class_from_name(MonoEmbedding::GetImage(), "", "ClrFuncInvokeContext");

    return klass;
}

ClrFuncInvokeContext::ClrFuncInvokeContext(Handle<v8::Value> callbackOrSync) : _this(0), callback(0), uv_edge_async(0)
{
    static MonoClassField* field;
    static MonoClassField* syncField;

    if (!field)
        field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), "native");
    if (!syncField)
        syncField = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), "Sync");

    MonoObject* obj = mono_object_new(mono_domain_get(), GetClrFuncInvokeContextClass());

    mono_field_set_value(obj, field, this);
    this->_this = mono_gchandle_new(obj, FALSE);

    DBG("ClrFuncInvokeContext::ClrFuncInvokeContext");
    if (callbackOrSync->IsFunction())
    {
        this->callback = new Persistent<Function>();
        *(this->callback) = Persistent<Function>::New(Handle<Function>::Cast(callbackOrSync));
        this->Sync(FALSE);
    }
    else 
    {
        this->Sync(callbackOrSync->BooleanValue());
    }

    MonoMethod* getAction = mono_class_get_method_from_name(GetClrFuncInvokeContextClass(), "GetCompleteOnV8ThreadAsynchronousAction", -1);
    this->uv_edge_async = V8SynchronizationContext::RegisterAction(mono_runtime_invoke(getAction, obj, NULL, NULL));
}

void ClrFuncInvokeContext::DisposeCallback()
{
    if (this->callback)
    {
        DBG("ClrFuncInvokeContext::DisposeCallback");
        (*(this->callback)).Dispose();
        (*(this->callback)).Clear();
        //delete this->callback;
        this->callback = NULL;        
    }
}

void ClrFuncInvokeContext::CompleteOnCLRThread(MonoObject* task)
{
    DBG("ClrFuncInvokeContext::CompleteOnCLRThread");
    this->Task(task);
    V8SynchronizationContext::ExecuteAction(this->uv_edge_async);
}

void ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous(ClrFuncInvokeContext *_this)
{
    HandleScope scope;
    _this->CompleteOnV8Thread(false);
}

Handle<v8::Value> ClrFuncInvokeContext::CompleteOnV8Thread(bool completedSynchronously)
{
    DBG("ClrFuncInvokeContext::CompleteOnV8Thread");

    HandleScope handleScope;
    if (completedSynchronously)
    {
        V8SynchronizationContext::CancelAction(this->uv_edge_async);
        this->uv_edge_async = NULL;
    }

    if (!this->Sync() && !this->callback)
    {
        // this was an async call without callback specified
        return handleScope.Close(Undefined());
    }

    Handle<Value> argv[] = { Undefined(), Undefined() };
    int argc = 1;

    switch (Task::Status(this->Task())) {
        default:
            argv[0] = v8::String::New("The operation reported completion in an unexpected state.");
        break;
        case Task::Faulted:
            if (Task::Exception(this->Task()) != NULL) {
                argv[0] = exceptionCLR2stringV8(Task::Exception(this->Task()));
            }
            else {
                argv[0] = v8::String::New("The operation has failed with an undetermined error.");
            }
        break;
        case Task::Canceled:
            argv[0] = v8::String::New("The operation was cancelled.");
        break;
        case Task::RanToCompletion:
            argc = 2;
            TryCatch try_catch;
            argv[1] = ClrFunc::MarshalCLRToV8(Task::Result(this->Task()));
            if (try_catch.HasCaught()) 
            {
                argc = 1;
                argv[0] = try_catch.Exception();
            }
        break;
    };

    if (!this->Sync())
    {
        // complete the asynchronous call to C# by invoking a callback in JavaScript
        TryCatch try_catch;
        (*(this->callback))->Call(v8::Context::GetCurrent()->Global(), argc, argv);
        this->DisposeCallback();
        if (try_catch.HasCaught()) 
        {
            node::FatalException(try_catch);
        }        

        return handleScope.Close(Undefined());
    }
    else if (1 == argc) 
    {
        // complete the synchronous call to C# by re-throwing the resulting exception
        return handleScope.Close(ThrowException(argv[0]));
    }
    else
    {
        // complete the synchronous call to C# by returning the result
        return handleScope.Close(argv[1]);
    }
}

#define IMPLEMENT_REF_FIELD(T, Name)\
    static MonoClassField* Name ## Field;\
    T ClrFuncInvokeContext::Name()\
    {\
        if (!Name ## Field)\
        Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
        T value;\
        mono_field_get_value(mono_gchandle_get_target(_this), Name ## Field, &value);\
        return value;\
    }\
    void ClrFuncInvokeContext::Name(T value)\
    {\
        if (!Name ## Field)\
            Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
        mono_field_set_value(mono_gchandle_get_target(_this), Name ## Field, value);\
    }\

#define IMPLEMENT_FIELD(T, Name)\
    static MonoClassField* Name ## Field;\
    T ClrFuncInvokeContext::Name()\
    {\
        if (!Name ## Field)\
        Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
        T value;\
        mono_field_get_value(mono_gchandle_get_target(_this), Name ## Field, &value);\
        return value;\
    }\
    void ClrFuncInvokeContext::Name(T value)\
    {\
        if (!Name ## Field)\
            Name ## Field = mono_class_get_field_from_name(GetClrFuncInvokeContextClass(), #Name);\
        mono_field_set_value(mono_gchandle_get_target(_this), Name ## Field, &value);\
    }\

IMPLEMENT_REF_FIELD(MonoObject*, Payload)
IMPLEMENT_REF_FIELD(MonoObject*, Task)
IMPLEMENT_FIELD(bool, Sync)

// vim: ts=4 sw=4 et: 
