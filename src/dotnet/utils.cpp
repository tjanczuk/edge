#include "edge.h"

Handle<v8::String> stringCLR2V8(System::String^ text)
{
    HandleScope scope;
    if (text->Length > 0)
    {
        array<unsigned char>^ utf8 = System::Text::Encoding::UTF8->GetBytes(text);
        pin_ptr<unsigned char> ch = &utf8[0];
        return scope.Close(v8::String::New((char*)ch));
    }
    else
    {
        return scope.Close(v8::String::Empty());
    }
}

System::String^ stringV82CLR(Handle<v8::String> text)
{
    HandleScope scope;
    String::Utf8Value utf8text(text);
    if (*utf8text)
    {
        return gcnew System::String(
            *utf8text, 0, utf8text.length(), System::Text::Encoding::UTF8);
    }
    else
    {
        return System::String::Empty;
    }
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
