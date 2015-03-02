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
    NanEscapableScope();
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args[2]);
    ClrFuncWrap* wrap = (ClrFuncWrap*)(correlator->Value());
    ClrFunc* clrFunc = wrap->clrFunc;
    NanReturnValue(clrFunc->Call(args[0], args[1]));
}

NAN_WEAK_CALLBACK(clrFuncProxyNearDeath)
{
    DBG("clrFuncProxyNearDeath");
    ClrFuncWrap* wrap = (ClrFuncWrap*)(data.GetParameter());
    wrap->clrFunc = NULL;
    delete wrap;
}

Handle<v8::Function> ClrFunc::Initialize(MonoObject* func)
{
    DBG("ClrFunc::Initialize Func<object,Task<object>> wrapper");

    static Persistent<v8::Function> proxyFactory;
    static Persistent<v8::Function> proxyFunction;    

    NanEscapableScope();

    ClrFunc* app = new ClrFunc();
    app->func = mono_gchandle_new(func, FALSE);
    ClrFuncWrap* wrap = new ClrFuncWrap;
    wrap->clrFunc = app;

    // See https://github.com/tjanczuk/edge/issues/128 for context
    
    if (proxyFactory.IsEmpty())
    {
        NanAssignPersistent(
            proxyFunction, NanNew<FunctionTemplate>(clrFuncProxy)->GetFunction());
        Handle<v8::String> code = NanNew<v8::String>(
            "(function (f, ctx) { return function (d, cb) { return f(d, cb, ctx); }; })");
        NanAssignPersistent(
            proxyFactory, Handle<v8::Function>::Cast(v8::Script::Compile(code)->Run()));
    }

    Handle<v8::Value> factoryArgv[] = { NanNew(proxyFunction), NanNew<v8::External>((void*)wrap) };
    Local<v8::Function> funcProxy = 
            (NanNew(proxyFactory)->Call(NanGetCurrentContext()->Global(), 2, factoryArgv)).As<v8::Function>();
    NanMakeWeakPersistent(funcProxy, (void*)wrap, &clrFuncProxyNearDeath);

    return NanEscapeScope(funcProxy);
}

NAN_METHOD(ClrFunc::Initialize)
{
    DBG("ClrFunc::Initialize MethodInfo wrapper");

    NanEscapableScope();
    Handle<v8::Object> options = args[0]->ToObject();
    Handle<v8::Function> result;

    Handle<v8::Value> jsassemblyFile = options->Get(NanNew<v8::String>("assemblyFile"));
    if (jsassemblyFile->IsString())
    {
        // reference .NET code through pre-compiled CLR assembly 
        String::Utf8Value assemblyFile(jsassemblyFile);
        String::Utf8Value nativeTypeName(options->Get(NanNew<v8::String>("typeName")));
        String::Utf8Value nativeMethodName(options->Get(NanNew<v8::String>("methodName")));
        MonoException* exc = NULL;
        MonoObject* func = MonoEmbedding::GetClrFuncReflectionWrapFunc(*assemblyFile, *nativeTypeName, *nativeMethodName, &exc);
        if (exc) {
            throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(exc));
            NanReturnValue(NanUndefined());
        }
        result = ClrFunc::Initialize(func);
    }
    else
    {
        //// reference .NET code throgh embedded source code that needs to be compiled
        MonoException* exc = NULL;

        String::Utf8Value compilerFile(options->Get(NanNew<v8::String>("compiler")));
        MonoAssembly *assembly = mono_domain_assembly_open (mono_domain_get(), *compilerFile);
        MonoClass* compilerClass = mono_class_from_name(mono_assembly_get_image(assembly), "", "EdgeCompiler");
        MonoObject* compilerInstance = mono_object_new(mono_domain_get(), compilerClass);
        MonoMethod* ctor = mono_class_get_method_from_name(compilerClass, ".ctor", 0);
        mono_runtime_invoke(ctor, compilerInstance, NULL, (MonoObject**)&exc);
        if (exc) {
            throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(exc));
            NanReturnValue(NanUndefined());
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
            throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(exc));
            NanReturnValue(NanUndefined());
        }

        result = ClrFunc::Initialize(func);
    }

    NanReturnValue(result);
}

