#include <dlfcn.h>
#include <limits.h>
#include <libgen.h>
#include "edge.h"

#include "mono/metadata/assembly.h"
#include "mono/metadata/mono-config.h"
#include "mono/jit/jit.h"


MonoAssembly* MonoEmbedding::assembly = NULL;

void MonoEmbedding::Initialize()
{
    // Construct the absolute file path to MonoEmbedding.exe assuming
    // it is located next to edge.node
    Dl_info dlinfo;
    char fullPath[PATH_MAX];
    dladdr((void*)&MonoEmbedding::Initialize, &dlinfo);
    strcpy(fullPath, dlinfo.dli_fname);
    strcpy(fullPath, dirname(fullPath));
    strcat(fullPath, "/MonoEmbedding.exe");

    mono_config_parse (NULL);
    mono_jit_init (fullPath);
    assembly = mono_domain_assembly_open (mono_domain_get(), fullPath);
    MonoClass* klass = mono_class_from_name(mono_assembly_get_image(assembly), "", "MonoEmbedding");
    MonoMethod* main = mono_class_get_method_from_name(klass, "Main", -1);
    MonoException* exc;
    MonoArray* args = mono_array_new(mono_domain_get(), mono_get_string_class(), 0);
    mono_runtime_exec_main(main, args, (MonoObject**)&exc);

    mono_add_internal_call("ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronousICall", (const void*)&ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous); 
    mono_add_internal_call("ClrFuncInvokeContext::CompleteOnCLRThreadICall", (const void*)&ClrFuncInvokeContext::CompleteOnCLRThread); 
    mono_add_internal_call("NodejsFuncInvokeContext::CallFuncOnV8ThreadInternal", (const void*)&NodejsFuncInvokeContext::CallFuncOnV8Thread); 
    mono_add_internal_call("NodejsFunc::ExecuteActionOnV8Thread", (const void*)&NodejsFunc::ExecuteActionOnV8Thread); 
    mono_add_internal_call("NodejsFunc::Release", (const void*)&NodejsFunc::Release); 
}

void MonoEmbedding::NormalizeException(MonoException** e) 
{
    static MonoMethod* method;

    if (!method)
    {
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "NormalizeException", -1);
    }

    void *params[] = { *e };
    MonoException *exc = NULL;
    MonoException *en = (MonoException*)mono_runtime_invoke(method, NULL, params, (MonoObject**)exc);
    if (NULL == exc)
    {
        *e = en;
    }
}

MonoAssembly* MonoEmbedding::GetAssembly()
{
    return assembly;
}

MonoImage* MonoEmbedding::GetImage()
{
    return mono_assembly_get_image(assembly);
}

MonoClass* MonoEmbedding::GetClass()
{
    static MonoClass* klass;

    if (!klass)
        klass = mono_class_from_name(GetImage(), "", "MonoEmbedding");

    return klass;
}

MonoObject* MonoEmbedding::GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName, MonoException ** exc)
{
    static MonoMethod* method;
    void* params[3];

    if (!method)
    {
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "GetFunc", 3);
    }

    params[0] = mono_string_new(mono_domain_get(), assemblyFile);
    params[1] = mono_string_new(mono_domain_get(), typeName);
    params[2] = mono_string_new(mono_domain_get(), methodName);
    MonoObject* action = mono_runtime_invoke(method, NULL, params, (MonoObject**)exc);

    return action;
}

MonoClass* MonoEmbedding::GetIDictionaryStringObjectClass(MonoException** exc) 
{
    static MonoClass* klass;
    *exc = NULL;

    if (!klass)
    {
        MonoMethod* method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "GetIDictionaryStringObjectType", -1);
        MonoReflectionType* typeObject = (MonoReflectionType*)mono_runtime_invoke(method, NULL, NULL, (MonoObject**)exc);
        if(*exc)
            return NULL;
        
        MonoType* type = mono_reflection_type_get_type(typeObject);
        klass = mono_class_from_mono_type(type);
    }

    return klass;    
}

MonoClass* MonoEmbedding::GetUriClass(MonoException** exc) 
{
    static MonoClass* klass;
    *exc = NULL;

    if (!klass)
    {
        MonoMethod* method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "GetUriType", -1);
        MonoReflectionType* typeObject = (MonoReflectionType*)mono_runtime_invoke(method, NULL, NULL, (MonoObject**)exc);
        if(*exc)
            return NULL;

        MonoType* type = mono_reflection_type_get_type(typeObject);
        klass = mono_class_from_mono_type(type);
    }

    return klass;    
}

MonoObject* MonoEmbedding::CreateDateTime(double ticks) 
{
    static MonoMethod* method;
    MonoException* exc = NULL;

    if (!method)
    {
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "CreateDateTime", -1);
    }

    void* args[] = { &ticks };
    MonoObject* ret = mono_runtime_invoke(method, NULL, args, (MonoObject**)&exc);

    return ret;
}

MonoObject* MonoEmbedding::CreateExpandoObject()
{
    static MonoMethod* method;
    MonoException* exc = NULL;

    if (!method)
    {
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "CreateExpandoObject", -1);
    }
    MonoObject* dictionary = mono_runtime_invoke(method, NULL, NULL, (MonoObject**)&exc);

    return dictionary;
}

MonoClass* MonoEmbedding::GetFuncClass()
{
    static MonoMethod* method;
    MonoException* exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "GetFuncType", -1);

    MonoReflectionType* typeObject = (MonoReflectionType*)mono_runtime_invoke(method, NULL, NULL, (MonoObject**)&exc);
    MonoType* type = mono_reflection_type_get_type(typeObject);
    MonoClass* klass = mono_class_from_mono_type(type);
    return klass;
}

MonoArray* MonoEmbedding::IEnumerableToArray(MonoObject* ienumerable, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "IEnumerableToArray", -1);

    void* args[1];
    args[0] = ienumerable;
    MonoArray* values = (MonoArray*)mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
    if(*exc)
        return NULL;
    return values;
}

MonoArray* MonoEmbedding::IDictionaryToFlatArray(MonoObject* dictionary, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "IDictionaryToFlatArray", -1);

    void* args[1];
    args[0] = dictionary;
    MonoArray* values = (MonoArray*)mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
    if(*exc)
        return NULL;
    return values;
}

void MonoEmbedding::ContinueTask(MonoObject* task, MonoObject* state, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "ContinueTask", -1);

    void* args[2];
    args[0] = task;
    args[1] = state;
    mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
}

double MonoEmbedding::GetDateValue(MonoObject* dt, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "GetDateValue", -1);

    void* args[] = { mono_object_unbox(dt) };

    MonoObject* obj = mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
    if (*exc)
        return 0.0;
    return *(double*)mono_object_unbox(obj);
}

double MonoEmbedding::Int64ToDouble(MonoObject* i64, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "Int64ToDouble", -1);

    void* args[] = { mono_object_unbox(i64) };

    MonoObject* obj = mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
    if (*exc)
        return 0.0;
    return *(double*)mono_object_unbox(obj);
}

MonoString* MonoEmbedding::ToString(MonoObject* o, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "ObjectToString", -1);

    void* args[] = { o };

    return (MonoString*)mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
}

MonoString* MonoEmbedding::TryConvertPrimitiveOrDecimal(MonoObject* obj, MonoException** exc)
{
    static MonoMethod* method;
    *exc = NULL;

    if (!method)
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "TryConvertPrimitiveOrDecimal", -1);

    void* args[] = { obj };

    return (MonoString*)mono_runtime_invoke(method, NULL, args, (MonoObject**)exc);
}

// vim: ts=4 sw=4 et: 
