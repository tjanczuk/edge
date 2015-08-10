#ifndef __CORECLR_EDGE_H
#define __CORECLR_EDGE_H

#include "../common/edge_common.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <stdio.h>

#ifndef EDGE_PLATFORM_WINDOWS
#include <uchar.h>

typedef int BOOL;
typedef const char16_t* LPCTSTR;
typedef LPCTSTR LPCWSTR;
typedef LPCTSTR LPWSTR;
typedef int* INT_PTR;
typedef unsigned long ULONG;
typedef double ULONGLONG;

#define SUCCEEDED(status) ((HRESULT)(status) >= 0)
#define FAILED(status) ((HRESULT)(status) < 0)
#define HRESULT_CODE(status) ((status) & 0xFFFF)

typedef int32_t HRESULT;
typedef const char* LPCSTR;
typedef uint32_t DWORD;
typedef void IUnknown;

const HRESULT S_OK = 0;
const HRESULT E_FAIL = -1;

struct IID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};

typedef IID* REFIID;
#endif

#ifdef EDGE_PLATFORM_WINDOWS
#define _u(stringLiteral) L ##stringLiteral
#else
#define _u(stringLiteral) u ##stringLiteral
#endif

typedef HRESULT (FExecuteInAppDomainCallback)(void *cookie);
typedef void* CoreClrGcHandle;

typedef void (*CallFuncFunction)(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType);
typedef CoreClrGcHandle (*GetFuncFunction)(const char* assemblyFile, const char* typeName, const char* methodName, void** exception);
typedef void (*SetDebugModeFunction)(const BOOL debugMode);
typedef void (*FreeHandleFunction)(CoreClrGcHandle handle);
typedef void (*FreeMarshalDataFunction)(void* marshalData, int marshalDataType);
typedef void (*NodejsFuncCompleteFunction)(CoreClrGcHandle context, int taskStatus, void* result, int resultType);

#ifdef EDGE_PLATFORM_WINDOWS
#define StringToUTF16(input, output)\
{\
    int len;\
    int slength = (int)input.length() + 1;\
    len = MultiByteToWideChar(CP_ACP, 0, input.c_str(), slength, 0, 0); \
    output = new wchar_t[len];\
    MultiByteToWideChar(CP_ACP, 0, input.c_str(), slength, output, len);\
};
#else
#define StringToUTF16(input, output)\
{\
	char16_t c16str[3] = _u("\0");\
	mbstate_t mbs;\
	for (const auto& it: input)\
	{\
		memset (&mbs, 0, sizeof (mbs));\
		memmove(c16str, _u("\0\0\0"), 3);\
		mbrtoc16(c16str, &it, 3, &mbs);\
		output.append(std::u16string(c16str));\
	}\
};
#endif

typedef enum _STARTUP_FLAGS
{
    STARTUP_CONCURRENT_GC = 0x1,
    STARTUP_LOADER_OPTIMIZATION_MASK = (0x3 << 1),
    STARTUP_LOADER_OPTIMIZATION_SINGLE_DOMAIN = (0x1 << 1) ,
    STARTUP_LOADER_OPTIMIZATION_MULTI_DOMAIN = (0x2 << 1) ,
    STARTUP_LOADER_OPTIMIZATION_MULTI_DOMAIN_HOST = (0x3 << 1) ,
    STARTUP_LOADER_SAFEMODE = 0x10,
    STARTUP_LOADER_SETPREFERENCE = 0x100,
    STARTUP_SERVER_GC = 0x1000,
    STARTUP_HOARD_GC_VM = 0x2000,
    STARTUP_SINGLE_VERSION_HOSTING_INTERFACE = 0x4000,
    STARTUP_LEGACY_IMPERSONATION = 0x10000,
    STARTUP_DISABLE_COMMITTHREADSTACK = 0x20000,
    STARTUP_ALWAYSFLOW_IMPERSONATION = 0x40000,
    STARTUP_TRIM_GC_COMMIT = 0x80000,
    STARTUP_ETW = 0x100000,
    STARTUP_ARM = 0x400000,
    STARTUP_SINGLE_APPDOMAIN = 0x800000,
    STARTUP_APPX_APP_MODEL = 0x1000000,
    STARTUP_DISABLE_RANDOMIZED_STRING_HASHING = 0x2000000
} STARTUP_FLAGS;

