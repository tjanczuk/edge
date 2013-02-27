Hosting .NET OWIN applications in node.js
====

Owin allows you to host .NET code in node.js applications. It helps with:
* implementing express.js handlers and connect middleware for node.js application using .NET 4.5 ([read more](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html)),  
* implementing CPU-bound computations in .NET and running them in-process with node.js application without blocking the event loop ([read more](http://tomasz.janczuk.org/2013/02/cpu-bound-workers-for-nodejs.html)),  
* using C# and .NET instead of writing native node.js extensions in C/C++ and Win32 to access Windows specific functionality from a node.js application ([read more](http://tomasz.janczuk.org/2013/02/access-ms-sql-from-nodejs-application.html)). 

Owin is a native node.js module for Windows. It utilizes the [OWIN](http://owin.org) interface to bridge between JavaScript, native, and CLR/.NET code (think C#). The module takes care of marshaling data between V8 and CLR heaps as well as reconciling threading models. The .NET code is running in-process either asynchronously or on CLR threads while the node.js event loop remains unblocked.

Read more about the background and motivations of the project [here](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html). 

[Follow @tjanczuk](https://twitter.com/tjanczuk) for updates related to the module. 

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

## How to: access MS SQL from node.js

You can access a SQL database from node.js using CLR and asychronous ADO.NET running in-process, without writing a line of .NET code. 

To get started, you need a SQL connection string to your database. The snippets below assume this is the [sample Northwind SQL database](http://www.microsoft.com/en-us/download/details.aspx?id=23654). Initialize the `OWIN_SQL_CONNECTION_STRING` environment variable with this connection string:

```
set OWIN_SQL_CONNECTION_STRING=Data Source=(local);Initial Catalog=Northwind;Integrated Security=True
```

Install owin:

```
npm install owin
```

Then run the SQL command from your node.js application as follows:

```javascript
var owin = require('owin');

owin.sql("select * from Region", function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

The `owin.sql` call returns an array of arrays like this one:

```
C:\projects\owin>node test.js
[ [ 'RegionID', 'RegionDescription' ],
  [ 1, 'Eastern                                           ' ],
  [ 2, 'Western                                           ' ],
  [ 3, 'Northern                                          ' ],
  [ 4, 'Southern                                          ' ] ]
```

The first array specifies column names, subsequent arrays contain rows of data. 

Similarly to the `select` SQL statement, you can call `insert`, `update`, or `delete`:

```javascript
owin.sql("insert into Region values (5, 'Pacific Northwest')", function (error, result) { /* ... */ });
owin.sql("update Region set RegionDescription='Pacific Northwest' where RegionID=5", function (error, result) { /* ... */ });
owin.sql("delete Region where RegionID=5", function (error, result) { /* ... */ });
```

Upon successful execution, the callback of `insert`, `update`, and `delete` receives an integer indicating the number of affected rows in the database. 

ADO.NET exceptions are propagated back to the JavaScript as the `error` parameter of the callback function. For example, adding a row with an ID that already exists in the database yields:

```
C:\projects\owin>node test.js
C:\projects\owin\test.js:9
        if (error) throw error;
                         ^
System.AggregateException: One or more errors occurred. ---> System.Data.SqlClient.SqlException: Violation of 
PRIMARY KEY constraint 'PK_Region'. Cannot insert duplicate key in object 'dbo.Region'.
The statement has been terminated.
...
```

All SQL operations are using asychronous ADO.NET executing on the CLR thread pool. They block neither the node.js event loop, or tie up CLR threads. 

## How to: debugging

You can debug the .NET code running as part of your node.js application by attaching a managed code debugger (e.g. Visual Studio) to node.exe. Since the node.exe process runs both native and managed code, make sure to select the appropriate language to target:

![debug](https://f.cloud.github.com/assets/822369/190564/a41bab2c-7efb-11e2-878f-82ae2325876c.PNG)

## How to: exceptions

Exceptions thrown within the .NET code are propagated to the calling node.js application and re-thrown there by the owin module. For example:

```
C:\projects\owin_test>node test.js
Starting long running operation...

C:\projects\owin_test\test_worker.js:8
                if (error) throw error;
                                 ^
System.AggregateException: One or more errors occurred. ---> System.Exception: Sample exception
   at CalculateBudget.Startup.Execute(IDictionary`2 input)
   at Owinjs.Worker.<>c__DisplayClass1.<Invoke>b__0()
   at System.Threading.Tasks.Task.Execute()
   --- End of inner exception stack trace ---
---> (Inner Exception #0) System.Exception: Sample exception
   at CalculateBudget.Startup.Execute(IDictionary`2 input)
   at Owinjs.Worker.<>c__DisplayClass1.<Invoke>b__0()
   at System.Threading.Tasks.Task.Execute()<---
```

In case of express request handlers written in .NET, express framework will return them back to the browser client. For example:

![exception](https://f.cloud.github.com/assets/822369/190735/51f14504-7f0b-11e2-8ea1-04f81fa406ff.PNG)

## More

Issues? Feedback? You [know what to do](https://github.com/tjanczuk/owin/issues/new). Pull requests welcome.
