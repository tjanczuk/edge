#include "edge.h"

void ClrActionContext::ActionCallback(void* data)
{
    static MonoMethod* actionInvoke;
    if (!actionInvoke)
    {
        MonoClass* actionClass = mono_class_from_name(mono_get_corlib(), "System", "Action");
        actionInvoke = mono_class_get_method_from_name(actionClass, "Invoke", -1);
    }

    ClrActionContext* context = (ClrActionContext*)data;
    MonoObject* action = mono_gchandle_get_target(context->action);
    MonoException* exc = NULL;
    mono_runtime_invoke(actionInvoke, action, NULL, (MonoObject**)&exc);
    mono_gchandle_free(context->action);
    delete context;
    if (exc)
        ABORT_TODO();
}