typedef enum _APPDOMAIN_SECURITY_FLAGS
{
    APPDOMAIN_SECURITY_DEFAULT = 0,
    APPDOMAIN_SECURITY_SANDBOXED = 0x1,
    APPDOMAIN_SECURITY_FORBID_CROSSAD_REVERSE_PINVOKE = 0x2,
    APPDOMAIN_IGNORE_UNHANDLED_EXCEPTIONS = 0x4,
    APPDOMAIN_FORCE_TRIVIAL_WAIT_OPERATIONS = 0x8,
    APPDOMAIN_ENABLE_PINVOKE_AND_CLASSIC_COMINTEROP = 0x10,
    APPDOMAIN_SET_TEST_KEY = 0x20,
    APPDOMAIN_ENABLE_PLATFORM_SPECIFIC_APPS = 0x40,
    APPDOMAIN_ENABLE_ASSEMBLY_LOADFILE = 0x80,
    APPDOMAIN_DISABLE_TRANSPARENCY_ENFORCEMENT = 0x100
} APPDOMAIN_SECURITY_FLAGS;

class ICLRRuntimeHost2 
{
    public:
        virtual HRESULT QueryInterface(
            /* [in] */ REFIID riid,
            /* [annotation][iid_is][out] */
            void **ppvObject);

        virtual ULONG AddRef();
        
        virtual ULONG Release();

        virtual HRESULT Start() = 0;

        virtual HRESULT Stop() = 0;

        virtual HRESULT SetHostControl(
            /* [in] */ void *pHostControl) = 0;

        virtual HRESULT GetCLRControl(
            /* [out] */ void **pCLRControl) = 0;

        virtual HRESULT UnloadAppDomain(
            /* [in] */ DWORD dwAppDomainId,
            /* [in] */ BOOL fWaitUntilDone) = 0;

        virtual HRESULT ExecuteInAppDomain(
            /* [in] */ DWORD dwAppDomainId,
            /* [in] */ FExecuteInAppDomainCallback pCallback,
            /* [in] */ void *cookie) = 0;

        virtual HRESULT GetCurrentAppDomainId(
            /* [out] */ DWORD *pdwAppDomainId) = 0;

        virtual HRESULT ExecuteApplication(
            /* [in] */ LPCWSTR pwzAppFullName,
            /* [in] */ DWORD dwManifestPaths,
            /* [in] */ LPCWSTR *ppwzManifestPaths,
            /* [in] */ DWORD dwActivationData,
            /* [in] */ LPCWSTR *ppwzActivationData,
            /* [out] */ int *pReturnValue) = 0;

        virtual HRESULT ExecuteInDefaultAppDomain(
            /* [in] */ LPCWSTR pwzAssemblyPath,
            /* [in] */ LPCWSTR pwzTypeName,
            /* [in] */ LPCWSTR pwzMethodName,
            /* [in] */ LPCWSTR pwzArgument,
            /* [out] */ DWORD *pReturnValue) = 0;

        virtual HRESULT CreateAppDomainWithManager(
            /* [in] */ LPCWSTR wszFriendlyName,
            /* [in] */ DWORD dwFlags,
            /* [in] */ LPCWSTR wszAppDomainManagerAssemblyName,
            /* [in] */ LPCWSTR wszAppDomainManagerTypeName,
            /* [in] */ int nProperties,
            /* [in] */ LPCWSTR *pPropertyNames,
            /* [in] */ LPCWSTR *pPropertyValues,
            /* [out] */ DWORD *pAppDomainID) = 0;

