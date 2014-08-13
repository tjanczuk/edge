using System;
using System.Dynamic;
using System.Threading.Tasks;
using System.Reflection;
using System.Collections.Generic;

public static class MonoEmbedding
{
    static Int64 MinDateTimeTicks = 621355968000000000; // (new DateTime(1970, 1, 1, 0, 0, 0)).Ticks;

    // dummy Main so we can exec from mono
    static public int Main()
    {
        return 0;
    }

    static public Exception NormalizeException(Exception e)
    {
        AggregateException aggregate = e as AggregateException;
        if (aggregate != null && aggregate.InnerExceptions.Count == 1)
        {
            e = aggregate.InnerExceptions[0];
        }
        else {
            TargetInvocationException target = e as TargetInvocationException;
            if (target != null && target.InnerException != null)
            {
                e = target.InnerException;
            }
        }

        return e;
    }

    static public Type GetIDictionaryStringObjectType()
    {
        return typeof(IDictionary<string, Object>);
    }

    static public Type GetUriType()
    {
        return typeof(Uri);
    }

    static public Func<Object, Task<Object>> GetFunc(string assemblyFile, string typeName, string methodName)
    {
        var assembly = Assembly.LoadFrom(assemblyFile);
        var wrap = ClrFuncReflectionWrap.Create(assembly, typeName, methodName);
        return new Func<Object, Task<Object>>(wrap.Call);
    }

    static public DateTime CreateDateTime(double ticks)
    {
        return new DateTime((Int64)ticks * 10000 + MinDateTimeTicks, DateTimeKind.Utc);
    }

    static public ExpandoObject CreateExpandoObject()
    {
        return new ExpandoObject();
    }

    static public double GetDateValue(DateTime dt)
    {
        if (dt.Kind == DateTimeKind.Local)
            dt = dt.ToUniversalTime();
        else if (dt.Kind == DateTimeKind.Unspecified)
            dt = new DateTime(dt.Ticks, DateTimeKind.Utc);
        Int64 value = (dt.Ticks - MinDateTimeTicks) / 10000;

        return (double)value;
    }

    static public object[] IDictionaryToFlatArray(object dictionary)
    {
        var list = new List<object>();
        IDictionary<string, object> dso = dictionary as IDictionary<string, object>;
        if (dso != null)
        {
            foreach (var kv in dso)
            {
                list.Add(kv.Key);
                list.Add(kv.Value);
            }
        }
        else
        {
            System.Collections.IDictionary d = dictionary as System.Collections.IDictionary;
            if (d != null)
            {
                foreach (System.Collections.DictionaryEntry kv in d)
                {
                    if ((kv.Key as string) != null)
                    {
                        list.Add(kv.Key);
                        list.Add(kv.Value);
                    }
                }
            }
            else
            {
                throw new InvalidOperationException("Expected a dictionary.");
            }
        }

        return list.ToArray();
    }

    static public object[] IEnumerableToArray(System.Collections.IEnumerable enumerable)
    {
        var list = new List<object>();
        foreach (var item in enumerable)
            list.Add(item);

        return list.ToArray();
    }

    static public Type GetFuncType()
    {
        return typeof(Func<Object, Task<Object>>);
    }

    static public void edgeAppCompletedOnCLRThread(Task<object> task, object state)
    {
        var context = (ClrFuncInvokeContext)state;
        context.CompleteOnCLRThread(task);
    }

    static public void ContinueTask(Task<object> task, object state)
    {
        // Will complete asynchronously. Schedule continuation to finish processing.
        task.ContinueWith(new Action<Task<object>, object>(edgeAppCompletedOnCLRThread), state);
    }

    static public string ObjectToString(object o)
    {
        return o.ToString();
    }

    static public double Int64ToDouble(long i64)
    {
        return (double)i64;
    }

    static public string TryConvertPrimitiveOrDecimal(object obj)
    {
        Type t = obj.GetType();
        if (t.IsPrimitive || typeof(Decimal) == t)
        {
            IConvertible c = obj as IConvertible;
            return c == null ? obj.ToString() : c.ToString();
        }
        else 
        {
            return null;
        }
    }
}
