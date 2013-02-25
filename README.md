Hosting .NET OWIN applications in node.js
====

Owin allows you to host .NET code in a node.js application. It allows you to:
* implement express.js handlers and connect middleware for node.js application using .NET 4.5,  
* implement CPU-bound computations in .NET and run them in-process with node.js application without blocking the event loop,  
* use C# and .NET instead of writing native node.js extensions in C/C++ and Win32 to access Windows specific functionality from a node.js application. 

Owin is a native node.js module for Windows. It hosts [OWIN](http://owin.org/) handlers written in .NET 4.5 (think C#) in a node.js application. Owin allows integration of .NET code into express.js applications by providing a connect wrapper around OWIN .NET handlers. Owin also provides an easy way to run CPU bound computations implemented in .NET without blocking the node.js event loop. 

Read more about the background and motivations of the project [here](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html).

## What you need

* Windows x64  
* node.js 0.8.x x64 (developed and tested with [v0.8.19](http://nodejs.org/dist/v0.8.19/))  
* [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653)  

## How to: express.js request handler in .NET

First get the owin module and express.js:

```
npm install owin
npm install express
```

Implement your Startup.cs [OWIN](http://owin.org/) handler in C# as follows:

```c#
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;

namespace OwinHelloWorld
{
    public class Startup
    {
        public Task Invoke(IDictionary<string, object> env)
        {
            env["owin.ResponseStatusCode"] = 200;
            ((IDictionary<string, string[]>)env["owin.ResponseHeaders"]).Add(
                "Content-Type", new string[] { "text/html" });
            StreamWriter w = new StreamWriter((Stream)env["owin.ResponseBody"]);
            w.Write("Hello, from C#. Time on server is " + DateTime.Now.ToString());
            w.Flush();

            return Task.FromResult<object>(null);
        }
    }
}
```

Compile it to OwinHelloWorld.dll with:

```
csc /target:library /out:OwinHelloWorld.dll Startup.cs
```

Then in your server.js, create an express.js application and register two handlers. One is implemented in JavaScript, and the other uses the Owin module to load the .NET handler in OwinHelloWorld.dll:

```javascript
var owin = require('./lib/owin.js')
	, express = require('express');

var app = express();
app.use(express.bodyParser());
app.all('/jazz', owin('OwinHelloWorld.dll'))
app.all('/rocknroll', function (req, res) {
	res.send(200, 'Hello from JavaScript! Time on server ' + new Date());
});

app.listen(3000);
```

Now start your server:

```
node server.js
```

(Make sure OwinHelloWorld.dll is in the current directory, or specify the full file path to it in the call to the `owin()` function in your server.js)

Now open the web browser and navigate to `http://localhost:3000/jazz`. Welcome to .NET! 

![jazz](https://f.cloud.github.com/assets/822369/190582/79ad0574-7efc-11e2-9d61-227ab70c37b4.PNG)

Now navigate to `http://localhost:3000/rocknroll`. Hello JavaScript!

![rocknroll](https://f.cloud.github.com/assets/822369/190583/8151f190-7efc-11e2-92ab-dcbfffd96f35.PNG)

## How to: CPU-bound in-process worker

This is how you can execute CPU-bound code written in .NET within the node.js process without blocking the node.js event loop. Read more about the scenario and goals [here](http://tomasz.janczuk.org/2013/02/cpu-bound-workers-for-nodejs.html). 

First install the owin module with:

```
npm install owin
```

Then implement your CPU bound code in .NET and save it to Startup.cs:

```c#
namespace CalculateBudget
{
    public class Startup : Owinjs.Worker
    {
        protected override IDictionary<string, string> Execute(IDictionary<string, string> input)
        {
            int revenue = int.Parse(input["revenue"]);
            int cost = int.Parse(input["cost"]);
            int income = revenue - cost;

            Thread.Sleep(5000); // pretend it takes a long time

            return new Dictionary<string, string> { { "income", income.ToString() } };
        }
    }
}
```

The `Execute` method can conatin any long-running, CPU-blocking code. You can accept a dictionary of strings as input and return another dictionary of strings as the result of the computation. 

Now compile the .NET into a `CalculateBudget.dll` (the name of the assembly should by convention match the namespace name and reference the `Owinjs.dll` included with the owin module:

```
copy node_modules\owin\lib\clr\Owinjs.dll
csc /target:library /r:Owinjs.dll /out:CalculateBudget.dll Startup.cs
```

Lastly implement your node.js application that imports the owin module and calls into the blocking .NET code from `CalculateBudget.dll`:

```javascript
var owin = require('owin')

console.log('Starting long running operation...');
owin.worker(
    'CalculateBudget.dll',
    { revenue: 100, cost: 80 },
    function (error, result) {
        if (error) throw error;
        console.log('Result of long running operation: ', result);
    }
);

setInterval(function () { 
    console.log('Node.js event loop is alive!')
}, 1000);
```

Save the file as `test.js` and run it with `node test.js`. You will see output similar to this one:

```
C:\projects\owin_test>node test.js
Starting long running operation...
Node.js event loop is alive!
Node.js event loop is alive!
Node.js event loop is alive!
Node.js event loop is alive!
Node.js event loop is alive!
Result of long running operation:  { income: '20' }
Node.js event loop is alive!
Node.js event loop is alive!
```

This proves that the node.js event loop is not blocked while the CPU-bound computation runs on a CLR thread separate from the main event loop thread in node.js. 

## More

Issues? Feedback? You [know what to do](https://github.com/tjanczuk/owin/issues/new). 
Pull requests welcome.
