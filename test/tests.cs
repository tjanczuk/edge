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
using System.Text;
using System.Threading.Tasks;

#pragma warning disable 1998

namespace Edge.Tests
{
    public class Startup
    {
        string ValidateMarshalNodeJsToNet(object input, bool expectFunction) 
        {
            string result = "yes"; 
            IDictionary<string, object> data = input as IDictionary<string, object>;
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
            }
            catch (Exception e)
            {
                result = e.ToString();
            }

            return result;
        }

        public Task<object> Invoke(object input)
        {
            return Task.FromResult<object>(".NET welcomes " + input.ToString());
        }

        public Task<object> MarshalIn(object input)
        {
            string result = ValidateMarshalNodeJsToNet(input, true);
            return Task.FromResult<object>(result);
        }

        public Task<object> MarshalBack(object input)
        {
            var result = new {
                a = 1,
                b = 3.1415,
                c = "foo",
                d = true,
                e = false,
                f = new byte[10],
                g = new object[] { 1, "foo" },
                h = new { a = "foo", b = 12 },
                i = (Func<object,Task<object>>)(async (i) => { return i; })
            };

            return Task.FromResult<object>(result);
        }       

        public Task<object> NetExceptionTaskStart(object input)
        {
            throw new Exception("Test .NET exception");
        }                   

        public Task<object> NetExceptionCLRThread(object input)
        {
            Task<object> task = new Task<object>(() =>
            {
                throw new Exception("Test .NET exception");
            });

            task.Start();

            return task;            
        }                   

        public async Task<object> InvokeBack(object input)
        {
            Func<object, Task<object>> hello = (Func<object, Task<object>>)(((IDictionary<string, object>)input)["hello"]);
            var result = await hello(".NET");
            return result;
        }

        public async Task<object> MarshalInFromNet(object input)
        {
            Func<object, Task<object>> hello = (Func<object, Task<object>>)(((IDictionary<string, object>)input)["hello"]);
            var payload = new {
                a = 1,
                b = 3.1415,
                c = "foo",
                d = true,
                e = false,
                f = new byte[10],
                g = new object[] { 1, "foo" },
                h = new { a = "foo", b = 12 },
                i = (Func<object,Task<object>>)(async (i) => { return i; })
            };          
            var result = await hello(payload);
            return result;
        }

        public async Task<object> MarshalBackToNet(object input)
        {
            Func<object, Task<object>> hello = (Func<object, Task<object>>)(((IDictionary<string, object>)input)["hello"]);     
            var payload = await hello(null);
            string result = ValidateMarshalNodeJsToNet(payload, false);
            return result;
        }        

        public async Task<object> MarshalException(object input)
        {
            string result = "No exception was thrown";
            Func<object, Task<object>> hello = (Func<object, Task<object>>)(((IDictionary<string, object>)input)["hello"]); 
            try 
            {   
                await hello(null);
            }
            catch (Exception e) 
            {
                result = e.ToString();
            }

            return result;
        }       

        private WeakReference weakRefToNodejsFunc;
        public Task<object> InvokeBackAfterCLRCallHasFinished(object input)
        {
            var trace = new List<string>();
            trace.Add("InvokeBackAfterCLRCallHasFinished#EnteredCLR");
            Func<object, Task<object>> callback = (Func<object, Task<object>>)(((IDictionary<string, object>)input)["eventCallback"]);

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

        public Task<object> EnsureNodejsFuncIsCollected(object input) {

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
    }
}
