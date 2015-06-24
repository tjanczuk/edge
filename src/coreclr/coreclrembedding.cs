using System;
using System.Security;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Dynamic;
using System.Collections.Generic;
using System.Collections;
using System.Threading.Tasks;

[StructLayout(LayoutKind.Sequential)]
public struct JsObjectData
{
	public int propertiesCount;
	public IntPtr propertyTypes;
	public IntPtr propertyNames;
	public IntPtr propertyValues;
}

[StructLayout(LayoutKind.Sequential)]
public struct JsArrayData
{
	public int arrayLength;
	public IntPtr itemTypes;
	public IntPtr itemValues;
}

public enum JsPropertyType
{
	Function = 1,
	Buffer = 2,
	Array = 3,
	Date = 4,
	Object = 5,
	String = 6,
	Boolean = 7,
	Int32 = 8,
	UInt32 = 9,
	Number = 10,
	Null = 11,
	Task = 12
}

[SecurityCritical]
public class CoreCLREmbedding
{
	private class TaskState
	{
		public TaskCompleteDelegate Callback;
		public IntPtr Context;

		public TaskState(TaskCompleteDelegate callback, IntPtr context)
		{
			Callback = callback;
			Context = context;
		}
	}

	private static bool DebugMode = false;
	private static long MinDateTimeTicks = 621355968000000000;

	public delegate void TaskCompleteDelegate(IntPtr result, int resultType, IntPtr context);

    [SecurityCritical]
	public static IntPtr GetFunc(string assemblyFile, string typeName, string methodName)
    {
		Assembly assembly = Assembly.LoadFile(assemblyFile);
		DebugMessage("Assembly {0} loaded successfully", assemblyFile);

		ClrFuncReflectionWrap wrapper = ClrFuncReflectionWrap.Create(assembly, typeName, methodName);
		DebugMessage("Method {0}.{1}() loaded successfully", typeName, methodName);

		GCHandle wrapperHandle = GCHandle.Alloc(wrapper);

		return GCHandle.ToIntPtr(wrapperHandle);
    }

	[SecurityCritical]
	public static void FreeHandle(IntPtr gcHandle)
	{
		GCHandle actualHandle = GCHandle.FromIntPtr(gcHandle);
		actualHandle.Free();
	}

	[SecurityCritical]
	public static void CallFunc(IntPtr function, IntPtr payload, int payloadType, IntPtr taskState, IntPtr result, IntPtr resultType)
	{
		// TODO: add exception handling
		GCHandle wrapperHandle = GCHandle.FromIntPtr(function);
		ClrFuncReflectionWrap wrapper = (ClrFuncReflectionWrap)wrapperHandle.Target;

		Task<Object> functionTask = wrapper.Call(JsToObject(payload, payloadType));

		if (functionTask.IsCompleted)
		{
			int objectType;
			IntPtr resultObject = ObjectToJs(functionTask.Result, out objectType);

			Marshal.WriteInt32(taskState, (int)TaskStatus.RanToCompletion);
			Marshal.WriteIntPtr(result, resultObject);
			Marshal.WriteInt32(resultType, objectType);
		}

		else
		{
			GCHandle taskHandle = GCHandle.Alloc(functionTask);

			Marshal.WriteInt32(taskState, (int)functionTask.Status);
			Marshal.WriteIntPtr(result, GCHandle.ToIntPtr(taskHandle));
			Marshal.WriteInt32(resultType, (int)JsPropertyType.Task);
		}
	}

	private static void TaskCompleted(Task<object> task, object state)
	{
		int objectType;
		IntPtr resultObject = ObjectToJs(task.Result, out objectType);
		TaskState actualState = (TaskState) state;

		actualState.Callback(resultObject, objectType, actualState.Context);
	}

