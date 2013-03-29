// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var createController = edge.func(function () {/*
    using System;
    using System.Threading;
    using System.Threading.Tasks;

    public class Startup
    {
        static TaskCompletionSource<object> tcs;

        public async Task<object> Invoke(object input)
        {
            return new {
                yieldControl = (Func<object,Task<object>>)((i) => {
                    return Startup.AcquireControl();
                }),
                regainControl = (Func<object,Task<object>>)(async (i) => {
                    Startup.ReleaseControl();
                    return null;
                })
            };
        }

        static Task<object> AcquireControl()
        {
            // single threaded; always called on V8 thread

            if (tcs != null)
            {
                throw new InvalidOperationException("CLR already controls the lifetime of the process.");
            }

            TaskCompletionSource<object> tmp = new TaskCompletionSource<object>();
            tcs = tmp;
            return tmp.Task;
        }

        public static void ReleaseControl()
        {
            // multi-threaded; can be called from V8 or one of many CLR threads

            TaskCompletionSource<object> tmp = Interlocked.Exchange(ref tcs, null);
            if (tmp != null)
            {
                tmp.SetResult(null);
            }
        }
    }
*/});

// yield control over process lifetime to CLR
var controller = createController(null, true);
controller.yieldControl();
console.log('Control over process lifetime yielded to CLR, the process will not exit...');

// at this point, the process will not exit until one of the following happens:
// - node.js calls controller.regainControl(), or
// - CLR calls Startup.ReleaseControl()

// controller.regainControl();