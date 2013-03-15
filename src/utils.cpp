#include "edge.h"

Handle<v8::String> stringCLR2V8(System::String^ text)
{
    HandleScope scope;
    pin_ptr<const wchar_t> message = PtrToStringChars(text);
    return scope.Close(v8::String::New((uint16_t*)message));  
}

System::String^ stringV82CLR(Handle<v8::String> text)
{
    HandleScope scope;
    String::Utf8Value utf8text(text);
    return gcnew System::String(*utf8text);    
}

System::String^ exceptionV82stringCLR(Handle<v8::Value> exception)
{
    HandleScope scope;
    if (exception->IsObject())
    {
        Handle<Value> stack = exception->ToObject()->Get(v8::String::NewSymbol("stack"));
        if (stack->IsString())
        {
            return gcnew System::String(stringV82CLR(stack->ToString()));
        }
    }

    return gcnew System::String(stringV82CLR(Handle<v8::String>::Cast(exception)));
}

Handle<String> exceptionCLR2stringV8(System::Exception^ exception)
{
    HandleScope scope;
    if (exception == nullptr)
    {
        return scope.Close(v8::String::New("Unrecognized exception thrown by CLR."));
    }
    else
    {
        return scope.Close(stringCLR2V8(exception->ToString()));
    }
}

Handle<Value> throwV8Exception(System::Exception^ exception)
{
    HandleScope scope;
    return scope.Close(ThrowException(exceptionCLR2stringV8(exception)));
}