        virtual HRESULT CreateDelegate(
            /* [in] */ DWORD appDomainID,
            /* [in] */ LPCWSTR wszAssemblyName,
            /* [in] */ LPCWSTR wszClassName,
            /* [in] */ LPCWSTR wszMethodName,
            /* [out] */ INT_PTR *fnPtr) = 0;

        virtual HRESULT Authenticate(
            /* [in] */ ULONGLONG authKey) = 0;

        virtual HRESULT RegisterMacEHPort() = 0;

        virtual HRESULT SetStartupFlags(
            /* [in] */ STARTUP_FLAGS dwFlags) = 0;
};

typedef enum v8Type
{
    PropertyTypeFunction = 1,
    PropertyTypeBuffer = 2,
    PropertyTypeArray = 3,
    PropertyTypeDate = 4,
    PropertyTypeObject = 5,
    PropertyTypeString = 6,
    PropertyTypeBoolean = 7,
    PropertyTypeInt32 = 8,
    PropertyTypeUInt32 = 9,
    PropertyTypeNumber = 10,
    PropertyTypeNull = 11,
    PropertyTypeTask = 12,
    PropertyTypeException = 13
} V8Type;

class CoreClrFuncInvokeContext
{
	private:
		Persistent<Function>* callback;
		CoreClrGcHandle task;
		uv_edge_async_t* uv_edge_async;
		void* resultData;
		int resultType;
		int taskState;

	public:
		bool Sync();
		void Sync(bool value);

		CoreClrFuncInvokeContext(Handle<v8::Value> callback, void* task);
		~CoreClrFuncInvokeContext();

		void InitializeAsyncOperation();

		static void TaskComplete(void* result, int resultType, int taskState, CoreClrFuncInvokeContext* context);
		static void TaskCompleteSynchronous(void* result, int resultType, int taskState, Handle<v8::Value> callback);
		static void InvokeCallback(void* data);
};

typedef void (*TaskCompleteFunction)(void* result, int resultType, int taskState, CoreClrFuncInvokeContext* context);
typedef void (*ContinueTaskFunction)(void* task, void* context, TaskCompleteFunction callback, void** exception);

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
        static CoreClrGcHandle GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName, v8::Handle<v8::Value>* exception);
        static void CallClrFunc(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType);
        static HRESULT Initialize(BOOL debugMode);
        static void ContinueTask(CoreClrGcHandle taskHandle, void* context, TaskCompleteFunction callback, void** exception);
        static void FreeHandle(CoreClrGcHandle handle);
        static void FreeMarshalData(void* marshalData, int marshalDataType);
};

class CoreClrFunc
{
	private:
		CoreClrGcHandle functionHandle;

		CoreClrFunc();

		static char* CopyV8StringBytes(Handle<v8::String> v8String);
		static Handle<v8::Function> InitializeInstance(CoreClrGcHandle functionHandle);

	public:
		static NAN_METHOD(Initialize);
		Handle<v8::Value> Call(Handle<v8::Value> payload, Handle<v8::Value> callbackOrSync);
		static void FreeMarshalData(void* marshalData, int payloadType);
		static void MarshalV8ToCLR(Handle<v8::Value> jsdata, void** marshalData, int* payloadType);
		static Handle<v8::Value> MarshalCLRToV8(void* marshalData, int payloadType);
		static void MarshalV8ExceptionToCLR(Handle<v8::Value> exception, void** marshalData);
};

class CoreClrNodejsFunc
{
	public:
		Persistent<Function>* Func;

		CoreClrNodejsFunc(Handle<Function> function);
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
	int propertiesCount = 0;
	int* propertyTypes;
	char** propertyNames;
	void** propertyData;

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
	int arrayLength = 0;
	int* itemTypes;
	void** itemValues;

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
	int bufferLength = 0;
	char* buffer;

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
typedef void (*SetCallV8FunctionDelegateFunction)(CallV8FunctionFunction callV8Function, void** exception);

#endif
