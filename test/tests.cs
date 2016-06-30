/**
 * Portions Copyright (c) Microsoft Corporation. All rights reserved. 
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0  
 *
 * THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION 
 * ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR 
 * PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
 *
 * See the Apache Version 2.0 License for specific language governing 
 * permissions and limitations under the License.
 */
using System;
using System.Collections.Generic;
using System.Dynamic;
using System.Text;
using System.Threading.Tasks;
#if NETCOREAPP1_0
using Newtonsoft.Json;
using System.Reflection;
using Microsoft.Extensions.DependencyModel;
using System.Linq;
using System.Net;
using Microsoft.AspNetCore.Server.Kestrel.Internal.Networking;
#endif

#pragma warning disable 1998

namespace Edge.Tests
{
    public class Startup
    {
        static void Main(string[] args)
        {
            
        }

        string ValidateMarshalNodeJsToNet(dynamic input, bool expectFunction)
        {
            string result = "yes"; 
            var data = (IDictionary<string, object>)input;
            try 
            {
                int a = (int)data["a"];
                if (a != 1) throw new Exception("a is not 1");
                double b = (double)data["b"];
                if (b != 3.1415) throw new Exception("b is not 3.1415");
                string c = (string)data["c"];
                if (c != "foo") throw new Exception("c is not foo");
                bool d = (bool)data["d"];
                if (d != true) throw new Exception("d is not true");
                bool e = (bool)data["e"];
                if (e != false) throw new Exception("e is not false");
                byte[] f = (byte[])data["f"];
                if (f.Length != 10) throw new Exception("f.length is not 10");
                object[] g = (object[])data["g"];
                if (g.Length != 2) throw new Exception("g.length is not 2");
                if ((int)g[0] != 1) throw new Exception("g[0] is not 1");
                if ((string)g[1] != "foo") throw new Exception("g[1] is not foo");
                IDictionary<string, object> h = (IDictionary<string,object>)data["h"];
                if ((string)h["a"] != "foo") throw new Exception("h.a is not foo");
                if ((int)h["b"] != 12) throw new Exception("h.b is not 12");
                if (expectFunction)
                {
                    var i = data["i"] as Func<object, Task<object>>;
                    if (i == null) throw new Exception("i is not a Func<object,Task<object>>");
                }
                if ((DateTime)data["j"] != new DateTime(2013, 08, 30)) throw new Exception("j is not DateTime(2013,08,30)");
            }
            catch (Exception e)
            {
                result = e.ToString();
            }

            try
            {
                int a = input.a;
                if (a != 1) throw new Exception("dynamic a is not 1");
                double b = input.b;
                if (b != 3.1415) throw new Exception("dynamic b is not 3.1415");
                string c = input.c;
                if (c != "foo") throw new Exception("dynamic c is not foo");
                bool d = input.d;
                if (d != true) throw new Exception("dynamic d is not true");
                bool e = input.e;
                if (e != false) throw new Exception("dynamic e is not false");
                byte[] f = input.f;
                if (f.Length != 10) throw new Exception("dynamic f.length is not 10");
                dynamic[] g = input.g;
                if (g.Length != 2) throw new Exception("dynamic g.length is not 2");
                if ((int)g[0] != 1) throw new Exception("dynamic g[0] is not 1");
                if ((string)g[1] != "foo") throw new Exception("dynamic g[1] is not foo");
                dynamic h = input.h;
                if ((string)h.a != "foo") throw new Exception("dynamic h.a is not foo");
                if ((int)h.b != 12) throw new Exception("dynamic h.b is not 12");
                if (expectFunction)
                {
                    var i = input.i as Func<object, Task<object>>;
                    if (i == null) throw new Exception("dynamic i is not a Func<object,Task<object>>");
                }
                if ((DateTime)input.j != new DateTime(2013, 08, 30)) throw new Exception("dynamic j is not DateTime(2013,08,30)");
            }
            catch (Exception e)
            {
                result = e.ToString();
            }

            return result;
        }

        public Task<object> Invoke(dynamic input)
        {
            return Task.FromResult<object>(".NET welcomes " + input);
        }

        public Task<object> MarshalIn(dynamic input)
        {
            string result = ValidateMarshalNodeJsToNet(input, true);
            return Task.FromResult<object>(result);
        }

