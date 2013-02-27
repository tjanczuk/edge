using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

namespace Owinjs
{
    static class Utils
    {
        public static T ReadBody<T>(IDictionary<string, object> env)
        {
            Stream body = (Stream)env["owin.RequestBody"];
            string request = new StreamReader(body).ReadToEnd();
            JavaScriptSerializer serializer = new JavaScriptSerializer();
            return serializer.Deserialize<T>(request);
        }

        public static void WriteBody(object value, IDictionary<string, object> env)
        {
            JavaScriptSerializer serializer = new JavaScriptSerializer();
            string response = serializer.Serialize(value);
            IDictionary<string, string[]> responseHeaders = ((IDictionary<string, string[]>)env["owin.ResponseHeaders"]);
            responseHeaders.Add("Content-Type", new string[] { "application/json" });
            responseHeaders.Add("Content-Length", new string[] { Encoding.UTF8.GetByteCount(response).ToString() });
            StreamWriter w = new StreamWriter((Stream)env["owin.ResponseBody"]);
            w.Write(response);
            w.Flush();
            env["owin.ResponseStatusCode"] = 200;
        }
    }
}