Handle<v8::Value> ClrFunc::MarshalCLRToV8(MonoObject* netdata, MonoException** exc)
{
    DBG("ClrFunc::MarshalCLRToV8");
    NanEscapableScope();
    Handle<v8::Value> jsdata;
    *exc = NULL;

    if (netdata == NULL)
    {
        return NanEscapeScope(NanNull());
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
        jsdata = NanNew<v8::Boolean>((bool)*(bool*)mono_object_unbox(netdata));
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
            jsdata = NanNew<v8::Date>(value);
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
        jsdata = NanNew<v8::Integer>(*(int16_t*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_int32_class())
    {
        jsdata = NanNew<v8::Integer>(*(int32_t*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_int64_class())
    {
        double val = MonoEmbedding::Int64ToDouble(netdata, exc);
        if (!*exc)
            jsdata = NanNew<v8::Number>(val);
    }
    else if (klass == mono_get_double_class())
    {
        jsdata = NanNew<v8::Number>(*(double*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_single_class())
    {
        jsdata = NanNew<v8::Number>(*(float*)mono_object_unbox(netdata));
    }
    else if (NULL != (primitive = MonoEmbedding::TryConvertPrimitiveOrDecimal(netdata, exc)) || *exc)
    {
        if (!*exc)
            jsdata = stringCLR2V8(primitive);
    }
    else if (mono_class_is_enum(klass))
    {
        MonoString* str = MonoEmbedding::ToString(netdata, exc);
        if (!*exc)
            jsdata = stringCLR2V8(str);
    }
    else if (mono_class_get_rank(klass) > 0 && mono_class_get_element_class(klass) == mono_get_byte_class())
    {
        MonoArray* buffer = (MonoArray*)netdata;
        size_t length = mono_array_length(buffer);
        if (length > 0)
        {
            uint8_t* pinnedBuffer = mono_array_addr(buffer, uint8_t, 0);
            jsdata = NanNewBufferHandle((char *)pinnedBuffer, length);
        }
        else 
        {
            jsdata = NanNewBufferHandle(0);
        }
    }
    else if (mono_class_is_assignable_from (idictionary_string_object_class, klass)
             || mono_class_is_assignable_from (idictionary_class, klass))
    {
        Handle<v8::Object> result = NanNew<v8::Object>();
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
        Handle<v8::Array> result = NanNew<v8::Array>();
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
        return NanEscapeScope(NanUndefined());
    }

    return NanEscapeScope(jsdata);
}

Handle<v8::Object> ClrFunc::MarshalCLRExceptionToV8(MonoException* exception)
{
    DBG("ClrFunc::MarshalCLRExceptionToV8");
    NanEscapableScope();
    Handle<v8::Object> result;
    Handle<v8::String> message;
    Handle<v8::String> name;
    MonoException* exc = NULL;
        
    if (exception == NULL)
    {
        result = NanNew<v8::Object>();
        message = NanNew<v8::String>("Unrecognized exception thrown by CLR.");
        name = NanNew<v8::String>("InternalException");
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
    result->Set(NanNew<v8::String>("message"), message);
    
    // Recording the actual type - 'name' seems to be the common used property
    result->Set(NanNew<v8::String>("name"), name);

    return NanEscapeScope(result);
}

Handle<v8::Object> ClrFunc::MarshalCLRObjectToV8(MonoObject* netdata, MonoException** exc)
{
    DBG("ClrFunc::MarshalCLRObjectToV8");
    NanEscapableScope();
    Handle<v8::Object> result = NanNew<v8::Object>();
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
        return NanEscapeScope(result);
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
                NanNew<v8::String>(name), 
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
                    NanNew<v8::String>(name),
                    ClrFunc::MarshalCLRToV8(value, exc));
            }
        }
    }

    if (*exc) 
    {
        return NanEscapeScope(ClrFunc::MarshalCLRExceptionToV8(*exc));
    }

    return NanEscapeScope(result);
}

MonoObject* ClrFunc::MarshalV8ToCLR(Handle<v8::Value> jsdata)
{
    DBG("ClrFunc::MarshalV8ToCLR");
    NanScope();

    if (jsdata->IsFunction())
    {
        NodejsFunc* functionContext = new NodejsFunc(Handle<v8::Function>::Cast(jsdata));
        MonoObject* netfunc = functionContext->GetFunc();

        return netfunc;
    }
    else if (node::Buffer::HasInstance(jsdata))
    {
        Handle<v8::Object> jsbuffer = jsdata->ToObject();
        MonoArray* netbuffer = mono_array_new(mono_domain_get(), mono_get_byte_class(), (int)node::Buffer::Length(jsbuffer));
        memcpy(mono_array_addr_with_size(netbuffer, sizeof(char), 0), node::Buffer::Data(jsbuffer), mono_array_length(netbuffer));

        return (MonoObject*)netbuffer;
    }
    else if (jsdata->IsArray())
    {
        Handle<v8::Array> jsarray = Handle<v8::Array>::Cast(jsdata);
        MonoArray* netarray = mono_array_new(mono_domain_get(), mono_get_object_class(), jsarray->Length());
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            mono_array_setref(netarray, i, ClrFunc::MarshalV8ToCLR(jsarray->Get(i)));
        }

        return (MonoObject*)netarray;
    }
    else if (jsdata->IsDate())
    {
        Handle<v8::Date> jsdate = Handle<v8::Date>::Cast(jsdata);
        double ticks = jsdate->NumberValue();
        return MonoEmbedding::CreateDateTime(ticks);
    }    
    else if (jsdata->IsObject()) 
    {
        MonoObject* netobject = MonoEmbedding::CreateExpandoObject();
        Handle<v8::Object> jsobject = Handle<v8::Object>::Cast(jsdata);
        Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
            String::Utf8Value utf8name(name);
            Dictionary::Add(netobject, *utf8name, ClrFunc::MarshalV8ToCLR(jsobject->Get(name)));
        }

        return netobject;
    }
    else if (jsdata->IsString()) 
    {
        return (MonoObject*)stringV82CLR(Handle<v8::String>::Cast(jsdata));
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

Handle<v8::Value> ClrFunc::Call(Handle<v8::Value> payload, Handle<v8::Value> callback)
{
    DBG("ClrFunc::Call instance");
    NanEscapableScope();
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
        throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(exc));
        return NanEscapeScope(NanUndefined());
    }

    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(task), "IsCompleted");
    MonoObject* isCompletedObject = mono_property_get_value(prop, task, NULL, (MonoObject**)&exc);
    if (exc)
    {
        delete c;
        c = NULL;
        throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(exc));
        return NanEscapeScope(NanUndefined());
    }

    bool isCompleted = *(bool*)mono_object_unbox(isCompletedObject);
    if (isCompleted)
    {
        // Completed synchronously. Return a value or invoke callback based on call pattern.
        c->Task(task);
        return NanEscapeScope(c->CompleteOnV8Thread(true));
    }
    else if (c->Sync())
    {
        throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(mono_get_exception_invalid_operation("The JavaScript function was called synchronously "
            "but the underlying CLR function returned without completing the Task. Call the "
            "JavaScript function asynchronously.")));
        return NanEscapeScope(NanUndefined());
    }
    else
    {
        c->InitializeAsyncOperation();

        MonoEmbedding::ContinueTask(task, c->GetMonoObject(), &exc);
        if (exc)
        {
            delete c;
            c = NULL;
            throwV8Exception(ClrFunc::MarshalCLRExceptionToV8(exc));
            return NanEscapeScope(NanUndefined());
        }
    }

    return NanEscapeScope(NanUndefined()); 
}

// vim: ts=4 sw=4 et: 
