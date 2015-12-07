#ifndef __CORECLR_EDGE_H
#define __CORECLR_EDGE_H

#include "../common/edge_common.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <stdio.h>

#ifndef EDGE_PLATFORM_WINDOWS
typedef int BOOL;

#define SUCCEEDED(status) ((HRESULT)(status) >= 0)
#define FAILED(status) ((HRESULT)(status) < 0)
#define HRESULT_CODE(status) ((status) & 0xFFFF)

typedef int32_t HRESULT;

const HRESULT S_OK = 0;
const HRESULT E_FAIL = -1;

#define STDMETHODCALLTYPE 
#endif

typedef void* CoreClrGcHandle;

typedef struct bootstrapperContext
{
	const char* operatingSystem;
	const char* operatingSystemVersion;
	const char* architecture;
	const char* runtimeDirectory;
	const char* applicationDirectory;
	const char* edgeNodePath;
} BootstrapperContext;

typedef void (STDMETHODCALLTYPE *CallFuncFunction)(
		CoreClrGcHandle functionHandle,
		void* payload,
		int payloadType,
		int* taskState,
		void** result,
		int* resultType);
typedef CoreClrGcHandle (STDMETHODCALLTYPE *GetFuncFunction)(
		const char* assemblyFile,
		const char* typeName,
		const char* methodName,
		void** exception);
typedef void (STDMETHODCALLTYPE *FreeHandleFunction)(CoreClrGcHandle handle);
typedef void (STDMETHODCALLTYPE *FreeMarshalDataFunction)(void* marshalData, int marshalDataType);
typedef void (STDMETHODCALLTYPE *NodejsFuncCompleteFunction)(CoreClrGcHandle context, int taskStatus, void* result, int resultType);
typedef CoreClrGcHandle (STDMETHODCALLTYPE *CompileFuncFunction)(
        const void* options,
        const int payloadType,
        void** exception);
typedef void (STDMETHODCALLTYPE *InitializeFunction)(BootstrapperContext* context, void** exception);

typedef enum v8Type
{
    V8TypeFunction = 1,
    V8TypeBuffer = 2,
    V8TypeArray = 3,
    V8TypeDate = 4,
    V8TypeObject = 5,
    V8TypeString = 6,
    V8TypeBoolean = 7,
    V8TypeInt32 = 8,
    V8TypeUInt32 = 9,
    V8TypeNumber = 10,
    V8TypeNull = 11,
    V8TypeTask = 12,
    V8TypeException = 13
} V8Type;

class CoreClrFuncInvokeContext
{
	private:
		Nan::Persistent<Function>* callback;
		CoreClrGcHandle task;
		uv_edge_async_t* uv_edge_async;
		void* resultData;
		int resultType;
		int taskState;

	public:
		bool Sync();
		void Sync(bool value);

		CoreClrFuncInvokeContext(v8::Local<v8::Value> callback, void* task);
		~CoreClrFuncInvokeContext();

		void InitializeAsyncOperation();

		static void TaskComplete(void* result, int resultType, int taskState, CoreClrFuncInvokeContext* context);
		static void TaskCompleteSynchronous(void* result, int resultType, int taskState, v8::Local<v8::Value> callback);
		static void InvokeCallback(void* data);
};

typedef void (*TaskCompleteFunction)(void* result, int resultType, int taskState, CoreClrFuncInvokeContext* context);
typedef void (STDMETHODCALLTYPE *ContinueTaskFunction)(void* task, void* context, TaskCompleteFunction callback, void** exception);

class CoreClrEmbedding
{
    private:
		CoreClrEmbedding();
        static void FreeCoreClr(void** pLibCoreClr);
        static bool LoadCoreClrAtPath(const char* loadPath, void** ppLibCoreClr);
        static void GetPathToBootstrapper(char* bootstrapper, size_t bufferSize);
        static void AddToTpaList(std::string directoryPath, std::string* tpaList);
        static void* LoadSymbol(void* library, const char* symbolName);
        static char* GetLoadError();

