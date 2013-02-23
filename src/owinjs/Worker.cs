using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace Owinjs
{
    public abstract class Worker
    {
        protected Worker() { }

        public Task Invoke(IDictionary<string, object> env)
        {
            Task task = new Task(() =>
            {
                Stream body = (Stream)env["owin.RequestBody"];
                string request = new StreamReader(body).ReadToEnd();
                JavaScriptSerializer serializer = new JavaScriptSerializer();
                Dictionary<string, string> input = serializer.Deserialize<Dictionary<string, string>>(request);
                IDictionary<string, string> result = this.Execute(input);
                string response = serializer.Serialize(result);
                IDictionary<string, string[]> responseHeaders = ((IDictionary<string, string[]>)env["owin.ResponseHeaders"]);
                responseHeaders.Add("Content-Type", new string[] { "application/json" });
                responseHeaders.Add("Content-Length", new string[] { Encoding.UTF8.GetByteCount(response).ToString() });
                StreamWriter w = new StreamWriter((Stream)env["owin.ResponseBody"]);
                w.Write(response);
                w.Flush();
                env["owin.ResponseStatusCode"] = 200;
            });

            task.Start();

            return task;
        }

        protected abstract IDictionary<string, string> Execute(IDictionary<string, string> input);
    }
}
