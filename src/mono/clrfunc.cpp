#include "edge.h"

#include "mono/metadata/assembly.h"
#include "mono/metadata/exception.h"
#include "mono/jit/jit.h"


ClrFunc::ClrFunc()
{
    // empty
}

Handle<v8::Value> clrFuncProxy(const v8::Arguments& args)
{
    DBG("clrFuncProxy");
    HandleScope scope;
    Handle<v8::External> correlator = Handle<v8::External>::Cast(args.Callee()->Get(v8::String::NewSymbol("_edgeContext")));
    ClrFuncWrap* wrap = (ClrFuncWrap*)(correlator->Value());
    ClrFunc* clrFunc = wrap->clrFunc;
    return scope.Close(clrFunc->Call(args[0], args[1]));
}

void clrFuncProxyNearDeath(v8::Persistent<v8::Value> object, void* parameters)
{
    DBG("clrFuncProxyNearDeath");
    ClrFuncWrap* wrap = (ClrFuncWrap*)parameters;
    object.Dispose();
    object.Clear();
    wrap->clrFunc = NULL;
    delete wrap;
}

Handle<v8::Function> ClrFunc::Initialize(MonoObject* func)
{
    DBG("ClrFunc::Initialize Func<object,Task<object>> wrapper");

    HandleScope scope;

    ClrFunc* app = new ClrFunc();
    app->func = mono_gchandle_new(func, FALSE);
    ClrFuncWrap* wrap = new ClrFuncWrap;
    wrap->clrFunc = app;
    v8::Persistent<v8::Function> funcProxy = v8::Persistent<v8::Function>::New(
        FunctionTemplate::New(clrFuncProxy)->GetFunction());
    funcProxy->Set(v8::String::NewSymbol("_edgeContext"), v8::External::New((void*)wrap));
    funcProxy.MakeWeak((void*)wrap, clrFuncProxyNearDeath);
    return scope.Close(funcProxy);
}

Handle<v8::Value> ClrFunc::Initialize(const v8::Arguments& args)
{
    DBG("ClrFunc::Initialize MethodInfo wrapper");

    HandleScope scope;
    Handle<v8::Object> options = args[0]->ToObject();
    Handle<v8::Function> result;

    Handle<v8::Value> jsassemblyFile = options->Get(String::NewSymbol("assemblyFile"));
    if (jsassemblyFile->IsString()) {
        // reference .NET code through pre-compiled CLR assembly 
        String::Utf8Value assemblyFile(jsassemblyFile);
        String::Utf8Value nativeTypeName(options->Get(String::NewSymbol("typeName")));
        String::Utf8Value nativeMethodName(options->Get(String::NewSymbol("methodName")));
        MonoException* exc = NULL;
        MonoObject* func = MonoEmbedding::GetClrFuncReflectionWrapFunc(*assemblyFile, *nativeTypeName, *nativeMethodName, &exc);
        if(exc)
            return scope.Close(throwV8Exception(exc));
        result = ClrFunc::Initialize(func);
    }
    else {
        //// reference .NET code throgh embedded source code that needs to be compiled
        String::Utf8Value compilerFile(options->Get(String::NewSymbol("compiler")));
        MonoAssembly *assembly = mono_domain_assembly_open (mono_domain_get(), *compilerFile);
        MonoClass* compilerClass = mono_class_from_name(mono_assembly_get_image(assembly), "", "EdgeCompiler");
        MonoObject* compilerInstance = mono_object_new(mono_domain_get(), compilerClass);
        MonoMethod* ctor = mono_class_get_method_from_name(compilerClass, ".ctor", 0);
        mono_runtime_invoke(ctor, compilerInstance, NULL, NULL);
        MonoMethod* compileFunc = mono_class_get_method_from_name(compilerClass, "CompileFunc", -1);
        MonoReflectionMethod* methodInfo = mono_method_get_object(mono_domain_get(),compileFunc, compilerClass);
        MonoClass* methodBase = mono_class_from_name(mono_get_corlib(), "System.Reflection", "MethodBase");
        MonoMethod* invoke = mono_class_get_method_from_name(methodBase, "Invoke", 2);
        if (compileFunc == NULL)
        {
            // exception
        }
        MonoException* exc = NULL;
        MonoObject* parameters = ClrFunc::MarshalV8ToCLR(NULL, options);
        MonoArray* methodInfoParams = mono_array_new(mono_domain_get(), mono_get_object_class(), 1);
        mono_array_setref(methodInfoParams, 0, parameters);
        void* params[2];
        params[0] = compilerInstance;
        params[1] = methodInfoParams;
        MonoObject* func = mono_runtime_invoke(invoke, methodInfo, params, (MonoObject**)&exc);
        if (exc)
            return scope.Close(throwV8Exception(exc));
        result = ClrFunc::Initialize(func);
    }

    return scope.Close(result);
}

