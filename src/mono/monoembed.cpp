#include "edge.h"

#include "mono/metadata/assembly.h"
#include "mono/jit/jit.h"

MonoAssembly* MonoEmbedding::assembly = NULL;

void MonoEmbedding::Initialize()
{
    const char* fullPath = "C:\\Users\\Jonathan\\Development\\edge\\src\\mono\\MonoEmbedding.exe";
    mono_jit_init (fullPath);
    assembly = mono_domain_assembly_open (mono_domain_get(), fullPath);
    MonoClass* klass = mono_class_from_name(mono_assembly_get_image(assembly), "", "MonoEmbedding");
    MonoMethod* main = mono_class_get_method_from_name(klass, "Main", -1);
    MonoException* exc;
    MonoArray* args = mono_array_new(mono_domain_get(), mono_get_string_class(), 0);
    int ret = mono_runtime_exec_main(main, args, (MonoObject**)&exc);

    mono_add_internal_call("ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronousICall", (const void*)&ClrFuncInvokeContext::CompleteOnV8ThreadAsynchronous); 
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

MonoObject* MonoEmbedding::CreateDictionary()
{
    static MonoMethod* method;
    MonoException* exc = NULL;

    if (!method)
    {
        method = mono_class_get_method_from_name(MonoEmbedding::GetClass(), "CreateDictionary", -1);
    }
    MonoObject* dictionary = mono_runtime_invoke(method, NULL, NULL, (MonoObject**)&exc);

    return dictionary;
}

// vim: ts=4 sw=4 et: 
