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

    }
}