	[SecurityCritical]
	public static void ContinueTask(IntPtr task, IntPtr context, IntPtr callback)
	{
		GCHandle taskHandle = GCHandle.FromIntPtr(task);
		Task<Object> actualTask = (Task<Object>) taskHandle.Target;
		TaskCompleteDelegate taskCompleteDelegate = Marshal.GetDelegateForFunctionPointer<TaskCompleteDelegate>(callback);

		// Will complete asynchronously. Schedule continuation to finish processing.
		actualTask.ContinueWith(new Action<Task<object>, object>(TaskCompleted), new TaskState(taskCompleteDelegate, context));
	}

	private static IntPtr ObjectToJs(object clrObject, out int objectType)
	{	
		if (clrObject == null)
		{
			objectType = (int)JsPropertyType.Null;
			return IntPtr.Zero;
		}

		if (clrObject is string)
		{
			objectType = (int)JsPropertyType.String;
			return Marshal.StringToHGlobalAnsi((string)clrObject);
		}

		else
		{
			throw new Exception("Unsupported CLR object type: " + clrObject.GetType().FullName + ".");
		}
	}

	private static object JsToObject(IntPtr payload, int payloadType)
	{
		switch ((JsPropertyType) payloadType) 
		{
			case JsPropertyType.String:
				return Marshal.PtrToStringAnsi(payload);

			case JsPropertyType.Object:
				return JsObjectToExpando(Marshal.PtrToStructure<JsObjectData>(payload));

			case JsPropertyType.Boolean:
				return Marshal.ReadByte(payload) != 0;

			case JsPropertyType.Number:
				double[] value = new double[1];
				Marshal.Copy(payload, value, 0, 1);

				return value[0];

			case JsPropertyType.Date:
				double[] ticks = new double[1];
				Marshal.Copy(payload, ticks, 0, 1);

				return new DateTime(Convert.ToInt64(ticks[0]) * 10000 + MinDateTimeTicks, DateTimeKind.Utc);

			case JsPropertyType.Null:
				return null;

			case JsPropertyType.Int32:
				return Marshal.ReadInt32(payload);

			case JsPropertyType.UInt32:
				return (uint) Marshal.ReadInt32(payload);

			case JsPropertyType.Array:
				JsArrayData arrayData = Marshal.PtrToStructure<JsArrayData>(payload);
				int[] itemTypes = new int[arrayData.arrayLength];
				IntPtr[] itemValues = new IntPtr[arrayData.arrayLength];
				List<object> array = new List<object>();

				Marshal.Copy(arrayData.itemTypes, itemTypes, 0, arrayData.arrayLength);
				Marshal.Copy(arrayData.itemValues, itemValues, 0, arrayData.arrayLength);

				for (int i = 0; i < arrayData.arrayLength; i++)
				{
					array.Add(JsToObject(itemValues[i], itemTypes[i]));
				}

				return array.ToArray();

			default:
				throw new Exception("Unsupported payload type: " + payloadType + ".");
		}
	}

	private static ExpandoObject JsObjectToExpando(JsObjectData payload)
	{
		ExpandoObject expando = new ExpandoObject();
		IDictionary<string, object> expandoDictionary = (IDictionary<string, object>) expando;
		int[] propertyTypes = new int[payload.propertiesCount];
		IntPtr[] propertyNamePointers = new IntPtr[payload.propertiesCount];
		IntPtr[] propertyValuePointers = new IntPtr[payload.propertiesCount];

		Marshal.Copy(payload.propertyTypes, propertyTypes, 0, payload.propertiesCount);
		Marshal.Copy(payload.propertyNames, propertyNamePointers, 0, payload.propertiesCount);
		Marshal.Copy(payload.propertyValues, propertyValuePointers, 0, payload.propertiesCount);

		for (int i = 0; i < payload.propertiesCount; i++) 
		{
			IntPtr propertyNamePointer = propertyNamePointers [i];
			IntPtr propertyValuePointer = propertyValuePointers [i];
			string propertyName = Marshal.PtrToStringAnsi(propertyNamePointer);

			expandoDictionary[propertyName] = JsToObject(propertyValuePointer, propertyTypes[i]);
		}

		return expando;
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
