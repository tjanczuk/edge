#include "edge_common.h"

v8::Local<Value> throwV8Exception(v8::Local<Value> exception)
{
    Nan::EscapableHandleScope scope;
    Nan::ThrowError(exception);
    return scope.Escape(exception);
}

v8::Local<Value> throwV8Exception(const char* format, ...)
{
	va_list args;
	va_start(args, format);

	size_t size = vsnprintf(NULL, 0, format, args);
	char* message = new char[size + 1];

	vsnprintf(message, size + 1, format, args);

	Nan::EscapableHandleScope scope;

	v8::Local<v8::Object> exception = Nan::New<v8::Object>();
	exception->SetPrototype(v8::Exception::Error(Nan::New<v8::String>(message).ToLocalChecked()));

	v8::Local<v8::Value> exceptionValue = exception;
	Nan::ThrowError(exceptionValue);

	return scope.Escape(exception);
}
