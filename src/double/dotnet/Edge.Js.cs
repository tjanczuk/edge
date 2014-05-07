using System;
using System.ComponentModel;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace Edge.Js
{
    public class Edge
    {
        static object syncRoot = new object();
        static bool initialized;
        static TaskCompletionSource<object> tcs;
        static Func<object, Task<object>> compileFunc;

        static string AssemblyDirectory
        {
            get
            {
                string codeBase = Assembly.GetExecutingAssembly().CodeBase;
                UriBuilder uri = new UriBuilder(codeBase);
                string path = Uri.UnescapeDataString(uri.Path);
                return Path.GetDirectoryName(path);
            }
        }

        [EditorBrowsable(EditorBrowsableState.Never)]
        public Task<object> InitializeInternal(object input)
        {
            compileFunc = (Func<object, Task<object>>)input;
            tcs = new TaskCompletionSource<object>();
            initialized = true;

            Task.Run(async () =>
            {
                await compileFunc("SOME JS CODE TO COMPILE");
            });

            return tcs.Task;
        }

        [DllImport(@"node86.dll, Edge.Js", EntryPoint = "#585", CallingConvention = CallingConvention.Cdecl)]
        static extern int NodeStart86(int argc, string[] argv);

        [DllImport(@"node64.dll, Edge.Js", EntryPoint = "#585", CallingConvention = CallingConvention.Cdecl)]
        static extern int NodeStart64(int argc, string[] argv);

        public static Func<object,Task<object>> Func(string code)
        {
            if (!initialized)
            {
                lock (syncRoot)
                {
                    if (!initialized)
                    {
                        var boostrap = @"
                            __dirname = process.argv[1];
                            process.env['EDGE_NATIVE'] = process.argv[1]
                                + (process.arch === 'x64' ? '\\x64\\edge.node' : '\\x86\\edge.node');
                            var edge = require(process.argv[1] + '\\edge.js');
                            var initialize = edge.func({
                                assemblyFile: 'Edge.Js.dll',
                                typeName: 'Edge.Js.Edge',
                                methodName: 'InitializeInternal'
                            });
                            var compileFunc = function (data, callback) {
                                console.log(data);
                                callback();
                            }
                            initialize(compileFunc, function (error, data) {
                                if (error) throw error;
                                // At this point V8 thread will terminate unless there 
                                // are other pending events or active listeners. 
                            });
                        ";

                        if (IntPtr.Size == 4)
                        {
                            NodeStart86(4, new string[] { "node", "-e", boostrap, AssemblyDirectory });
                        }
                        else if (IntPtr.Size == 8)
                        {
                            NodeStart64(4, new string[] { "node", "-e", boostrap, AssemblyDirectory });
                        }
                        else
                        {
                            throw new InvalidOperationException("Unsupported architecture. Only x86 and x64 are supported.");
                        }
                    }
                }
            }

            if (compileFunc == null)
            {
                throw new InvalidOperationException("Edge.Func cannot be used after Edge.Close had been called.");
            }

            var task = compileFunc(code);
            task.Wait();
            return (Func<object, Task<object>>)task.Result;
        }

        public static void Close()
        {
            if (initialized && compileFunc != null)
            {
                lock (syncRoot)
                {
                    if (compileFunc != null)
                    {
                        tcs.SetResult(null);
                        compileFunc = null;
                        tcs = null;
                    }
                }
            }
        }
    }
}
