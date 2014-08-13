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

#pragma warning disable 1998

namespace Edge.Tests
{
    public class Startup
    {
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

        public Task<object> NetExceptionCLRThread(dynamic input)
        {
            Task<object> task = new Task<object>(() =>
            {
                throw new Exception("Test .NET exception");
            });

            task.Start();

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
