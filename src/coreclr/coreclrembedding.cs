using System;
using System.Linq;
using System.Security;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Linq.Expressions;
using System.Dynamic;
using System.Collections.Generic;
using System.Collections;
using System.Threading.Tasks;
using System.IO;

[StructLayout(LayoutKind.Sequential)]
public struct V8ObjectData
{
	public int propertiesCount;
	public IntPtr propertyTypes;
	public IntPtr propertyNames;
	public IntPtr propertyValues;
}

[StructLayout(LayoutKind.Sequential)]
public struct V8ArrayData
{
	public int arrayLength;
	public IntPtr itemTypes;
	public IntPtr itemValues;
}

[StructLayout(LayoutKind.Sequential)]
public struct V8BufferData
{
	public int bufferLength;
	public IntPtr buffer;
}

public enum V8Type
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
	Task = 12,
	Exception = 13
}

public delegate void CallV8FunctionDelegate(IntPtr payload, int payloadType, IntPtr v8FunctionContext, IntPtr callbackContext, IntPtr callbackDelegate);
public delegate void TaskCompleteDelegate(IntPtr result, int resultType, int taskState, IntPtr context);

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
	private static Dictionary<Type, List<Tuple<string, Func<object, object>>>> TypePropertyAccessors = new Dictionary<Type, List<Tuple<string, Func<object, object>>>>();

    [SecurityCritical]
	public static IntPtr GetFunc(string assemblyFile, string typeName, string methodName, IntPtr exception)
    {
		try
		{
			Marshal.WriteIntPtr(exception, IntPtr.Zero);

			if (!Path.IsPathRooted(assemblyFile))
			{
				assemblyFile = Path.Combine(Directory.GetCurrentDirectory(), assemblyFile);
			}

			Assembly assembly = Assembly.LoadFile(assemblyFile);
			DebugMessage("CoreCLREmbedding::GetFunc (CLR) - Assembly {0} loaded successfully", assemblyFile);

			ClrFuncReflectionWrap wrapper = ClrFuncReflectionWrap.Create(assembly, typeName, methodName);
			DebugMessage("CoreCLREmbedding::GetFunc (CLR) - Method {0}.{1}() loaded successfully", typeName, methodName);

			Func<object, Task<object>> wrapperFunc = new Func<object, Task<object>>(wrapper.Call);
			GCHandle wrapperHandle = GCHandle.Alloc(wrapperFunc);

			return GCHandle.ToIntPtr(wrapperHandle);
		}

		catch (Exception e)
		{
			DebugMessage("CoreCLREmbedding::GetFunc (CLR) - Exception was thrown: {0}", e.Message);

			V8Type v8type;
			Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8type));

			return IntPtr.Zero;
		}
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
		try
		{
			DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Starting");

			GCHandle wrapperHandle = GCHandle.FromIntPtr(function);
			Func<object, Task<object>> wrapperFunc = (Func<object, Task<object>>)wrapperHandle.Target;

			DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Marshalling data of type {0} and calling the .NET method", ((V8Type)payloadType).ToString("G"));
			Task<Object> functionTask = wrapperFunc(MarshalV8ToCLR(payload, (V8Type)payloadType));

			if (functionTask.IsFaulted)
			{
				DebugMessage("CoreCLREmbedding::CallFunc (CLR) - .NET method ran synchronously and faulted, marshalling exception data for V8");

				V8Type taskExceptionType;

				Marshal.WriteInt32(taskState, (int)TaskStatus.Faulted);
				Marshal.WriteIntPtr(result, MarshalCLRToV8(functionTask.Exception, out taskExceptionType));
				Marshal.WriteInt32(resultType, (int)V8Type.Exception);
			}

			else if (functionTask.IsCompleted)
			{
				DebugMessage("CoreCLREmbedding::CallFunc (CLR) - .NET method ran synchronously, marshalling data for V8");

				V8Type taskResultType;
				IntPtr marshalData = MarshalCLRToV8(functionTask.Result, out taskResultType);

				DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Method return data is of type {0}", taskResultType.ToString("G"));

				Marshal.WriteInt32(taskState, (int)TaskStatus.RanToCompletion);
				Marshal.WriteIntPtr(result, marshalData);
				Marshal.WriteInt32(resultType, (int)taskResultType);
			}

			else
			{
				DebugMessage("CoreCLREmbedding::CallFunc (CLR) - .NET method ran asynchronously, returning task handle and status");

				GCHandle taskHandle = GCHandle.Alloc(functionTask);

				Marshal.WriteInt32(taskState, (int)functionTask.Status);
				Marshal.WriteIntPtr(result, GCHandle.ToIntPtr(taskHandle));
				Marshal.WriteInt32(resultType, (int)V8Type.Task);
			}

			DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Finished");
		}

		catch (Exception e)
		{
			DebugMessage("CoreCLREmbedding::CallFunc (CLR) - Exception was thrown: {0}", e.Message);

			V8Type v8Type;

			Marshal.WriteIntPtr(result, MarshalCLRToV8(e, out v8Type));
			Marshal.WriteInt32(resultType, (int)v8Type);
			Marshal.WriteInt32(taskState, (int)TaskStatus.Faulted);
		}
	}

	private static void TaskCompleted(Task<object> task, object state)
	{
		DebugMessage("CoreCLREmbedding::TaskCompleted (CLR) - Task completed with a state of {0}", task.Status.ToString("G"));
		DebugMessage("CoreCLREmbedding::TaskCompleted (CLR) - Marshalling data to return to V8", task.Status.ToString("G"));

		V8Type v8Type;
		TaskState actualState = (TaskState)state;
		IntPtr resultObject = IntPtr.Zero;
		TaskStatus taskStatus;

		if (task.IsFaulted)
		{
			taskStatus = TaskStatus.Faulted;

			try
			{
				resultObject = MarshalCLRToV8(task.Exception, out v8Type);
			}

			catch (Exception e)
			{
				taskStatus = TaskStatus.Faulted;
				resultObject = MarshalCLRToV8(e, out v8Type);
			}
		}

		else
		{
			taskStatus = TaskStatus.RanToCompletion;

			try
			{
				resultObject = MarshalCLRToV8(task.Result, out v8Type);
			}

			catch (Exception e)
			{
				taskStatus = TaskStatus.Faulted;
				resultObject = MarshalCLRToV8(e, out v8Type);
			}
		}

		DebugMessage("CoreCLREmbedding::TaskCompleted (CLR) - Invoking unmanaged callback");
		actualState.Callback(resultObject, (int)v8Type, (int)taskStatus, actualState.Context);
	}

	[SecurityCritical]
	public static void ContinueTask(IntPtr task, IntPtr context, IntPtr callback, IntPtr exception)
	{
		try
		{
			Marshal.WriteIntPtr(exception, IntPtr.Zero);

			DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Starting");

			GCHandle taskHandle = GCHandle.FromIntPtr(task);
			Task<Object> actualTask = (Task<Object>) taskHandle.Target;

			TaskCompleteDelegate taskCompleteDelegate = Marshal.GetDelegateForFunctionPointer<TaskCompleteDelegate>(callback);
			DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Marshalled unmanaged callback successfully");

			actualTask.ContinueWith(new Action<Task<object>, object>(TaskCompleted), new TaskState(taskCompleteDelegate, context));

			DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Finished");
		}

		catch (Exception e)
		{
			DebugMessage("CoreCLREmbedding::ContinueTask (CLR) - Exception was thrown: {0}", e.Message);

			V8Type v8type;
			Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8type));
		}
	}

	[SecurityCritical]
	public static void SetCallV8FunctionDelegate(IntPtr callV8Function, IntPtr exception)
	{
		try
		{
			Marshal.WriteIntPtr(exception, IntPtr.Zero);
			NodejsFuncInvokeContext.CallV8Function = Marshal.GetDelegateForFunctionPointer<CallV8FunctionDelegate>(callV8Function);
		}

		catch (Exception e)
		{
			DebugMessage("CoreCLREmbedding::SetCallV8FunctionDelegate (CLR) - Exception was thrown: {0}", e.Message);

			V8Type v8type;
			Marshal.WriteIntPtr(exception, MarshalCLRToV8(e, out v8type));
		}
	}

	[SecurityCritical]
	public static void FreeMarshalData(IntPtr marshalData, int v8Type)
	{
		switch ((V8Type)v8Type)
		{
			case V8Type.String:
			case V8Type.Int32:
			case V8Type.Boolean:
			case V8Type.Number:
			case V8Type.Date:
				Marshal.FreeHGlobal(marshalData);
				break;

			case V8Type.Object:
			case V8Type.Exception:
				V8ObjectData objectData = Marshal.PtrToStructure<V8ObjectData>(marshalData);

				for (int i = 0; i < objectData.propertiesCount; i++)
				{
					int propertyType = Marshal.ReadInt32(objectData.propertyTypes, i * sizeof(int));
					IntPtr propertyValue = Marshal.ReadIntPtr(objectData.propertyValues, i * Marshal.SizeOf(typeof(IntPtr)));
					IntPtr propertyName = Marshal.ReadIntPtr(objectData.propertyNames, i * Marshal.SizeOf(typeof(IntPtr)));

					FreeMarshalData(propertyValue, propertyType);
					Marshal.FreeHGlobal(propertyName);
				}

				Marshal.FreeHGlobal(objectData.propertyTypes);
				Marshal.FreeHGlobal(objectData.propertyValues);
				Marshal.FreeHGlobal(objectData.propertyNames);
				Marshal.FreeHGlobal(marshalData);

				break;

			case V8Type.Array:
				V8ArrayData arrayData = Marshal.PtrToStructure<V8ArrayData>(marshalData);

				for (int i = 0; i < arrayData.arrayLength; i++)
				{
					int itemType = Marshal.ReadInt32(arrayData.itemTypes, i * sizeof(int));
					IntPtr itemValue = Marshal.ReadIntPtr(arrayData.itemValues, i * Marshal.SizeOf(typeof(IntPtr)));

					FreeMarshalData(itemValue, itemType);
				}

				Marshal.FreeHGlobal(arrayData.itemTypes);
				Marshal.FreeHGlobal(arrayData.itemValues);
				Marshal.FreeHGlobal(marshalData);

				break;

			case V8Type.Buffer:
				V8BufferData bufferData = Marshal.PtrToStructure<V8BufferData>(marshalData);

				Marshal.FreeHGlobal(bufferData.buffer);
				Marshal.FreeHGlobal(marshalData);

				break;

			case V8Type.Null:
			case V8Type.Function:
				break;

			default:
				throw new Exception("Unsupported marshalled data type: " + v8Type);
		}
	}

	public static IntPtr MarshalCLRToV8(object clrObject, out V8Type v8Type)
	{	
		if (clrObject == null)
		{
			v8Type = V8Type.Null;
			return IntPtr.Zero;
		}

		// TODO: return strings as unicode
		else if (clrObject is string)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi((string)clrObject);
		}

		else if (clrObject is char)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi(clrObject.ToString());
		}

		else if (clrObject is bool)
		{
			v8Type = V8Type.Boolean;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(int));

			Marshal.WriteInt32(memoryLocation, ((bool)clrObject) ? 1 : 0);
			return memoryLocation;
		}

		else if (clrObject is Guid)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi(clrObject.ToString());
		}

		else if (clrObject is DateTime)
		{
			v8Type = V8Type.Date;
			DateTime dateTime = (DateTime)clrObject;

			if (dateTime.Kind == DateTimeKind.Local)
			{
				dateTime = dateTime.ToUniversalTime();
			}

			else if (dateTime.Kind == DateTimeKind.Unspecified)
			{
				dateTime = new DateTime(dateTime.Ticks, DateTimeKind.Utc);
			}

			long ticks = (dateTime.Ticks - MinDateTimeTicks) / 10000;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(double));

			WriteDouble(memoryLocation, (double)ticks);
			return memoryLocation;
		}

		else if (clrObject is DateTimeOffset)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi(clrObject.ToString());
		}

		else if (clrObject is Uri)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi(clrObject.ToString());
		}

		else if (clrObject is Int16)
		{
			v8Type = V8Type.Int32;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(int));

			Marshal.WriteInt32(memoryLocation, (int)clrObject);
			return memoryLocation;
		}

		else if (clrObject is Int32)
		{
			v8Type = V8Type.Int32;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(int));

			Marshal.WriteInt32(memoryLocation, (int)clrObject);
			return memoryLocation;
		}

		else if (clrObject is Int64)
		{
			v8Type = V8Type.Number;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(double));

			WriteDouble(memoryLocation, Convert.ToDouble((long)clrObject));
			return memoryLocation;
		}

		else if (clrObject is Double)
		{
			v8Type = V8Type.Number;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(double));

			WriteDouble(memoryLocation, (double)clrObject);
			return memoryLocation;
		}

		else if (clrObject is Single)
		{
			v8Type = V8Type.Number;
			IntPtr memoryLocation = Marshal.AllocHGlobal(sizeof(double));

			WriteDouble(memoryLocation, Convert.ToDouble((Single)clrObject));
			return memoryLocation;
		}

		else if (clrObject is Decimal)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi(clrObject.ToString());
		}

		else if (clrObject is Enum)
		{
			v8Type = V8Type.String;
			return Marshal.StringToHGlobalAnsi(clrObject.ToString());
		}

		else if (clrObject is byte[] || clrObject is IEnumerable<byte>)
		{
			v8Type = V8Type.Buffer;

			V8BufferData bufferData = new V8BufferData();
			byte[] buffer;

			if (clrObject is byte[])
			{
				buffer = (byte[]) clrObject;
			}

			else
			{
				buffer = ((IEnumerable<byte>) clrObject).ToArray();
			}

			bufferData.bufferLength = buffer.Length;
			bufferData.buffer = Marshal.AllocHGlobal(buffer.Length * sizeof(byte));

			Marshal.Copy(buffer, 0, bufferData.buffer, bufferData.bufferLength);

			IntPtr destinationPointer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(V8BufferData)));
			Marshal.StructureToPtr<V8BufferData>(bufferData, destinationPointer, false);

			return destinationPointer;
		}

		else if (clrObject is IDictionary || clrObject is ExpandoObject)
		{
			v8Type = V8Type.Object;

			IEnumerable keys;
			int keyCount;
			Func<object, object> getValue;

			if (clrObject is ExpandoObject)
			{
				IDictionary<string, object> objectDictionary = (IDictionary<string, object>)clrObject;

				keys = objectDictionary.Keys;
				keyCount = objectDictionary.Keys.Count;
				getValue = (index) => objectDictionary[index.ToString()];
			}

			else
			{
				IDictionary objectDictionary = (IDictionary)clrObject;

				keys = objectDictionary.Keys;
				keyCount = objectDictionary.Keys.Count;
				getValue = (index) => objectDictionary[index];
			}

			V8ObjectData objectData = new V8ObjectData();
			IntPtr[] propertyNames = new IntPtr[keyCount];
			int[] propertyTypes = new int[keyCount];
			IntPtr[] propertyValues = new IntPtr[keyCount];
			int pointerSize = Marshal.SizeOf(typeof(IntPtr));
			int counter = 0;
			V8Type propertyType;

			objectData.propertiesCount = keyCount;
			objectData.propertyNames = Marshal.AllocHGlobal(pointerSize * keyCount);
			objectData.propertyTypes = Marshal.AllocHGlobal(sizeof(int) * keyCount);
			objectData.propertyValues = Marshal.AllocHGlobal(pointerSize * keyCount);

			foreach (object key in keys)
			{
				propertyNames[counter] = Marshal.StringToHGlobalAnsi(key.ToString());
				propertyValues[counter] = MarshalCLRToV8(getValue(key), out propertyType);
				propertyTypes[counter] = (int)propertyType;

				counter++;
			}

			Marshal.Copy(propertyNames, 0, objectData.propertyNames, propertyNames.Length);
			Marshal.Copy(propertyTypes, 0, objectData.propertyTypes, propertyTypes.Length);
			Marshal.Copy(propertyValues, 0, objectData.propertyValues, propertyValues.Length);

			IntPtr destinationPointer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(V8ObjectData)));
			Marshal.StructureToPtr<V8ObjectData>(objectData, destinationPointer, false);

			return destinationPointer;
		}

		else if (clrObject is IEnumerable)
		{
			v8Type = V8Type.Array;

			V8ArrayData arrayData = new V8ArrayData();
			List<IntPtr> itemValues = new List<IntPtr>();
			List<int> itemTypes = new List<int>();
			V8Type itemType;

			foreach (object item in (IEnumerable)clrObject)
			{
				itemValues.Add(MarshalCLRToV8(item, out itemType));
				itemTypes.Add((int)itemType);
			}

			arrayData.arrayLength = itemValues.Count;
			arrayData.itemTypes = Marshal.AllocHGlobal(sizeof(int) * arrayData.arrayLength);
			arrayData.itemValues = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(IntPtr)) * arrayData.arrayLength);

			Marshal.Copy(itemTypes.ToArray(), 0, arrayData.itemTypes, arrayData.arrayLength);
			Marshal.Copy(itemValues.ToArray(), 0, arrayData.itemValues, arrayData.arrayLength);

			IntPtr destinationPointer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(V8ArrayData)));
			Marshal.StructureToPtr<V8ArrayData>(arrayData, destinationPointer, false);

			return destinationPointer;
		}

		else if (clrObject.GetType().FullName.StartsWith("System.Func`"))
		{
			Func<object, Task<object>> funcObject = clrObject as Func<object, Task<object>>;

			if (clrObject == null)
			{
				throw new Exception("Properties that return Func<> instances must return Func<object, Task<object>> instances");
			}

			v8Type = V8Type.Function;
			return GCHandle.ToIntPtr(GCHandle.Alloc(funcObject));
		}

		else
		{
			v8Type = clrObject is Exception ? V8Type.Exception : V8Type.Object;

			if (clrObject is Exception)
			{
				AggregateException aggregateException = clrObject as AggregateException;
					
				if (aggregateException != null && aggregateException.InnerExceptions != null && aggregateException.InnerExceptions.Count > 0)
                {
					clrObject = aggregateException.InnerExceptions[0];
                }
							
				else 
				{
					TargetInvocationException targetInvocationException = clrObject as TargetInvocationException;
							
					if (targetInvocationException != null && targetInvocationException.InnerException != null)
					{
						clrObject = targetInvocationException.InnerException;
					}
				}
			}

			List<Tuple<string, Func<object, object>>> propertyAccessors = GetPropertyAccessors(clrObject.GetType());
			V8ObjectData objectData = new V8ObjectData();
			IntPtr[] propertyNames = new IntPtr[propertyAccessors.Count];
			int[] propertyTypes = new int[propertyAccessors.Count];
			IntPtr[] propertyValues = new IntPtr[propertyAccessors.Count];
			int pointerSize = Marshal.SizeOf(typeof(IntPtr));
			int counter = 0;
			V8Type propertyType;

			objectData.propertiesCount = propertyAccessors.Count;
			objectData.propertyNames = Marshal.AllocHGlobal(pointerSize * propertyAccessors.Count);
			objectData.propertyTypes = Marshal.AllocHGlobal(sizeof(int) * propertyAccessors.Count);
			objectData.propertyValues = Marshal.AllocHGlobal(pointerSize * propertyAccessors.Count);

			foreach (Tuple<string, Func<object, object>> propertyAccessor in propertyAccessors)
			{
				propertyNames[counter] = Marshal.StringToHGlobalAnsi(propertyAccessor.Item1);
				propertyValues[counter] = MarshalCLRToV8(propertyAccessor.Item2(clrObject), out propertyType);
				propertyTypes[counter] = (int)propertyType;

				counter++;
			}

			Marshal.Copy(propertyNames, 0, objectData.propertyNames, propertyNames.Length);
			Marshal.Copy(propertyTypes, 0, objectData.propertyTypes, propertyTypes.Length);
			Marshal.Copy(propertyValues, 0, objectData.propertyValues, propertyValues.Length);

			IntPtr destinationPointer = Marshal.AllocHGlobal(Marshal.SizeOf(typeof(V8ObjectData)));
			Marshal.StructureToPtr<V8ObjectData>(objectData, destinationPointer, false);

			return destinationPointer;
		}
	}

	public static object MarshalV8ToCLR(IntPtr v8Object, V8Type objectType)
	{
		switch (objectType) 
		{
			case V8Type.String:
				return Marshal.PtrToStringAnsi(v8Object);

			case V8Type.Object:
				return V8ObjectToExpando(Marshal.PtrToStructure<V8ObjectData>(v8Object));

			case V8Type.Boolean:
				return Marshal.ReadByte(v8Object) != 0;

			case V8Type.Number:
				return ReadDouble(v8Object);

			case V8Type.Date:
				double ticks = ReadDouble(v8Object);
				return new DateTime(Convert.ToInt64(ticks) * 10000 + MinDateTimeTicks, DateTimeKind.Utc);

			case V8Type.Null:
				return null;

			case V8Type.Int32:
				return Marshal.ReadInt32(v8Object);

			case V8Type.UInt32:
				return (uint) Marshal.ReadInt32(v8Object);

			case V8Type.Function:
				NodejsFunc nodejsFunc = new NodejsFunc(v8Object);
				return nodejsFunc.GetFunc();

			case V8Type.Array:
				V8ArrayData arrayData = Marshal.PtrToStructure<V8ArrayData>(v8Object);
				int[] itemTypes = new int[arrayData.arrayLength];
				IntPtr[] itemValues = new IntPtr[arrayData.arrayLength];
				// TODO: use an array instead of a list
				List<object> array = new List<object>();

				Marshal.Copy(arrayData.itemTypes, itemTypes, 0, arrayData.arrayLength);
				Marshal.Copy(arrayData.itemValues, itemValues, 0, arrayData.arrayLength);

				for (int i = 0; i < arrayData.arrayLength; i++)
				{
					array.Add(MarshalV8ToCLR(itemValues[i], (V8Type)itemTypes[i]));
				}

				return array.ToArray();

			case V8Type.Buffer:
				V8BufferData bufferData = Marshal.PtrToStructure<V8BufferData>(v8Object);
				byte[] buffer = new byte[bufferData.bufferLength];

				Marshal.Copy(bufferData.buffer, buffer, 0, bufferData.bufferLength);

				return buffer;

			case V8Type.Exception:
				string message = Marshal.PtrToStringAnsi(v8Object);
				return new Exception(message);

			default:
				throw new Exception("Unsupported V8 object type: " + objectType + ".");
		}
	}

	private static unsafe void WriteDouble(IntPtr pointer, double value)
	{
		try
		{
			byte* address = (byte*)pointer;

			if ((unchecked((int)address) & 0x7) == 0)
			{
				*((double*)address) = value;
			}

			else
			{
				byte* valuePointer = (byte*) &value;

				address[0] = valuePointer[0];
				address[1] = valuePointer[1];
				address[2] = valuePointer[2];
				address[3] = valuePointer[3];
				address[4] = valuePointer[4];
				address[5] = valuePointer[5];
				address[6] = valuePointer[6];
				address[7] = valuePointer[7];
			}
		}

		catch (NullReferenceException)
		{
			throw new AccessViolationException();
		}
	}

	private static unsafe double ReadDouble(IntPtr pointer)
	{
		try
		{
			byte* address = (byte*)pointer;

			if ((unchecked((int)address) & 0x7) == 0)
			{
				return *((double*)address);
			}

			else
			{
				double value;
				byte* valuePointer = (byte*) &value;

				valuePointer[0] = address[0];
				valuePointer[1] = address[1];
				valuePointer[2] = address[2];
				valuePointer[3] = address[3];
				valuePointer[4] = address[4];
				valuePointer[5] = address[5];
				valuePointer[6] = address[6];
				valuePointer[7] = address[7];

				return value;
			}
		}

		catch (NullReferenceException)
		{
			throw new AccessViolationException();
		}
	}

	private static ExpandoObject V8ObjectToExpando(V8ObjectData v8Object)
	{
		ExpandoObject expando = new ExpandoObject();
		IDictionary<string, object> expandoDictionary = (IDictionary<string, object>) expando;
		int[] propertyTypes = new int[v8Object.propertiesCount];
		IntPtr[] propertyNamePointers = new IntPtr[v8Object.propertiesCount];
		IntPtr[] propertyValuePointers = new IntPtr[v8Object.propertiesCount];

		Marshal.Copy(v8Object.propertyTypes, propertyTypes, 0, v8Object.propertiesCount);
		Marshal.Copy(v8Object.propertyNames, propertyNamePointers, 0, v8Object.propertiesCount);
		Marshal.Copy(v8Object.propertyValues, propertyValuePointers, 0, v8Object.propertiesCount);

		for (int i = 0; i < v8Object.propertiesCount; i++) 
		{
			IntPtr propertyNamePointer = propertyNamePointers [i];
			IntPtr propertyValuePointer = propertyValuePointers [i];
			string propertyName = Marshal.PtrToStringAnsi(propertyNamePointer);

			expandoDictionary[propertyName] = MarshalV8ToCLR(propertyValuePointer, (V8Type) propertyTypes[i]);
		}

		return expando;
	}

	internal static void DebugMessage(string message, params object[] parameters)
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

	private static List<Tuple<string, Func<object, object>>> GetPropertyAccessors(Type type)
	{
		if (TypePropertyAccessors.ContainsKey(type))
		{
			return TypePropertyAccessors[type];
		}

		List<Tuple<string, Func<object, object>>> propertyAccessors = new List<Tuple<string, Func<object, object>>>();

		foreach (PropertyInfo propertyInfo in type.GetProperties(BindingFlags.Instance | BindingFlags.Public))
		{
			ParameterExpression instance = Expression.Parameter(typeof(object));
			UnaryExpression instanceConvert = Expression.TypeAs(instance, type);
			MemberExpression property = Expression.Property(instanceConvert, propertyInfo);
			UnaryExpression propertyConvert = Expression.TypeAs(property, typeof(object));

			propertyAccessors.Add(new Tuple<string, Func<object, object>>(propertyInfo.Name, (Func<object, object>) Expression.Lambda(propertyConvert, instance).Compile()));
		}

		foreach (FieldInfo fieldInfo in type.GetFields(BindingFlags.Instance | BindingFlags.Public))
		{
			ParameterExpression instance = Expression.Parameter(typeof(object));
			UnaryExpression instanceConvert = Expression.TypeAs(instance, type);
			MemberExpression field = Expression.Field(instanceConvert, fieldInfo);
			UnaryExpression fieldConvert = Expression.TypeAs(field, typeof(object));

			propertyAccessors.Add(new Tuple<string, Func<object, object>>(fieldInfo.Name, (Func<object, object>) Expression.Lambda(fieldConvert, instance).Compile()));
		}

		if (typeof(Exception).IsAssignableFrom(type) && !propertyAccessors.Any(a => a.Item1 == "Name"))
		{
			propertyAccessors.Add(new Tuple<string, Func<object, object>>("Name", (o) => type.FullName));
		}

		TypePropertyAccessors[type] = propertyAccessors;

		return propertyAccessors;
	}
}
