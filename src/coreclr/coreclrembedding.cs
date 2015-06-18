using System;
using System.Security;
using System.Reflection;
using System.Runtime.InteropServices;

[SecurityCritical]
public class CoreCLREmbedding
{
	private static bool DebugMode = false;

	[return: MarshalAs(UnmanagedType.LPStr)]
	public delegate string CallFunctionDelegate([MarshalAs(UnmanagedType.LPStr)] string payload);

    [SecurityCritical]
	public static IntPtr GetFunc(string assemblyFile, string typeName, string methodName)
    {
		DebugMessage("Loading " + typeName + "." + methodName + "() from " + assemblyFile);

		Assembly assembly = Assembly.LoadFile(assemblyFile);
		DebugMessage("Assembly {0} loaded successfully", assemblyFile);

		ClrFuncReflectionWrap wrapper = ClrFuncReflectionWrap.Create (assembly, typeName, methodName);
		DebugMessage("Method {0}.{1}() loaded successfully", typeName, methodName);

		CallFunctionDelegate callback = new CallFunctionDelegate(wrapper.SimpleCall);

		return Marshal.GetFunctionPointerForDelegate(callback);
    }

	private static void DebugMessage(string message, params object[] parameters)
	{
		if (DebugMode) 
		{
			Console.WriteLine(message, parameters);
		}
	}

	public static void SetDebugMode(bool debugMode)
	{
		DebugMode = debugMode;
	}
}
