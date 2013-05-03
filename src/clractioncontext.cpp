#include "edge.h"

void ClrActionContext::ActionCallback(void* data)
{
    ClrActionContext* context = (ClrActionContext*)data;
    System::Action^ action = context->action;
    delete context;
    action();
}
