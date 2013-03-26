#include "edge.h"

EdgeJavaScriptConverter::EdgeJavaScriptConverter()
{
	this->Objects = gcnew List<System::Object^>();
}

System::Object^ EdgeJavaScriptConverter::Deserialize(
        IDictionary<System::String^, System::Object^>^ dictionary, 
        System::Type^ type, 
        JavaScriptSerializer^ serializer)
{
	throw gcnew System::NotImplementedException();
}

IDictionary<System::String^, System::Object^>^ EdgeJavaScriptConverter::Serialize(
        System::Object^ obj, 
        JavaScriptSerializer^ serializer)
{
	int edgeId = this->Objects->Count;
	this->Objects->Add(obj);
	Dictionary<System::String^,System::Object^>^ result = gcnew Dictionary<System::String^,System::Object^>();
	result->Add("_edge_id", edgeId);
	return result;
} 

Handle<v8::Value> EdgeJavaScriptConverter::FixupResult(Handle<v8::Value> data)
{
	HandleScope scope;

	if (data->IsArray())
	{
        Handle<v8::Array> jsarray = Handle<v8::Array>::Cast(data);
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            Handle<v8::Value> value = jsarray->Get(i);
            jsarray->Set(i, this->FixupResult(value));
        }
	}
	else if (data->IsObject())
	{
		Handle<v8::Object> jsobject = data->ToObject();
    	Handle<v8::Value> edgeId = jsobject->Get(v8::String::NewSymbol("_edge_id"));
    	if (edgeId->IsInt32())
    	{
    		System::Object^ obj = this->Objects[edgeId->Int32Value()];
    		if (obj->GetType() == cli::array<byte>::typeid)
    		{
    			cli::array<byte>^ buffer = (cli::array<byte>^)obj;
		        pin_ptr<unsigned char> pinnedBuffer = &buffer[0];
		        node::Buffer* slowBuffer = node::Buffer::New(buffer->Length);
		        memcpy(node::Buffer::Data(slowBuffer), pinnedBuffer, buffer->Length);
		        Handle<v8::Value> args[] = { 
		            slowBuffer->handle_, 
		            v8::Integer::New(buffer->Length), 
		            v8::Integer::New(0) 
		        };
		        Handle<v8::Object> fastBuffer = bufferConstructor->NewInstance(3, args);    
		        return scope.Close(fastBuffer);		
		    }
		    else
		    {
		    	// Func<object,Task<object>>
		    	System::Func<System::Object^,Task<System::Object^>^>^ func = 
		    		(System::Func<System::Object^,Task<System::Object^>^>^)obj;
		    	Handle<v8::Function> funcProxy = ClrFunc::Initialize(func);
		    	return scope.Close(funcProxy);
		    }
    	}
    	else 
    	{
	        Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();
	        for (unsigned int i = 0; i < propertyNames->Length(); i++)
	        {
	            Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
	            jsobject->Set(name, this->FixupResult(jsobject->Get(name)));
	        }
    	}
	}

	return scope.Close(data);
}
