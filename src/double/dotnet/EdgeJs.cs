using System;
using System.ComponentModel;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Threading;
using System.Threading.Tasks;

namespace EdgeJs
{
    public class Edge
    {
        static object syncRoot = new object();
        static bool initialized;
        static Func<object, Task<object>> compileFunc;
        static ManualResetEvent waitHandle = new ManualResetEvent(false);

        static string assemblyDirectory;
        static string AssemblyDirectory
        {
            get
            {
                if (assemblyDirectory == null)
                {
                    assemblyDirectory = Environment.GetEnvironmentVariable("EDGE_BASE_DIR");
                    if (string.IsNullOrEmpty(assemblyDirectory))
                    {
                        string codeBase = typeof(Edge).Assembly.CodeBase;
                        UriBuilder uri = new UriBuilder(codeBase);
                        string path = Uri.UnescapeDataString(uri.Path);
                        assemblyDirectory = Path.GetDirectoryName(path);
                    }
                }

                return assemblyDirectory;
            }
        }

        [EditorBrowsable(EditorBrowsableState.Never)]
        public Task<object> InitializeInternal(object input)
        {
            compileFunc = (Func<object, Task<object>>)input;
            initialized = true;
            waitHandle.Set();

            return Task<object>.FromResult((object)null);
        }

        [DllImport("node.dll", EntryPoint = "#585", CallingConvention = CallingConvention.Cdecl)]
        static extern int NodeStart(int argc, string[] argv);

        [DllImport("kernel32.dll", EntryPoint = "LoadLibrary")]
        static extern int LoadLibrary([MarshalAs(UnmanagedType.LPStr)] string lpLibFileName);

        public static Func<object,Task<object>> Func(string code)
        {
            if (!initialized)
            {
                lock (syncRoot)
                {
                    if (!initialized)
                    {
                        if (IntPtr.Size == 4)
                        {
                            LoadLibrary(AssemblyDirectory + @"\edge\x86\node.dll");
                        }
                        else if (IntPtr.Size == 8)
                        {
                            LoadLibrary(AssemblyDirectory + @"\edge\x64\node.dll");
                        }
                        else
                        {
                            throw new InvalidOperationException(
                                "Unsupported architecture. Only x86 and x64 are supported.");
                        }

                        Thread v8Thread = new Thread(() => 
                        {
                            NodeStart(2, new string[] { "node", AssemblyDirectory + "\\edge\\double_edge.js" });
                            waitHandle.Set();
                        });

                        v8Thread.IsBackground = true;
                        v8Thread.Start();
                        waitHandle.WaitOne();

                        if (!initialized)
                        {
                            throw new InvalidOperationException("Unable to initialize Node.js runtime.");
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
    }
}