        public Task<object> MarshalBack(dynamic input)
        {
            dynamic result = new ExpandoObject();
            result.a = 1;
            result.b = 3.1415;
            result.c = "foo";
            result.d = true;
            result.e = false;
            result.f = new byte[10];
            result.g = new object[] { 1, "foo" };
            result.h = new { a = "foo", b = 12 };
            result.i = (Func<object,Task<object>>)(async (i) => { return i; });
            result.j = new DateTime(2013, 08, 30);

            return Task.FromResult<object>(result);
        }       

        public Task<object> NetExceptionTaskStart(dynamic input)
        {
            throw new Exception("Test .NET exception");
        }    

        public async Task<object> DelayedReturn(dynamic input)
        {
            await Task.Delay(1000);
            return null;
        }

        public Task<object> ReturnAsyncLambda(dynamic input)
        {
            return Task.FromResult((object)(new Func<object,Task<object>>(async (i) => { return ".NET welcomes " + i.ToString(); })));
        }

        public Task<object> NetExceptionCLRThread(dynamic input)
        {
            Task<object> task = Task.Delay(200).ContinueWith(new Func<Task, object>((antecedant) =>
            {
                throw new Exception("Test .NET exception");
            }));

            return task;            
        }                   

        public async Task<object> InvokeBack(dynamic input)
        {
            var result = await input.hello(".NET");
            return result;
        }

        public async Task<object> MarshalInFromNet(dynamic input)
        {
            dynamic payload = new ExpandoObject();
            payload.a = 1;
            payload.b = 3.1415;
            payload.c = "foo";
            payload.d = true;
            payload.e = false;
            payload.f = new byte[10];
            payload.g = new object[] { 1, "foo" };
            payload.h = new { a = "foo", b = 12 };
            payload.i = (Func<object,Task<object>>)(async (i) => { return i; });
            payload.j = new DateTime(2013, 08, 30);

            var result = await input.hello(payload);
            return result;
        }

        public async Task<object> MarshalBackToNet(dynamic input)
        {
            var payload = await input.hello(null);
            string result = ValidateMarshalNodeJsToNet(payload, false);
            return result;
        }        

        public async Task<object> MarshalException(dynamic input)
        {
            string result = "No exception was thrown";
            try 
            {   
                await input.hello(null);
            }
            catch (Exception e) 
            {
                result = e.ToString();
            }

            return result;
        }       

        private WeakReference weakRefToNodejsFunc;
        public Task<object> InvokeBackAfterCLRCallHasFinished(dynamic input)
        {
            var trace = new List<string>();
            trace.Add("InvokeBackAfterCLRCallHasFinished#EnteredCLR");
            Func<object, Task<object>> callback = input.eventCallback;

            // The target of the callback function is the ref to the NodejsFunc acting as
            // the proxy for the JS function to call.
            weakRefToNodejsFunc = new WeakReference(callback.Target);

            trace.Add("InvokeBackAfterCLRCallHasFinished#LeftCLR");

            var result = new TaskCompletionSource<object>();
            result.SetResult(trace);

            // Now simulate an event callback after this CLR method has finished.
            Task.Delay(50).ContinueWith(async (value) => {
                GC.Collect();
                // This throws an exception if the weak reference is not
                // pointing to an existing object.
                GC.GetGeneration(weakRefToNodejsFunc);
                await callback("InvokeBackAfterCLRCallHasFinished#CallingCallbackFromDelayedTask");
            });

            return result.Task;
        }

        public Task<object> EnsureNodejsFuncIsCollected(dynamic input) {

            var succeed = false;
            GC.Collect();
            try
            {
                // Throws an exception if the object the weak reference
                // points to is GCed.
                GC.GetGeneration(weakRefToNodejsFunc);
            }
            catch(Exception)
            {
                // The NodejsFunc is GCed.
                succeed = true;
            }
            weakRefToNodejsFunc = null;            

            var result = new TaskCompletionSource<object>();
            result.SetResult(succeed);
            return result.Task;
        }

        public async Task<object> MarshalObjectHierarchy(dynamic input)
        {
            var result = new B();
            result.A_field = "a_field";
            result.A_prop = "a_prop";
            result.B_field = "b_field";
            result.B_prop = "b_prop";

            return result;
        }

