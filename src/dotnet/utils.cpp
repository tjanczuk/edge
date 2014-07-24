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

Handle<Value> exceptionCLR2stringV8(System::Exception^ exception)
{
    HandleScope scope;
    if (exception == nullptr)
    {
        return scope.Close(v8::String::New("Unrecognized exception thrown by CLR."));
    }
    else
    {
		Handle<v8::String> ToString = stringCLR2V8(exception->ToString());
		Handle<v8::String> Message = stringCLR2V8(exception->Message);
		
		Local<Object> obj = Object::New();
		
		//Construct an error that is just used for the prototype - not verify efficient
		//but 'typeof Error' should work in JavaScript
		obj->SetPrototype(Exception::Error(Message));
		obj->Set(String::NewSymbol("message"), Message);
		
		//Recording the actual type - 'name' seems to be the common used property
		Handle<v8::String> type = stringCLR2V8(exception->GetType()->FullName);
		obj->Set(String::NewSymbol("name"), type);
		
		//Walk over all attributes and copy them
		array<PropertyInfo^>^ properties = exception->GetType()->GetProperties();
		
		for(int i = 0; i < properties->Length; i++){

			PropertyInfo^ pi = properties[i];
			Handle<v8::String> propertyName = stringCLR2V8(pi->Name);
				
			System::Object^ value = pi->GetValue(exception);
			if (value != nullptr){
				System::String^ propertyType = value->GetType()->FullName;
				Handle<v8::String> propertyValue;
				//There should be a better approach:
				if (propertyType->Equals("System.String")) {
					propertyValue = stringCLR2V8((System::String^)value);
				} else {
					propertyValue = stringCLR2V8(propertyType);
				}
				
				obj->Set(propertyName, propertyValue);
			}
			//obj->Set(String::NewSymbol((char*)(void*)System::Runtime::InteropServices::Marshal::StringToHGlobalAnsi(pi->Name)), Message);
		}
		//Record the whole toString for those who are interessted

		return scope.Close(obj);
    }
}

Handle<Value> throwV8Exception(System::Exception^ exception)
{
    HandleScope scope;
    return scope.Close(ThrowException(exceptionCLR2stringV8(exception)));
}
