#include "edge.h"

Handle<v8::String> stringCLR2V8(MonoString* text)
{
    NanEscapableScope();
    return NanEscapeScope(NanNew<v8::String>(
        (uint16_t*)mono_string_chars(text),
        mono_string_length(text)));  
}

MonoString* stringV82CLR(Handle<v8::String> text)
{
    NanScope();
    String::Utf8Value utf8text(text);
    return mono_string_new(mono_domain_get(), *utf8text);    
}

MonoString* exceptionV82stringCLR(Handle<v8::Value> exception)
{
    NanScope();
    if (exception->IsObject())
    {
        Handle<Value> stack = exception->ToObject()->Get(NanNew<v8::String>("stack"));
        if (stack->IsString())
        {
            return stringV82CLR(stack->ToString());
        }
    }

    return stringV82CLR(Handle<v8::String>::Cast(exception));
}

Handle<Value> throwV8Exception(Handle<Value> exception)
{
    NanEscapableScope();
    NanThrowError(exception);
    return NanEscapeScope(exception);
}

// vim: ts=4 sw=4 et: 