Handle<v8::Value> ClrFunc::MarshalCLRToV8(MonoObject* netdata)
{
    HandleScope scope;
    Handle<v8::Value> jsdata;

    if (netdata == NULL)
    {
        return scope.Close(Undefined());
    }

    static MonoClass* guid_class;
    static MonoClass* decimal_class;
    static MonoClass* idictionary_class;
    static MonoClass* ienumerable_class;
    static MonoClass* func_class;
    if (!guid_class)
        guid_class = mono_class_from_name (mono_get_corlib(), "System", "Guid");
    if (!decimal_class)
        decimal_class = mono_class_from_name (mono_get_corlib(), "System", "Decimal");
    if (!idictionary_class)
        idictionary_class = mono_class_from_name (mono_get_corlib(), "System.Collections", "IDictionary");
    if (!ienumerable_class)
        ienumerable_class = mono_class_from_name (mono_get_corlib(), "System.Collections", "IEnumerable");

    //try 
    //{
    MonoClass* klass = mono_object_get_class(netdata);
    if (klass == mono_get_string_class())
    {
        jsdata = stringCLR2V8((MonoString*)netdata);
    }
    else if (klass == mono_get_char_class())
    {
        //jsdata = stringCLR2V8(((System::Char^)netdata)->ToString());
        ABORT_TODO();
    }
    else if (klass == mono_get_boolean_class())
    {
        jsdata = v8::Boolean::New((bool)*(bool*)mono_object_unbox(netdata));
    }
    else if (klass == guid_class)
    {
        static MonoMethod* method;
        if (!method)
            method = mono_class_get_method_from_name(guid_class, "ToString", 0);
        MonoString* str = (MonoString*)mono_runtime_invoke(method, netdata, NULL, NULL);
        jsdata = stringCLR2V8(str);
    }
    //    else if (type == System::DateTime::typeid)
    //    {
    //        jsdata = stringCLR2V8(netdata->ToString());
    //    }
    //    else if (type == System::DateTimeOffset::typeid)
    //    {
    //        jsdata = stringCLR2V8(netdata->ToString());
    //    }
    //    else if (type == System::Uri::typeid)
    //    {
    //        jsdata = stringCLR2V8(netdata->ToString());
    //    }
    else if (klass == mono_get_int32_class())
    {
        jsdata = v8::Integer::New(*(int32_t*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_int64_class())
    {
        ABORT_TODO();
        //jsdata = v8::Number::New(((System::IConvertible^)netdata)->ToDouble(nullptr));
    }
    else if (klass == mono_get_double_class())
    {
        jsdata = v8::Number::New(*(double*)mono_object_unbox(netdata));
    }
    else if (klass == mono_get_single_class())
    {
        jsdata = v8::Number::New(*(float*)mono_object_unbox(netdata));
    }
    else if (/*type->IsPrimitive ||*/ klass == decimal_class)
    {
        ABORT_TODO();
        /*System::IConvertible^ convertible = dynamic_cast<System::IConvertible^>(netdata);
        if (convertible != nullptr)
        {
        jsdata = stringCLR2V8(convertible->ToString());
        }
        else
        {
        jsdata = stringCLR2V8(netdata->ToString());
        }*/
    }
    else if (mono_class_is_enum(klass))
    {
        ABORT_TODO();
        //jsdata = stringCLR2V8(netdata->ToString());
    }
    else if (mono_class_get_rank(klass) > 0 && mono_class_get_element_class(klass) == mono_get_byte_class())
    {
        MonoArray* buffer = (MonoArray*)netdata;
        size_t length = mono_array_length(buffer);
        uint8_t* pinnedBuffer = mono_array_addr(buffer, uint8_t, 0);
        node::Buffer* slowBuffer = node::Buffer::New(length);
        memcpy(node::Buffer::Data(slowBuffer), pinnedBuffer, length);
        Handle<v8::Value> args[] = { 
            slowBuffer->handle_, 
            v8::Integer::New(length), 
            v8::Integer::New(0) 
        };
        jsdata = bufferConstructor->NewInstance(3, args);
    }
    else if (mono_class_is_assignable_from (idictionary_class, klass))
    {
        ABORT_TODO();
        //Handle<v8::Object> result = v8::Object::New();
        //for each (System::Collections::DictionaryEntry^ entry in (System::Collections::IDictionary^)netdata)
        //{
        //    if (dynamic_cast<System::String^>(entry->Key) != nullptr)
        //    result->Set(stringCLR2V8((System::String^)entry->Key), ClrFunc::MarshalCLRToV8(entry->Value));
        //}

        //jsdata = result;
    }
    else if (mono_class_is_assignable_from (ienumerable_class, klass))
    {
        Handle<v8::Array> result = v8::Array::New();
        MonoArray* values = MonoEmbedding::IEnumerableToArray(netdata);
        size_t length = mono_array_length(values);
        unsigned int i = 0;
        for (i = 0; i < length; i++)
        {
            MonoObject* value = mono_array_get(values, MonoObject*, i);
            result->Set(i, ClrFunc::MarshalCLRToV8(value));
        }

        jsdata = result;
    }
    else if (MonoEmbedding::GetFuncClass() == klass)
    {
        jsdata = ClrFunc::Initialize(netdata);
    }
    else
    {
        jsdata = ClrFunc::MarshalCLRObjectToV8(netdata);
    }
    //}
    //catch (System::Exception^ e)
    //{
    //    return scope.Close(throwV8Exception(e));
    //}
    //else
    //{
    //    ABORT_TODO();
    //}

    return scope.Close(jsdata);
}

Handle<v8::Value> ClrFunc::MarshalCLRObjectToV8(MonoObject* netdata)
{
    HandleScope scope;
    Handle<v8::Object> result = v8::Object::New();
    MonoClass* klass = mono_object_get_class(netdata);
    MonoClassField* field;
    MonoProperty* prop;
    void* iter = NULL;

    while (field = mono_class_get_fields(klass, &iter))
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
            v8::String::New(name), 
            ClrFunc::MarshalCLRToV8(value));
    }

    iter = NULL;
    while(prop = mono_class_get_properties(klass, &iter))
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

        // TODO check signature for no paramters


        if (enableScriptIgnoreAttribute)
        {
        //        if (property->IsDefined(System::Web::Script::Serialization::ScriptIgnoreAttribute::typeid, true))
        //        {
        //            continue;
        //        }

        //        System::Web::Script::Serialization::ScriptIgnoreAttribute^ attr =
        //            (System::Web::Script::Serialization::ScriptIgnoreAttribute^)System::Attribute::GetCustomAttribute(
        //            property, 
        //            System::Web::Script::Serialization::ScriptIgnoreAttribute::typeid,
        //            true);

        //        if (attr != nullptr && attr->ApplyToOverrides)
        //        {
        //            continue;
        //        }
        }

        const char* name = mono_property_get_name(prop);
        MonoException* exc = NULL;
        MonoObject* value = mono_runtime_invoke(getMethod, netdata, NULL, (MonoObject**)&exc);
        if (exc)
            return scope.Close(throwV8Exception(exc));
        result->Set(
            v8::String::New(name),
            ClrFunc::MarshalCLRToV8(value));
    }

    //System::Type^ type = netdata->GetType();

    //for each (FieldInfo^ field in type->GetFields(BindingFlags::Public | BindingFlags::Instance))
    //{
    //    result->Set(
    //        stringCLR2V8(field->Name), 
    //        ClrFunc::MarshalCLRToV8(field->GetValue(netdata)));
    //}

    //for each (PropertyInfo^ property in type->GetProperties(BindingFlags::GetProperty | BindingFlags::Public | BindingFlags::Instance))
    //{
    //    if (enableScriptIgnoreAttribute)
    //    {
    //        if (property->IsDefined(System::Web::Script::Serialization::ScriptIgnoreAttribute::typeid, true))
    //        {
    //            continue;
    //        }

    //        System::Web::Script::Serialization::ScriptIgnoreAttribute^ attr =
    //            (System::Web::Script::Serialization::ScriptIgnoreAttribute^)System::Attribute::GetCustomAttribute(
    //            property, 
    //            System::Web::Script::Serialization::ScriptIgnoreAttribute::typeid,
    //            true);

    //        if (attr != nullptr && attr->ApplyToOverrides)
    //        {
    //            continue;
    //        }
    //    }

    //    MethodInfo^ getMethod = property->GetGetMethod();
    //    if (getMethod != nullptr && getMethod->GetParameters()->Length <= 0)
    //    {
    //        result->Set(
    //            stringCLR2V8(property->Name), 
    //            ClrFunc::MarshalCLRToV8(getMethod->Invoke(netdata, nullptr)));
    //    }
    //}

    return scope.Close(result);
}

