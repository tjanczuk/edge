#include "edge.h"

Local<v8::String> stringCLR2V8(System::String^ text)
{
	Isolate* isolate = Isolate::GetCurrent();
    EscapableHandleScope scope(isolate);
    if (text->Length > 0)
    {
        array<unsigned char>^ utf8 = System::Text::Encoding::UTF8->GetBytes(text);
        pin_ptr<unsigned char> ch = &utf8[0];
		return scope.Escape(v8::String::NewFromUtf8(isolate, (char*)ch));
    }
    else
    {
        return scope.Escape(v8::String::Empty(isolate));
    }
}

System::String^ stringV82CLR(Handle<v8::String> text)
{
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
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
	Isolate* isolate = Isolate::GetCurrent();
	HandleScope scope(isolate);
	if (exception->IsObject())
    {
		//... TJF -- this is an out and out guess!
        Handle<Value> stack = exception->ToObject()->Get(v8::String::NewFromUtf8(isolate, "stack", v8::String::kInternalizedString ));
        if (stack->IsString())
        {
            return gcnew System::String(stringV82CLR(stack->ToString()));
        }
    }

    return gcnew System::String(stringV82CLR(Handle<v8::String>::Cast(exception)));
}

Local<Value> throwV8Exception(Handle<Value> exception)
{
	Isolate* isolate = Isolate::GetCurrent();
	EscapableHandleScope scope(isolate);
    return scope.Escape(isolate->ThrowException(exception));
}
