#include "edge.h"
#include "mono/metadata/class.h"
#include "mono/metadata/object.h"

Task::TaskStatus Task::Status(MonoObject* _this)
{
    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(_this), "Status");
    MonoObject* statusBoxed = mono_property_get_value(prop, _this, NULL, NULL);
    Task::TaskStatus status = *(Task::TaskStatus*)mono_object_unbox(statusBoxed);
    return status;
}

MonoException* Task::Exception(MonoObject* _this)
{
    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(_this), "Exception");
    MonoObject* exception = mono_property_get_value(prop, _this, NULL, NULL);
    return (MonoException*)exception;
}

MonoObject* Task::Result(MonoObject* _this)
{
    MonoProperty* prop = mono_class_get_property_from_name(mono_object_get_class(_this), "Result");
    MonoObject* result = mono_property_get_value(prop, _this, NULL, NULL);
    return result;
}


void Dictionary::Add(MonoObject* _this, const char* name, MonoObject* value)
{
    static MonoMethod* add;
    
    if(!add) {
        add = mono_class_get_method_from_name(mono_object_get_class(_this), 
            "System.Collections.Generic.IDictionary<string,object>.Add", -1);
    }

    void* params[2];
    params[0] = mono_string_new(mono_domain_get(), name);
    params[1] = value;

    mono_runtime_invoke(add, _this, params, NULL);
}
 
// vim: ts=4 sw=4 et: 
