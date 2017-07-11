#ifndef __EDGE_COMMON_H
#define __EDGE_COMMON_H

#include <v8.h>
#include <node.h>
#include <node_buffer.h>
#include <uv.h>
#include <nan.h>
#include <stdarg.h>
#include <stdio.h>

using namespace v8;

// From http://stackoverflow.com/questions/5919996/how-to-detect-reliably-mac-os-x-ios-linux-windows-in-c-preprocessor
#ifdef _WIN64
   // Windows 64
#define EDGE_PLATFORM_WINDOWS 1
#elif _WIN32
   // Windows 32
#define EDGE_PLATFORM_WINDOWS 1
#elif __APPLE__
   // OSX
#define EDGE_PLATFORM_APPLE 1
#elif __linux
    // linux
#define EDGE_PLATFORM_NIX 1
#elif __unix // all unices not caught above
    // Unix
#define EDGE_PLATFORM_NIX 1
#elif __posix
    // POSIX
#define EDGE_PLATFORM_NIX 1
#endif

#ifndef EDGE_PLATFORM_WINDOWS
#include <stdlib.h>
#include <string.h>
#define __cdecl
#endif

#ifndef EDGE_PLATFORM_WINDOWS
#ifdef FALSE
#undef FALSE
#endif
#define FALSE 0
#ifdef TRUE
#undef TRUE
#endif
#define TRUE  1
typedef int BOOL;
#endif

#ifdef EDGE_PLATFORM_WINDOWS
#define ABORT_TODO() do { printf("%s (%d): %s\n", __FILE__, __LINE__, __FUNCTION__); abort(); } while (0)
#elif EDGE_PLATFORM_APPLE
#define ABORT_TODO() do { printf("%s (%d): %s\n", __FILE__, __LINE__, __func__); abort(); } while (0)
#else
#define ABORT_TODO() do { printf("%s (%d): %s\n", __FILE__, __LINE__, __func__); exit(1); } while (0)
#endif

extern BOOL debugMode;
extern BOOL enableScriptIgnoreAttribute;
extern BOOL enableMarshalEnumAsInt;

#define DBG(...) if (debugMode) { printf(__VA_ARGS__); printf("\n"); }

typedef void (*uv_async_edge_cb)(void* data);

typedef struct uv_edge_async_s {
    uv_async_t uv_async;
    uv_async_edge_cb action;
    void* data;
    bool singleton;
} uv_edge_async_t;

class V8SynchronizationContext {
private:

    static unsigned long v8ThreadId;
    static unsigned long GetCurrentThreadId();

public:

    // The node process will not exit until ExecuteAction or CancelAction had been called for all actions
    // registered by calling RegisterAction on V8 thread. Actions registered by calling RegisterAction
    // on CLR thread do not prevent the process from exiting.
    // Calls from JavaScript to .NET always call RegisterAction on V8 thread before invoking .NET code.
    // Calls from .NET to JavaScript call RegisterAction either on CLR or V8 thread, depending on
    // whether .NET code executes synchronously on V8 thread it strarted running on.
    // This means that if any call of a .NET function from JavaScript is in progress, the process won't exit.
    // It also means that existence of .NET proxies to JavaScript functions in the CLR does not prevent the
    // process from exiting.
    // In this model, JavaScript owns the lifetime of the process.

    static uv_edge_async_t* uv_edge_async;
    static uv_sem_t* funcWaitHandle;

    static void Initialize();
    static uv_edge_async_t* RegisterAction(uv_async_edge_cb action, void* data);
    static void ExecuteAction(uv_edge_async_t* uv_edge_async);
    static void CancelAction(uv_edge_async_t* uv_edge_async);
    static void Unref(uv_edge_async_t* uv_edge_async);
};

class CallbackHelper {
private:
    static Nan::Callback* tickCallback;

public:
    static void Initialize();
    static void KickNextTick();
};

typedef enum taskStatus
{
    TaskStatusCreated = 0,
    TaskStatusWaitingForActivation = 1,
    TaskStatusWaitingToRun = 2,
    TaskStatusRunning = 3,
    TaskStatusWaitingForChildrenToComplete = 4,
    TaskStatusRanToCompletion = 5,
    TaskStatusCanceled = 6,
    TaskStatusFaulted = 7
} TaskStatus;

v8::Local<Value> throwV8Exception(v8::Local<Value> exception);
v8::Local<Value> throwV8Exception(const char* format, ...);

bool HasEnvironmentVariable(const char* variableName);

#endif
