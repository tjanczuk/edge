#ifndef __CORECLR_EDGE_H
#define __CORECLR_EDGE_H

#include "../common/edge_common.h"
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <string>
#include <stdio.h>
#include <uchar.h>

typedef int BOOL;
typedef const char16_t* LPCTSTR;
typedef LPCTSTR LPCWSTR;
typedef int* INT_PTR;
typedef unsigned long ULONG;
typedef double ULONGLONG;

#define _T(str) str
#define SUCCEEDED(status) ((HRESULT)(status) >= 0)
#define FAILED(status) ((HRESULT)(status) < 0)
#define HRESULT_CODE(status) ((status) & 0xFFFF)

typedef int32_t HRESULT;
typedef const char* LPCSTR;
typedef uint32_t DWORD;
typedef void IUnknown;
typedef HRESULT (FExecuteInAppDomainCallback)(void *cookie);
typedef void* CoreClrGcHandle;

typedef void (*CallFuncFunction)(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType);
typedef CoreClrGcHandle (*GetFuncFunction)(const char* assemblyFile, const char* typeName, const char* methodName);
typedef void (*SetDebugModeFunction)(const BOOL debugMode);
typedef void (*FreeHandleFunction)(CoreClrGcHandle handle);
typedef void (*FreeMarshalDataFunction)(void* marshalData, int marshalDataType);

// TODO: use NaN for this
#define StringToUTF16(input, output)\
{\
	char16_t c16str[3] = u"\0";\
	mbstate_t mbs;\
	for (const auto& it: input)\
	{\
		memset (&mbs, 0, sizeof (mbs));\
		memmove(c16str, u"\0\0\0", 3);\
		mbrtoc16(c16str, &it, 3, &mbs);\
		output.append(std::u16string(c16str));\
	}\
};

struct IID
{
    unsigned long Data1;
    unsigned short Data2;
    unsigned short Data3;
    unsigned char Data4[8];
};

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

typedef IID* REFIID;

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

const HRESULT S_OK = 0;
const HRESULT E_FAIL = -1;

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
    PropertyTypeTask = 12
} V8Type;

class CoreClrFuncInvokeContext
{
	private:
		Persistent<Function>* callback;
		CoreClrGcHandle task;
		uv_edge_async_t* uv_edge_async;
		void* resultData;
		int resultType;

	public:
		bool Sync();
		void Sync(bool value);

		CoreClrFuncInvokeContext(Handle<v8::Value> callback, void* task);
		~CoreClrFuncInvokeContext();

		void InitializeAsyncOperation();

		static void TaskComplete(void* result, int resultType, CoreClrFuncInvokeContext* context);
		static void InvokeCallback(void* data);
};

typedef void (*TaskCompleteFunction)(void* result, int resultType, CoreClrFuncInvokeContext* context);
typedef void (*ContinueTaskFunction)(void* task, void* context, TaskCompleteFunction callback);

class CoreClrEmbedding
{
    private:
		CoreClrEmbedding();
        static void FreeCoreClr(void** pLibCoreClr);
        static bool LoadCoreClrAtPath(const char* loadPath, void** ppLibCoreClr);
        static void GetPathToBootstrapper(char* bootstrapper, size_t bufferSize);
        static void AddToTpaList(std::string directoryPath, std::string* tpaList);

    public:
        static CoreClrGcHandle GetClrFuncReflectionWrapFunc(const char* assemblyFile, const char* typeName, const char* methodName);
        static void CallClrFunc(CoreClrGcHandle functionHandle, void* payload, int payloadType, int* taskState, void** result, int* resultType);
        static HRESULT Initialize(BOOL debugMode);
        static void ContinueTask(CoreClrGcHandle taskHandle, void* context, TaskCompleteFunction callback);
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
		//static Handle<v8::Value> MarshalCLRToV8(MonoObject* netdata, MonoException** exc);
		//static Handle<v8::Object> MarshalCLRExceptionToV8(MonoException* exception);
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

#endif
