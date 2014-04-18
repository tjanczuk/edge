using System;
using System.Threading.Tasks;
using System.Runtime.CompilerServices;

class ClrFuncInvokeContext
{
#pragma warning disable 649 // assigned from clrfuncinvokecontext.cpp through embedding APIs
    IntPtr native;
#pragma warning disable 169 // assigned from clrfuncinvokecontext.cpp through embedding APIs
    Object Payload;
    Task<Object> Task;
    bool Sync;
#pragma warning restore 169
#pragma warning restore 649

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void CompleteOnCLRThreadICall(IntPtr ptr, Task<object> task);

    internal void CompleteOnCLRThread(Task<object> task)
    {
        CompleteOnCLRThreadICall(native, task);
    }

    [MethodImplAttribute(MethodImplOptions.InternalCall)]
    static extern void CompleteOnV8ThreadAsynchronousICall(IntPtr ptr);

    void CompleteOnV8ThreadAsynchronous()
    {
        CompleteOnV8ThreadAsynchronousICall(native);
    }

    public Action GetCompleteOnV8ThreadAsynchronousAction()
    {
        return new Action(CompleteOnV8ThreadAsynchronous);
    }
};
