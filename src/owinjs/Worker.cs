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
                Dictionary<string, string> input = Utils.ReadBody<Dictionary<string, string>>(env);
                IDictionary<string, string> result = this.Execute(input);
                Utils.WriteBody(result, env);
            });

            task.Start();

            return task;
        }

        protected abstract IDictionary<string, string> Execute(IDictionary<string, string> input);
    }
}
