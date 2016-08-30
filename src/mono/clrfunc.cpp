#include "edge.h"

#include "mono/metadata/assembly.h"
#include "mono/metadata/exception.h"
#include "mono/jit/jit.h"


ClrFunc::ClrFunc()
{
    // empty
}

NAN_METHOD(clrFuncProxy)
{
    DBG("clrFuncProxy");
    Nan::HandleScope scope;
    v8::Local<v8::External> correlator = v8::Local<v8::External>::Cast(info[2]);
    ClrFuncWrap* wrap = (ClrFuncWrap*)(correlator->Value());
    ClrFunc* clrFunc = wrap->clrFunc;
    info.GetReturnValue().Set(clrFunc->Call(info[0], info[1]));
}

template<typename T>
void clrFuncProxyNearDeath(const Nan::WeakCallbackInfo<T> &data)
{
    DBG("clrFuncProxyNearDeath");
    ClrFuncWrap* wrap = (ClrFuncWrap*)(data.GetParameter());
    delete wrap->clrFunc; 
    wrap->clrFunc = NULL;
    delete wrap;
}

v8::Local<v8::Function> ClrFunc::Initialize(MonoObject* func)
{
    DBG("ClrFunc::Initialize Func<object,Task<object>> wrapper");

    static Nan::Persistent<v8::Function> proxyFactory;
    static Nan::Persistent<v8::Function> proxyFunction;    

    Nan::EscapableHandleScope scope;

    ClrFunc* app = new ClrFunc();
    app->func = mono_gchandle_new(func, FALSE);
    ClrFuncWrap* wrap = new ClrFuncWrap;
    wrap->clrFunc = app;

    // See https://github.com/tjanczuk/edge/issues/128 for context
    
    if (proxyFactory.IsEmpty())
    {
        v8::Local<v8::Function> clrFuncProxyFunction = Nan::New<v8::FunctionTemplate>(clrFuncProxy)->GetFunction();
        proxyFunction.Reset(clrFuncProxyFunction);
        v8::Local<v8::String> code = Nan::New<v8::String>(
            "(function (f, ctx) { return function (d, cb) { return f(d, cb, ctx); }; })").ToLocalChecked();
        v8::Local<v8::Function> codeFunction = v8::Local<v8::Function>::Cast(v8::Script::Compile(code)->Run());
        proxyFactory.Reset(codeFunction);
    }

    v8::Local<v8::Value> factoryArgv[] = { Nan::New(proxyFunction), Nan::New<v8::External>((void*)wrap) };
    v8::Local<v8::Function> funcProxy = 
        (Nan::New(proxyFactory)->Call(Nan::GetCurrentContext()->Global(), 2, factoryArgv)).As<v8::Function>();
    Nan::Persistent<v8::Function> funcProxyPersistent(funcProxy);
    funcProxyPersistent.SetWeak((void*)wrap, &clrFuncProxyNearDeath, Nan::WeakCallbackType::kParameter);

    return scope.Escape(funcProxy);
}