MonoObject* ClrFunc::MarshalV8ToCLR(ClrFuncInvokeContext* context, Handle<v8::Value> jsdata)
{
    HandleScope scope;

    if (jsdata->IsFunction() && context != NULL)
    {
        NodejsFunc* functionContext = new NodejsFunc(context, Handle<v8::Function>::Cast(jsdata));
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
            mono_array_setref(netarray, i, ClrFunc::MarshalV8ToCLR(context, jsarray->Get(i)));
        }

        return (MonoObject*)netarray;
    }
    else if (jsdata->IsObject()) 
    {
        MonoObject* netobject = MonoEmbedding::CreateDictionary();
        Handle<v8::Object> jsobject = Handle<v8::Object>::Cast(jsdata);
        Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
            String::Utf8Value utf8name(name);
            Dictionary::Add(netobject, *utf8name, ClrFunc::MarshalV8ToCLR(context, jsobject->Get(name)));
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
    HandleScope scope;
    MonoException* exc = NULL;

    ClrFuncInvokeContext* c = new ClrFuncInvokeContext(callback);
    c->Payload(ClrFunc::MarshalV8ToCLR(c, payload));

    MonoObject* func = mono_gchandle_get_target(this->func);
    void* params[1];
    params[0] = c->Payload();
    MonoMethod* invoke = mono_class_get_method_from_name(mono_object_get_class(func), "Invoke", -1);
    MonoObject* task = mono_runtime_invoke(invoke, func, params, (MonoObject**)&exc);
    if (exc)
        return scope.Close(throwV8Exception(exc));

    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(task), "IsCompleted");
    MonoObject* isCompletedObject = mono_property_get_value(prop, task, NULL, (MonoObject**)&exc);
    if (exc)
        return scope.Close(throwV8Exception(exc));
    bool isCompleted = *(bool*)mono_object_unbox(isCompletedObject);
    if(isCompleted)
    {
        // Completed synchronously. Return a value or invoke callback based on call pattern.
        c->Task(task);
        return scope.Close(c->CompleteOnV8Thread(true));
    }
    else if (c->Sync())
    {
        return scope.Close(throwV8Exception(mono_get_exception_invalid_operation("The JavaScript function was called synchronously "
            "but the underlying CLR function returned without completing the Task. Call the "
            "JavaScript function asynchronously.")));
    }
    else
    {
        MonoEmbedding::ContinueTask(task, c->GetMonoObject());
    }

    return scope.Close(Undefined());    
}

// vim: ts=4 sw=4 et: 
