#include "owin.h"

static ClrFunc::ClrFunc()
{
    ClrFunc::apps = gcnew List<ClrFunc^>();
}

ClrFunc::ClrFunc()
{
    // empty
}

BOOL ClrFunc::TryCompile(
    System::String^ csx, 
    cli::array<System::Object^>^ references, 
    System::String^% errors, 
    Assembly^% assembly)
{
    BOOL result = FALSE;
    assembly = nullptr;
    errors = nullptr;

    Dictionary<System::String^, System::String^>^ options = gcnew Dictionary<System::String^, System::String^>();
    options->Add("CompilerVersion", "v4.0");
    CSharpCodeProvider^ csc = gcnew CSharpCodeProvider(options);
    CompilerParameters^ parameters = gcnew CompilerParameters();
    parameters->GenerateInMemory = true;
    if (references != nullptr)
    {
        for each (System::Object^ reference in references)
        {
            parameters->ReferencedAssemblies->Add((System::String^)reference);
        }
    }

    CompilerResults^ results = csc->CompileAssemblyFromSource(parameters, csx);
    if (results->Errors->HasErrors) 
    {
        for (int i = 0; i < results->Errors->Count; i++)
        {
            if (errors == nullptr)
            {
                errors = results->Errors[i]->ToString();
            }
            else
            {
                errors += "\n" + results->Errors[i]->ToString();
            }
        }
    }
    else 
    {
        assembly = results->CompiledAssembly;
        result = TRUE;
    }   

    return result;
}

Handle<Value> ClrFunc::Initialize(const v8::Arguments& args)
{
    DBG("ClrFunc::Initialize");

    HandleScope scope;
    Handle<v8::Object> options = args[0]->ToObject();
    Assembly^ assembly;
    System::String^ typeName;
    System::String^ methodName;

    try 
    {
        Handle<v8::Value> jsassemblyFile = options->Get(String::NewSymbol("assemblyFile"));
        String::Utf8Value assemblyFile(jsassemblyFile);
        String::Utf8Value nativeTypeName(options->Get(String::NewSymbol("typeName")));
        String::Utf8Value nativeMethodName(options->Get(String::NewSymbol("methodName")));  
        typeName = gcnew System::String(*nativeTypeName);
        methodName = gcnew System::String(*nativeMethodName);      
        if (jsassemblyFile->IsString()) {
            assembly = Assembly::LoadFrom(gcnew System::String(*assemblyFile));
        }
        else {
            cli::array<System::Object^>^ references = 
                (cli::array<System::Object^>^)ClrFunc::MarshalV8ToCLR(nullptr, options->Get(String::NewSymbol("references")));
            String::Utf8Value nativencsx(options->Get(String::NewSymbol("csx")));
            System::String^ csx = gcnew System::String(*nativencsx);
            System::String^ errorsClass;
            if (!ClrFunc::TryCompile(csx, references, errorsClass, assembly)) {
                csx = "using System;\n"
                    + "using System.Threading.Tasks;\n"
                    + "public class Startup {\n"
                    + "    public async Task<object> Invoke(object ___input) {\n"
                    + "        Func<object, Task<object>> func = " + csx + ";\n"
                    + "        return await func(___input);\n"
                    + "    }\n"
                    + "}";
                System::String^ errorsLambda;
                if (!ClrFunc::TryCompile(csx, references, errorsLambda, assembly)) {
                    throw gcnew System::InvalidOperationException(
                        "Unable to compile C# code.\n----> Errors when compiling as a CLR library:\n"
                        + errorsClass
                        + "\n----> Errors when compiling as a CLR async lambda expression:\n"
                        + errorsLambda);
                }
            }
        }

        System::Type^ startupType = assembly->GetType(typeName, true, true);
        ClrFunc^ app = gcnew ClrFunc();
        app->instance = System::Activator::CreateInstance(startupType, false);
        app->invokeMethod = startupType->GetMethod(methodName, BindingFlags::Instance | BindingFlags::Public);
		if (app->invokeMethod == nullptr) 
		{
			throw gcnew System::InvalidOperationException(
                "Unable to access the CLR method to wrap through reflection. Make sure it is a public instance method.");
		}

        ClrFunc::apps->Add(app);
    }
    catch (System::Exception^ e)
    {
        return scope.Close(throwV8Exception(e));
    }

    return scope.Close(Integer::New(ClrFunc::apps->Count));
}

void owinAppCompletedOnCLRThread(Task<System::Object^>^ task, System::Object^ state)
{
    DBG("owinAppCompletedOnCLRThread");
    ClrFuncInvokeContext^ context = (ClrFuncInvokeContext^)state;
    context->CompleteOnCLRThread(task);
}

