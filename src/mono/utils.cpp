#include "edge.h"

Handle<v8::String> stringCLR2V8(MonoString* text)
{
    HandleScope scope;
    return scope.Close(v8::String::New(
        (uint16_t*)mono_string_chars(text),
        mono_string_length(text)));  
}

MonoString* stringV82CLR(Handle<v8::String> text)
{
    HandleScope scope;
    String::Utf8Value utf8text(text);
    return mono_string_new(mono_domain_get(), *utf8text);    
}

MonoString* exceptionV82stringCLR(Handle<v8::Value> exception)
{
    HandleScope scope;
    if (exception->IsObject())
    {
        Handle<Value> stack = exception->ToObject()->Get(v8::String::NewSymbol("stack"));
        if (stack->IsString())
        {
            return stringV82CLR(stack->ToString());
        }
    }

    return stringV82CLR(Handle<v8::String>::Cast(exception));
}

Handle<Value> throwV8Exception(Handle<Value> exception)
{
    HandleScope scope;
    return scope.Close(ThrowException(exception));
}

// vim: ts=4 sw=4 et: 
