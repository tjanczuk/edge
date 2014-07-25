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

bool IsGenericList(System::Type^ type)
{
    if (type == nullptr) {
		return false;
    }
	array<System::Type^>^ interfaces = type->GetInterfaces();
	for(int i=0; i < interfaces->Length; i++)
	{
		System::Type^ x = interfaces[i];
		
		if (System::Collections::ICollection::typeid == x) {
			return true;
		}


    }
    return false;
}

Handle<Value> mapItem(System::Object^ value)
{
	if (value->GetType() == System::String::typeid)
	{
		return stringCLR2V8((System::String^)value);
	}
	else if (value->GetType() == System::Guid::typeid)
	{
		return stringCLR2V8(((System::Guid^)value)->ToString());
	}
	else if (value->GetType() == System::Int32::typeid)
	{
		return v8::Integer::New((System::Int32)value);
	}
	else if (value->GetType() == System::Byte::typeid)
	{
		return v8::Integer::New((System::Byte)value);
	}
	else if (value->GetType() == System::Int16::typeid)
	{
		return v8::Integer::New((System::Int16)value);
	}
	else if (value->GetType() == System::Single::typeid)
	{
		return v8::Number::New((System::Single)value);
	}
	else if (value->GetType() == System::Double::typeid)
	{
		return v8::Number::New((System::Double)value);
	}
	else if (value->GetType() == System::Boolean::typeid)
	{
		return v8::BooleanObject::New((System::Boolean)value);
	}
	else if (System::Exception::typeid->IsAssignableFrom(value->GetType()))
	{
		return exceptionCLR2stringV8((System::Exception^)value);
	}/*
	else if (IsGenericList(value->GetType()))
	{
		System::Collections::Generic::IList sourceValues;
		sourceValues = (System::Collections::Generic::IList<System::Object^>^)value;
		Local< Array > targetValues = Array::New(sourceValues->Count);
		
		for(int j = 0; j < sourceValues->Count; j++)
		{
			System::Object^ sourceValue = sourceValues[j];
			Handle<Value> targetValue = mapItem(sourceValue);
			
			targetValues->Set(j, targetValue);
		}
			
		return targetValues;

	}*/	
	else
	{
		// At least show the type
		return stringCLR2V8(value->GetType()->FullName);
	}
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
		
		for(int i = 0; i < properties->Length; i++)
		{
			PropertyInfo^ pi = properties[i];
			Handle<v8::String> propertyName = stringCLR2V8(pi->Name);
				
			System::Object^ value = pi->GetValue(exception);
			if (value != nullptr)
			{
				obj->Set(propertyName, mapItem(value));
			}
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