Handle<v8::Value> ClrFunc::MarshalCLRToV8(System::Object^ netdata)
{
    HandleScope scope;
    Handle<v8::String> serialized;
    OwinJavaScriptConverter^ converter = gcnew OwinJavaScriptConverter();
    Handle<v8::Value> jsdata;

    try 
    {
        JavaScriptSerializer^ serializer = gcnew JavaScriptSerializer();
        serializer->RegisterConverters(gcnew cli::array<JavaScriptConverter^> { converter });
        serialized = stringCLR2V8(serializer->Serialize(netdata));
        Handle<v8::Value> argv[] = { serialized };
        jsdata = jsonParse->Call(json, 1, argv);
        if (converter->Buffers->Count > 0)
        {
            // fixup object graph to replace buffer placeholders with buffers
            jsdata = converter->FixupBuffers(jsdata);
        }
    }
    catch (System::Exception^ e)
    {
        return scope.Close(throwV8Exception(e));
    }

    return scope.Close(jsdata);
}

System::Object^ ClrFunc::MarshalV8ToCLR(ClrFuncInvokeContext^ context, Handle<v8::Value> jsdata)
{
    HandleScope scope;

    if (jsdata->IsFunction() && context != nullptr) 
    {
        NodejsFunc^ functionContext = gcnew NodejsFunc(
            context, Handle<v8::Function>::Cast(jsdata));
        System::Func<System::Object^,Task<System::Object^>^>^ netfunc = 
            gcnew System::Func<System::Object^,Task<System::Object^>^>(
                functionContext, &NodejsFunc::FunctionWrapper);

        return netfunc;
    }
    else if (node::Buffer::HasInstance(jsdata))
    {
        Handle<v8::Object> jsbuffer = jsdata->ToObject();
        cli::array<byte>^ netbuffer = gcnew cli::array<byte>((int)node::Buffer::Length(jsbuffer));
        pin_ptr<byte> pinnedNetbuffer = &netbuffer[0];
        memcpy(pinnedNetbuffer, node::Buffer::Data(jsbuffer), netbuffer->Length);

        return netbuffer;
    }
    else if (jsdata->IsArray())
    {
        Handle<v8::Array> jsarray = Handle<v8::Array>::Cast(jsdata);
        cli::array<System::Object^>^ netarray = gcnew cli::array<System::Object^>(jsarray->Length());
        for (unsigned int i = 0; i < jsarray->Length(); i++)
        {
            netarray[i] = ClrFunc::MarshalV8ToCLR(context, jsarray->Get(i));
        }

        return netarray;
    }
    else if (jsdata->IsObject()) 
    {
        Dictionary<System::String^,System::Object^>^ netobject = gcnew Dictionary<System::String^,System::Object^>();
        Handle<v8::Object> jsobject = Handle<v8::Object>::Cast(jsdata);
        Handle<v8::Array> propertyNames = jsobject->GetPropertyNames();
        for (unsigned int i = 0; i < propertyNames->Length(); i++)
        {
            Handle<v8::String> name = Handle<v8::String>::Cast(propertyNames->Get(i));
            String::Utf8Value utf8name(name);
            System::String^ netname = gcnew System::String(*utf8name);
            System::Object^ netvalue = ClrFunc::MarshalV8ToCLR(context, jsobject->Get(name));
            netobject->Add(netname, netvalue);
        }

        return netobject;
    }
    else if (jsdata->IsString()) 
    {
        return stringV82CLR(Handle<v8::String>::Cast(jsdata));
    }
    else if (jsdata->IsBoolean())
    {
        return jsdata->BooleanValue();
    }
    else if (jsdata->IsInt32())
    {
        return jsdata->Int32Value();
    }
    else if (jsdata->IsUint32()) 
    {
        return jsdata->Uint32Value();
    }
    else if (jsdata->IsNumber()) 
    {
        return jsdata->NumberValue();
    }
    else if (jsdata->IsUndefined() || jsdata->IsNull())
    {
        return nullptr;
    }
    else
    {
        throw gcnew System::Exception("Unable to convert V8 value to CLR value.");
    }
}

Handle<Value> ClrFunc::Call(const Arguments& args) 
{
    DBG("ClrFunc::Call");
    HandleScope scope;
    
    try 
    {
        int appId = args[0]->Int32Value();
        ClrFuncInvokeContext^ context = gcnew ClrFuncInvokeContext(Handle<v8::Function>::Cast(args[2]));
        context->Payload = ClrFunc::MarshalV8ToCLR(context, args[1]);
        ClrFunc^ app = ClrFunc::apps->default[appId - 1];
        Task<System::Object^>^ task = (Task<System::Object^>^)app->invokeMethod->Invoke(
            app->instance, gcnew array<System::Object^> { context->Payload });
        task->ContinueWith(gcnew System::Action<Task<System::Object^>^,System::Object^>(owinAppCompletedOnCLRThread), context);
    }
    catch (System::Exception^ e)
    {
        return scope.Close(throwV8Exception(e));
    }

    return scope.Close(Undefined());
}
