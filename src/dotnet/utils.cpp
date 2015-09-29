#include "edge.h"

v8::Local<v8::String> stringCLR2V8(System::String^ text)
{
    Nan::EscapableHandleScope scope;
    if (text->Length > 0)
    {
        array<unsigned char>^ utf8 = System::Text::Encoding::UTF8->GetBytes(text);
        pin_ptr<unsigned char> ch = &utf8[0];
        return scope.Escape(Nan::New<v8::String>((char*)ch).ToLocalChecked());
    }
    else
    {
        return scope.Escape(Nan::New<v8::String>("").ToLocalChecked());
    }
}

System::String^ stringV82CLR(v8::Local<v8::String> text)
{
    Nan::HandleScope scope;
    v8::String::Utf8Value utf8text(text);
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

System::String^ exceptionV82stringCLR(v8::Local<v8::Value> exception)
{
    Nan::HandleScope scope;
    if (exception->IsObject())
    {
        v8::Local<v8::Value> stack = exception->ToObject()->Get(Nan::New<v8::String>("stack").ToLocalChecked());
        if (stack->IsString())
        {
            return gcnew System::String(stringV82CLR(stack->ToString()));
        }
    }

    return gcnew System::String(stringV82CLR(v8::Local<v8::String>::Cast(exception)));
}
