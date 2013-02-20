#include <node.h>
#include <v8.h>

using namespace v8;

Handle<Value> initializeOwinApp(const Arguments& args) 
{
    HandleScope scope;

    System::Console::WriteLine("In initializeOwinApp");

    return Undefined();
}

Handle<Value> callOwinApp(const Arguments& args) 
{
    HandleScope scope;

    System::Console::WriteLine("In callOwinApp");

    return Undefined();
}

void init(Handle<Object> target) 
{
    NODE_SET_METHOD(target, "initializeOwinApp", initializeOwinApp);
    NODE_SET_METHOD(target, "callOwinApp", callOwinApp);
}

NODE_MODULE(owin, init);