    public:
        static CoreClrGcHandle GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName, v8::Local<v8::Value>* exception);
        static void CallClrFunc(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType);
        static HRESULT Initialize(BOOL debugMode);
        static void ContinueTask(CoreClrGcHandle taskHandle, void* context, TaskCompleteFunction callback, void** exception);
        static void FreeHandle(CoreClrGcHandle handle);
        static void FreeMarshalData(void* marshalData, int marshalDataType);
        static CoreClrGcHandle CompileFunc(const void* options, const int payloadType, v8::Local<v8::Value>* exception);
};

class CoreClrFunc
{
	private:
		CoreClrGcHandle functionHandle;

		CoreClrFunc();

		static char* CopyV8StringBytes(v8::Local<v8::String> v8String);
		static v8::Local<v8::Function> InitializeInstance(CoreClrGcHandle functionHandle);

	public:
		static NAN_METHOD(Initialize);
		v8::Local<v8::Value> Call(v8::Local<v8::Value> payload, v8::Local<v8::Value> callbackOrSync);
		static void FreeMarshalData(void* marshalData, int payloadType);
		static void MarshalV8ToCLR(v8::Local<v8::Value> jsdata, void** marshalData, int* payloadType);
		static v8::Local<v8::Value> MarshalCLRToV8(void* marshalData, int payloadType);
		static void MarshalV8ExceptionToCLR(v8::Local<v8::Value> exception, void** marshalData);
};

class CoreClrNodejsFunc
{
	public:
		Nan::Persistent<Function>* Func;

		CoreClrNodejsFunc(v8::Local<Function> function);
		~CoreClrNodejsFunc();

		static void Call(void* payload, int payloadType, CoreClrNodejsFunc* functionContext, CoreClrGcHandle callbackContext, NodejsFuncCompleteFunction callbackFunction);
		static void Release(CoreClrNodejsFunc* function);
};

class CoreClrNodejsFuncInvokeContext
{
	private:
		uv_edge_async_t* uv_edge_async;

	public:
		void* Payload;
		int PayloadType;
		CoreClrNodejsFunc* FunctionContext;
		CoreClrGcHandle CallbackContext;
		NodejsFuncCompleteFunction CallbackFunction;

		CoreClrNodejsFuncInvokeContext(void* payload, int payloadType, CoreClrNodejsFunc* functionContext, CoreClrGcHandle callbackContext, NodejsFuncCompleteFunction callbackFunction);
		~CoreClrNodejsFuncInvokeContext();

		void Invoke();
		static void InvokeCallback(void* data);
		void Complete(TaskStatus taskStatus, void* result, int resultType);
};

typedef struct v8ObjectData
{
	int propertiesCount;
	int* propertyTypes;
	char** propertyNames;
	void** propertyData;

	v8ObjectData()
	{
		propertiesCount = 0;
		propertyTypes = NULL;
		propertyNames = NULL;
		propertyData = NULL;
	}

	~v8ObjectData()
	{
		for (int i = 0; i < propertiesCount; i++)
		{
			CoreClrFunc::FreeMarshalData(propertyData[i], propertyTypes[i]);
			delete propertyNames[i];
		}

		delete propertyNames;
		delete propertyData;
		delete propertyTypes;
	}
} V8ObjectData;

typedef struct v8ArrayData
{
	int arrayLength;
	int* itemTypes;
	void** itemValues;

	v8ArrayData()
	{
		arrayLength = 0;
		itemTypes = NULL;
		itemValues = NULL;
	}

	~v8ArrayData()
	{
		for (int i = 0; i < arrayLength; i++)
		{
			CoreClrFunc::FreeMarshalData(itemValues[i], itemTypes[i]);
		}

		delete itemValues;
		delete itemTypes;
	}
} V8ArrayData;

typedef struct v8BufferData
{
	int bufferLength;
	char* buffer;

	v8BufferData()
	{
		bufferLength = 0;
		buffer = NULL;
	}

	~v8BufferData()
	{
		delete buffer;
	}
} V8BufferData;

typedef struct coreClrFuncWrap
{
    CoreClrFunc* clrFunc;
} CoreClrFuncWrap;

typedef void (*CallV8FunctionFunction)(void* payload, int payloadType, CoreClrNodejsFunc* functionContext, CoreClrGcHandle callbackContext, NodejsFuncCompleteFunction callbackFunction);
typedef void (STDMETHODCALLTYPE *SetCallV8FunctionDelegateFunction)(CallV8FunctionFunction callV8Function, void** exception);

#endif
