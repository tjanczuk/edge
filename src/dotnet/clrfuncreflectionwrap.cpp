#include "edge.h"

ClrFuncReflectionWrap::ClrFuncReflectionWrap()
{
    // empty
}

ClrFuncReflectionWrap^ ClrFuncReflectionWrap::Create(
    Assembly^ assembly, System::String^ typeName, System::String^ methodName)
{
    System::Type^ startupType = assembly->GetType(typeName, true, true);
    ClrFuncReflectionWrap^ wrap = gcnew ClrFuncReflectionWrap();
    wrap->instance = System::Activator::CreateInstance(startupType, false);
    wrap->invokeMethod = startupType->GetMethod(methodName, BindingFlags::Instance | BindingFlags::Public);
    if (wrap->invokeMethod == nullptr) 
    {
        throw gcnew System::InvalidOperationException(
            "Unable to access the CLR method to wrap through reflection. Make sure it is a public instance method.");
    }
    
    return wrap;
}

Task<System::Object^>^ ClrFuncReflectionWrap::Call(System::Object^ payload)
{
    return (Task<System::Object^>^)this->invokeMethod->Invoke(
                this->instance, gcnew array<System::Object^> { payload });
}