NAN_METHOD(ClrFunc::Initialize)
{
    DBG("ClrFunc::Initialize MethodInfo wrapper");

    Nan::EscapableHandleScope scope;
    v8::Local<v8::Object> options = info[0]->ToObject();
    v8::Local<v8::Function> result;

    v8::Local<v8::Value> jsassemblyFile = options->Get(Nan::New<v8::String>("assemblyFile").ToLocalChecked());
    if (jsassemblyFile->IsString())
    {
        // reference .NET code through pre-compiled CLR assembly 
        String::Utf8Value assemblyFile(jsassemblyFile);
        String::Utf8Value nativeTypeName(options->Get(Nan::New<v8::String>("typeName").ToLocalChecked()));
        String::Utf8Value nativeMethodName(options->Get(Nan::New<v8::String>("methodName").ToLocalChecked()));
        MonoException* exc = NULL;
        MonoObject* func = MonoEmbedding::GetClrFuncReflectionWrapFunc(*assemblyFile, *nativeTypeName, *nativeMethodName, &exc);
        if (exc) {
            return Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(exc));
        }
        result = ClrFunc::Initialize(func);
    }
    else
    {
        //// reference .NET code throgh embedded source code that needs to be compiled
        MonoException* exc = NULL;

        String::Utf8Value compilerFile(options->Get(Nan::New<v8::String>("compiler").ToLocalChecked()));
        MonoAssembly *assembly = mono_domain_assembly_open (mono_domain_get(), *compilerFile);
        MonoClass* compilerClass = mono_class_from_name(mono_assembly_get_image(assembly), "", "EdgeCompiler");
        MonoObject* compilerInstance = mono_object_new(mono_domain_get(), compilerClass);
        MonoMethod* ctor = mono_class_get_method_from_name(compilerClass, ".ctor", 0);
        mono_runtime_invoke(ctor, compilerInstance, NULL, (MonoObject**)&exc);
        if (exc) {
            return Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(exc));
        }

        MonoMethod* compileFunc = mono_class_get_method_from_name(compilerClass, "CompileFunc", -1);
        MonoReflectionMethod* methodInfo = mono_method_get_object(mono_domain_get(),compileFunc, compilerClass);
        MonoClass* methodBase = mono_class_from_name(mono_get_corlib(), "System.Reflection", "MethodBase");
        MonoMethod* invoke = mono_class_get_method_from_name(methodBase, "Invoke", 2);
        if (compileFunc == NULL)
        {
            // exception
        }
        MonoObject* parameters = ClrFunc::MarshalV8ToCLR(options);
        MonoArray* methodInfoParams = mono_array_new(mono_domain_get(), mono_get_object_class(), 1);
        mono_array_setref(methodInfoParams, 0, parameters);
        void* params[2];
        params[0] = compilerInstance;
        params[1] = methodInfoParams;
        MonoObject* func = mono_runtime_invoke(invoke, methodInfo, params, (MonoObject**)&exc);
        if (exc) {
            return Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(exc));
        }

        result = ClrFunc::Initialize(func);
    }

    info.GetReturnValue().Set(result);
}