        public Task<object> NetStructuredExceptionCLRThread(dynamic input)
        {
            Task<object> task = Task.Delay(200).ContinueWith(new Func<Task, object>((antecedant) =>
                {
		    throw new InvalidOperationException(
                        "Outer exception", 
                        new ArgumentException("Inner exception", "input"));
                }));

            return task; 
        }

        public async Task<object> MarshalEmptyBuffer(dynamic input)
        {
            return ((byte[])input).Length == 0;
        }

        public async Task<object> ReturnEmptyBuffer(dynamic input)
        {
            return new byte[] {};
        }

        public async Task<object> ReturnInput(dynamic input)
        {
            return input;
        }

        public async Task<object> ReturnLambdaWithClosureOverState(dynamic input)
        {
            var k = (int)input;
            return (Func<object,Task<object>>)(async (i) => { return ++k; });
        }

        public async Task<object> ReturnLambdaThatThrowsException(dynamic input)
        {
            return (Func<object,Task<object>>)(async (i) => { throw new Exception("Test .NET exception"); });
        }

        public async Task<object> ReturnLambdaThatThrowsAsyncException(dynamic input)
        {
            return (Func<object,Task<object>>)((i) => { 
                return Task.Delay(200).ContinueWith(new Func<Task, object>((antecedant) => {
                    throw new Exception("Test .NET exception");
                }));
            });
        }

        public async Task<object> LargeNumberOfConcurrentCallbacks(dynamic input)
        {
            Func<object, Task<object>> rowCallback = (Func<object, Task<object>>)input;
            IList<Task<object>> rowEvents = new List<Task<object>>();

            for (int i = 0; i < 5000; ++i)
            {
                IDictionary<string, object> row = new Dictionary<string, object>();
                for (int j = 0; j < 25; j++) row.Add(j.ToString(), j);
                rowEvents.Add(rowCallback(row));
            }

            await Task.WhenAll(rowEvents);

            return rowEvents.Count;
        }

        public async Task<object> CallbackToNodejs(IDictionary<string, object> input) 
        {
            Func<object, Task<object>> callin = (Func<object, Task<object>>)input["callin"];
            await callin(input["payload"]);
            return input["payload"];
        }

        public async Task<object> DoubleLayerCallbackToNodejs(dynamic input)
        {
            Func<object,Task<object>> func1 = (Func<object,Task<object>>)input;
            Func<object,Task<object>> func2 = (Func<object,Task<object>>)(await func1(null));
            return await func2(null);
        }

        public async Task<object> ThrowExceptionMarshallingForCallback(Func<object, Task<object>> callin) 
        {
            try 
            {
                await callin(new BadPerson());
                return "Unexpected success";
            }
            catch (Exception e)
            {
                while (e.InnerException != null)
                    e = e.InnerException;

                if (e.Message == "I have no name")
                {
                    return "Expected failure";
                }   
                else 
                {
                    return "Unexpected failure: " + e.ToString();
                }
            }
        }

        public async Task<object> ThrowExceptionMarshallingForAsyncReturn(object input) 
        {
            await Task.Delay(200);
            return new BadPerson();
        }

        public async Task<object> ThrowExceptionMarshallingForReturn(object input) 
        {
            return new BadPerson();
        }

#if NETCOREAPP1_0
        public async Task<object> CorrectVersionOfNewtonsoftJsonUsed(object input)
        {
            return typeof(JsonConvert).GetTypeInfo().Assembly.GetName().Version.ToString();
        }

        public async Task<object> CanUseDefaultDependencyContext(object input)
        {
            return DependencyContext.Default.RuntimeLibraries.Single(l => l.Name == "Newtonsoft.Json").Version.ToString();
        }

        public async Task<object> CanUseNativeLibraries(object input)
        {
            Libuv libuv = new Libuv();
            return libuv.loop_size();
        }
#endif

        public class BadPerson 
        {
            public string Name 
            {
                get 
                {
                    throw new InvalidOperationException("I have no name");
                }
            }
        }

        class A 
        {
            public string A_field;
            public string A_prop { get; set; }
        }

        class B : A 
        {
            public string B_field;
            public string B_prop { get; set; }
        }
    }
}