v8::Local<v8::Value> ClrFunc::MarshalCLRToV8(MonoObject* netdata, MonoException** exc)
{
    DBG("ClrFunc::MarshalCLRToV8");
    Nan::EscapableHandleScope scope;
    v8::Local<v8::Value> jsdata;
    *exc = NULL;

    if (netdata == NULL)
    {
        return scope.Escape(Nan::Null());
    }

    static MonoClass* guid_class;
    static MonoClass* idictionary_class;
    static MonoClass* idictionary_string_object_class;
    static MonoClass* ienumerable_class;
    static MonoClass* datetime_class;
    static MonoClass* uri_class;
    static MonoClass* datetimeoffset_class;

    if (!guid_class)
        guid_class = mono_class_from_name (mono_get_corlib(), "System", "Guid");
    if (!idictionary_class)
        idictionary_class = mono_class_from_name (mono_get_corlib(), "System.Collections", "IDictionary");
    if (!idictionary_string_object_class)
    {
        idictionary_string_object_class = MonoEmbedding::GetIDictionaryStringObjectClass(exc);
        if (*exc)
            ABORT_TODO();
    }
    if (!ienumerable_class)
        ienumerable_class = mono_class_from_name (mono_get_corlib(), "System.Collections", "IEnumerable");
    if (!datetime_class)
        datetime_class = mono_class_from_name (mono_get_corlib(), "System", "DateTime");
    if (!uri_class)
    {
        uri_class = MonoEmbedding::GetUriClass(exc);
        if (*exc)
            ABORT_TODO();
    }
    if (!datetimeoffset_class)
        datetimeoffset_class = mono_class_from_name (mono_get_corlib(), "System", "DateTimeOffset");

    MonoClass* klass = mono_object_get_class(netdata);
    MonoString* primitive = NULL;

// printf("CLR->V8 class: %s\n", mono_class_get_name(klass));
    if (klass == mono_get_string_class())
    {
        jsdata = stringCLR2V8((MonoString*)netdata);
    }
    else if (klass == mono_get_char_class())
    {
        MonoString* str = MonoEmbedding::ToString(netdata, exc);
        if (!*exc)
            jsdata = stringCLR2V8(str);
    }
    else if (klass == mono_get_boolean_class())
    {
        jsdata = Nan::New<v8::Boolean>((bool)*(bool*)mono_object_unbox(netdata));
    }
    else if (klass == guid_class)
    {
        MonoString* str = MonoEmbedding::ToString(netdata, exc);
        if (!*exc)
            jsdata = stringCLR2V8(str);
    }
    else if (klass == datetime_class)
    {
        double value = MonoEmbedding::GetDateValue(netdata, exc);
        if (!*exc)
            jsdata = Nan::New<v8::Date>(value).ToLocalChecked();
    }
    else if (klass == datetimeoffset_class)
    {
        MonoString* str = MonoEmbedding::ToString(netdata, exc);
        if (*exc)
            jsdata = stringCLR2V8(str);
    }
    else if (mono_class_is_assignable_from(uri_class, klass))
    {
        MonoString* str = MonoEmbedding::ToString(netdata, exc);
        if (!*exc)
            jsdata = stringCLR2V8(str);
    }
    else if (klass == mono_get_int16_class())
    {
        jsdata = Nan::New<v8::Integer>(*(int16_t*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_int32_class())
    {
        jsdata = Nan::New<v8::Integer>(*(int32_t*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_int64_class())
    {
        double val = MonoEmbedding::Int64ToDouble(netdata, exc);
        if (!*exc)
            jsdata = Nan::New<v8::Number>(val);
    }
    else if (klass == mono_get_double_class())
    {
        jsdata = Nan::New<v8::Number>(*(double*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_single_class())
    {
        jsdata = Nan::New<v8::Number>(*(float*)mono_object_unbox(netdata));
    }
    else if (NULL != (primitive = MonoEmbedding::TryConvertPrimitiveOrDecimal(netdata, exc)) || *exc)
    {
        if (!*exc)
            jsdata = stringCLR2V8(primitive);
    }
    else if (mono_class_is_enum(klass))
    {
        if(enableMarshalEnumAsInt)
        {
            jsdata = Nan::New<v8::Integer>(*(int32_t*)mono_object_unbox(netdata));
        }
        else
        {
            MonoString* str = MonoEmbedding::ToString(netdata, exc);
            if (!*exc)
                jsdata = stringCLR2V8(str);
        }
    }
    else if (mono_class_get_rank(klass) > 0 && mono_class_get_element_class(klass) == mono_get_byte_class())
    {
        MonoArray* buffer = (MonoArray*)netdata;
        size_t length = mono_array_length(buffer);      
        if (length > 0)
        {
            uint8_t* pinnedBuffer = mono_array_addr(buffer, uint8_t, 0);
            jsdata = Nan::CopyBuffer((char *)pinnedBuffer, length).ToLocalChecked();
        }
        else 
        {
            jsdata = Nan::NewBuffer(0).ToLocalChecked();
        }
    }
    else if (mono_class_is_assignable_from (idictionary_string_object_class, klass)
             || mono_class_is_assignable_from (idictionary_class, klass))
    {
        v8::Local<v8::Object> result = Nan::New<v8::Object>();
        MonoArray* kvs = MonoEmbedding::IDictionaryToFlatArray(netdata, exc);
        if (!*exc)
        {
            size_t length = mono_array_length(kvs);
            for (unsigned int i = 0; i < length && !*exc; i += 2)
            {
                MonoString* k = (MonoString*)mono_array_get(kvs, MonoObject*, i);
                MonoObject* v = mono_array_get(kvs, MonoObject*, i + 1);
                result->Set(
                    stringCLR2V8(k),
                    ClrFunc::MarshalCLRToV8(v, exc));
            }

            if (!*exc) 
                jsdata = result;
        }
    }
    else if (mono_class_is_assignable_from (ienumerable_class, klass))
    {
        v8::Local<v8::Array> result = Nan::New<v8::Array>();
        MonoArray* values = MonoEmbedding::IEnumerableToArray(netdata, exc);
        if (!*exc)
        {
            size_t length = mono_array_length(values);
            unsigned int i = 0;
            for (i = 0; i < length && !*exc; i++)
            {
                MonoObject* value = mono_array_get(values, MonoObject*, i);
                result->Set(i, ClrFunc::MarshalCLRToV8(value, exc));
            }

            if (!*exc)
                jsdata = result;
        }
    }
    else if (MonoEmbedding::GetFuncClass() == klass)
    {
        jsdata = ClrFunc::Initialize(netdata);
    }
    else if (mono_class_is_assignable_from(mono_get_exception_class(), klass))
    {
        jsdata = ClrFunc::MarshalCLRExceptionToV8((MonoException*)netdata);
    }
    else
    {
        jsdata = ClrFunc::MarshalCLRObjectToV8(netdata, exc);
    }

    if (*exc)
    {
        return scope.Escape(Nan::Undefined());
    }

    return scope.Escape(jsdata);
}

v8::Local<v8::Value> ClrFunc::MarshalCLRExceptionToV8(MonoException* exception)
{
    DBG("ClrFunc::MarshalCLRExceptionToV8");
    Nan::EscapableHandleScope scope;
    v8::Local<v8::Object> result;
    v8::Local<v8::String> message;
    v8::Local<v8::String> name;
    MonoException* exc = NULL;
        
    if (exception == NULL)
    {
        result = Nan::New<v8::Object>();
        message = Nan::New<v8::String>("Unrecognized exception thrown by CLR.").ToLocalChecked();
        name = Nan::New<v8::String>("InternalException").ToLocalChecked();
    }
    else
    {
        MonoEmbedding::NormalizeException(&exception);

        result = ClrFunc::MarshalCLRObjectToV8((MonoObject*)exception, &exc);

        MonoClass* klass = mono_object_get_class((MonoObject*)exception);
        MonoProperty* prop = mono_class_get_property_from_name(klass, "Message");
        message = stringCLR2V8((MonoString*)mono_property_get_value(prop, exception, NULL, NULL));

        const char* namespaceName = mono_class_get_namespace(klass);
        const char* className = mono_class_get_name(klass);
        char full_name[strlen(namespaceName) + 1 + strlen(className) + 1]; 
        strcpy(full_name, namespaceName);
        strcat(full_name, ".");
        strcat(full_name, className);
        name = stringCLR2V8(mono_string_new_wrapper(full_name));
    }   
    
    // Construct an error that is just used for the prototype - not verify efficient
    // but 'typeof Error' should work in JavaScript
    result->SetPrototype(v8::Exception::Error(message));
    result->Set(Nan::New<v8::String>("message").ToLocalChecked(), message);
    
    // Recording the actual type - 'name' seems to be the common used property
    result->Set(Nan::New<v8::String>("name").ToLocalChecked(), name);

    return scope.Escape(result);
}

v8::Local<v8::Object> ClrFunc::MarshalCLRObjectToV8(MonoObject* netdata, MonoException** exc)
{
    DBG("ClrFunc::MarshalCLRObjectToV8");
    Nan::EscapableHandleScope scope;
    v8::Local<v8::Object> result = Nan::New<v8::Object>();
    MonoClass* klass = mono_object_get_class(netdata);
    MonoClass* current;
    MonoClassField* field;
    MonoProperty* prop;
    void* iter = NULL;
    *exc = NULL;

    if ((0 == strcmp(mono_class_get_name(klass), "MonoType")
        && 0 == strcmp(mono_class_get_namespace(klass), "System"))
        || 0 == strcmp(mono_class_get_namespace(klass), "System.Reflection"))
    {
        // Avoid stack overflow due to self-referencing reflection elements
        return scope.Escape(result);
    }
    for (current = klass; !*exc && current; current = mono_class_get_parent(current))
    {
        iter = NULL;
        while (NULL != (field = mono_class_get_fields(current, &iter)) && !*exc)
        {
            // magic numbers
            static uint32_t field_attr_static = 0x0010;
            static uint32_t field_attr_public = 0x0006;
            if (mono_field_get_flags(field) & field_attr_static)
                continue;
            if (!(mono_field_get_flags(field) & field_attr_public))
                continue;
            const char* name = mono_field_get_name(field);
            MonoObject* value = mono_field_get_value_object(mono_domain_get(), field, netdata);
            result->Set(
                Nan::New<v8::String>(name).ToLocalChecked(), 
                ClrFunc::MarshalCLRToV8(value, exc));
        }
    }

    for (current = klass; !*exc && current; current = mono_class_get_parent(current))
    {
        iter = NULL;
        while (!*exc && NULL != (prop = mono_class_get_properties(current, &iter)))
        {
            // magic numbers
            static uint32_t method_attr_static = 0x0010;
            static uint32_t method_attr_public = 0x0006;
            MonoMethod* getMethod = mono_property_get_get_method(prop);
            if (!getMethod)
                continue;
            if (mono_method_get_flags(getMethod, NULL) & method_attr_static)
                continue;
            if (!(mono_method_get_flags(getMethod, NULL) & method_attr_public))
                continue;

            const char* name = mono_property_get_name(prop);       
            MonoObject* value = mono_runtime_invoke(getMethod, netdata, NULL, (MonoObject**)exc);
            if (!*exc)
            {
                result->Set(
                    Nan::New<v8::String>(name).ToLocalChecked(),
                    ClrFunc::MarshalCLRToV8(value, exc));
            }
        }
    }

    if (*exc) 
    {
        return scope.Escape(v8::Local<v8::Object>::Cast(ClrFunc::MarshalCLRExceptionToV8(*exc)));
    }

    return scope.Escape(result);
}

MonoObject* ClrFunc::MarshalV8ToCLR(v8::Local<v8::Value> jsdata)
{
    DBG("ClrFunc::MarshalV8ToCLR");
    Nan::HandleScope scope;

    if (jsdata->IsFunction())
    {
        NodejsFunc* functionContext = new NodejsFunc(v8::Local<v8::Function>::Cast(jsdata));
        MonoObject* netfunc = functionContext->GetFunc();

        return netfunc;
    }
    else if (node::Buffer::HasInstance(jsdata))
    {
        v8::Local<v8::Object> jsbuffer = jsdata->ToObject();
        MonoArray* netbuffer = mono_array_new(mono_domain_get(), mono_get_byte_class(), (int)node::Buffer::Length(jsbuffer));
        memcpy(mono_array_addr_with_size(netbuffer, sizeof(char), 0), node::Buffer::Data(jsbuffer), mono_array_length(netbuffer));

        return (MonoObject*)netbuffer;
    }
    else if (jsdata->IsArray())
    {
        v8::Local<v8::Array> jsarray = v8::Local<v8::Array>::Cast(jsdata);
        MonoArray* netarray = mono_array_new(mono_domain_get(), mono_get_object_class(), jsarray->Length());
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            mono_array_setref(netarray, i, ClrFunc::MarshalV8ToCLR(jsarray->Get(i)));
        }

        return (MonoObject*)netarray;
    }
    else if (jsdata->IsDate())
    {
        v8::Local<v8::Date> jsdate = v8::Local<v8::Date>::Cast(jsdata);
        double ticks = jsdate->NumberValue();
        return MonoEmbedding::CreateDateTime(ticks);
    }    
    else if (jsdata->IsObject()) 
    {
        MonoObject* netobject = MonoEmbedding::CreateExpandoObject();
        v8::Local<v8::Object> jsobject = v8::Local<v8::Object>::Cast(jsdata);
        v8::Local<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            v8::Local<v8::String> name = v8::Local<v8::String>::Cast(propertyNames->Get(i));
            v8::String::Utf8Value utf8name(name);
            Dictionary::Add(netobject, *utf8name, ClrFunc::MarshalV8ToCLR(jsobject->Get(name)));
        }

        return netobject;
    }
    else if (jsdata->IsString()) 
    {
        return (MonoObject*)stringV82CLR(v8::Local<v8::String>::Cast(jsdata));
    }
    else if (jsdata->IsBoolean())
    {
        bool value = jsdata->BooleanValue();
        return mono_value_box(mono_domain_get(), mono_get_boolean_class(), &value);
    }
    else if (jsdata->IsInt32())
    {
        int32_t value = jsdata->Int32Value();
        return mono_value_box(mono_domain_get(), mono_get_int32_class(), &value);
    }
    else if (jsdata->IsUint32()) 
    {
        uint32_t value = jsdata->Uint32Value();
        return mono_value_box(mono_domain_get(), mono_get_uint32_class(), &value);
    }
    else if (jsdata->IsNumber()) 
    {
        double value = jsdata->NumberValue();
        return mono_value_box(mono_domain_get(), mono_get_double_class(), &value);
    }
    else if (jsdata->IsUndefined() || jsdata->IsNull())
    {
        return NULL;
    }
    else
    {
        ABORT_TODO();
        //throw gcnew System::Exception("Unable to convert V8 value to CLR value.");
    }
}

v8::Local<v8::Value> ClrFunc::Call(v8::Local<v8::Value> payload, v8::Local<v8::Value> callback)
{
    DBG("ClrFunc::Call instance");
    Nan::EscapableHandleScope scope;
    MonoException* exc = NULL;

    ClrFuncInvokeContext* c = new ClrFuncInvokeContext(callback);
    c->Payload(ClrFunc::MarshalV8ToCLR(payload));

    MonoObject* func = mono_gchandle_get_target(this->func);
    void* params[1];
    params[0] = c->Payload();
    MonoMethod* invoke = mono_class_get_method_from_name(mono_object_get_class(func), "Invoke", -1);
    // This is different from dotnet. From the documentation http://www.mono-project.com/Embedding_Mono: 
    MonoObject* task = mono_runtime_invoke(invoke, func, params, (MonoObject**)&exc);
    if (exc)
    {
        delete c;
        c = NULL;
        Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(exc));
        return scope.Escape(Nan::Undefined());
    }

    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(task), "IsCompleted");
    MonoObject* isCompletedObject = mono_property_get_value(prop, task, NULL, (MonoObject**)&exc);
    if (exc)
    {
        delete c;
        c = NULL;
        Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(exc));
        return scope.Escape(Nan::Undefined());
    }

    bool isCompleted = *(bool*)mono_object_unbox(isCompletedObject);
    if (isCompleted)
    {
        // Completed synchronously. Return a value or invoke callback based on call pattern.
        c->Task(task);
        return scope.Escape(c->CompleteOnV8Thread(true));
    }
    else if (c->Sync())
    {
        Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(mono_get_exception_invalid_operation("The JavaScript function was called synchronously "
            "but the underlying CLR function returned without completing the Task. Call the "
            "JavaScript function asynchronously.")));
        return scope.Escape(Nan::Undefined());
    }
    else
    {
        c->InitializeAsyncOperation();

        MonoEmbedding::ContinueTask(task, c->GetMonoObject(), &exc);
        if (exc)
        {
            delete c;
            c = NULL;
            Nan::ThrowError(ClrFunc::MarshalCLRExceptionToV8(exc));
            return scope.Escape(Nan::Undefined());
        }
    }

    return scope.Escape(Nan::Undefined());
}
