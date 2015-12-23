Edge.js: .NET and Node.js in-process [![Build Status](https://travis-ci.org/tjanczuk/edge.svg)](https://travis-ci.org/tjanczuk/edge)
====
 
An edge connects two nodes. This edge connects Node.js and .NET. V8 and CLR/CoreCLR/Mono - in process. On Windows, MacOS, and Linux. 

![image](https://cloud.githubusercontent.com/assets/822369/11969685/e9476f3a-a8d1-11e5-94d4-847bfc4ed960.png)

You can script C# from a Node.js process:

```javascript
var edge = require('edge');

var helloWorld = edge.func(function () {/*
    async (input) => { 
        return ".NET Welcomes " + input.ToString(); 
    }
*/});

helloWorld('JavaScript', function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

You can also script Node.js from C#:

```c#
using System;
using System.Threading.Tasks;
using EdgeJs;

class Program
{
    public static async void Start()
    {
        var func = Edge.Func(@"
            return function (data, callback) {
                callback(null, 'Node.js welcomes ' + data);
            }
        ");

        Console.WriteLine(await func(".NET"));
    }

    static void Main(string[] args)
    {
        Task.Run((Action)Start).Wait();
    }
}
```

## What problems does Edge.js solve?

> Ah, whatever problem you have. If you have this problem, this solves it.

*[--Scott Hanselman (@shanselman)](https://twitter.com/shanselman/status/461532471037677568)*

## Before you dive in

See the [Edge.js overview](http://tjanczuk.github.com/edge).  
Read the [Edge.js introduction on InfoQ](http://www.infoq.com/articles/the_edge_of_net_and_node).  
Listen to the [Edge.js podcast on Herdingcode](http://herdingcode.com/herding-code-166-tomasz-janczuk-on-edge-js/). 

![image](https://cloud.githubusercontent.com/assets/822369/2808101/87f73a5c-cd0f-11e3-9f7a-f53be86641be.png)

## Contents

[Introduction](#introduction)  
[Scripting CLR from Node.js](#scripting-clr-from-nodejs)  
&nbsp;&nbsp;&nbsp;&nbsp;[What you need](#what-you-need)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[Windows](#windows)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[Linux](#linux)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[OSX](#osx)  
&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;[Docker](#docker)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: C# hello, world](#how-to-c-hello-world)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: integrate C# code into Node.js code](#how-to-integrate-c-code-into-nodejs-code)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: specify additional CLR assembly references in C# code](#how-to-specify-additional-clr-assembly-references-in-c-code)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: marshal data between C# and Node.js](#how-to-marshal-data-between-c-and-nodejs)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: call Node.js from C#](#how-to-call-nodejs-from-c)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: export C# function to Node.js](#how-to-export-c-function-to-nodejs)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: script Python in a Node.js application](#how-to-script-python-in-a-nodejs-application)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: script PowerShell in a Node.js application](#how-to-script-powershell-in-a-nodejs-application)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: script F# in a Node.js application](#how-to-script-f-in-a-nodejs-application)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: script Lisp in a Node.js application](#how-to-script-lisp-in-a-nodejs-application)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: script T-SQL in a Node.js application](#how-to-script-t-sql-in-a-nodejs-application)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: support for other CLR languages](#how-to-support-for-other-clr-languages)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: exceptions](#how-to-exceptions)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: app.config](#how-to-appconfig)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: debugging](#how-to-debugging)  
&nbsp;&nbsp;&nbsp;&nbsp;[Performance](#performance)  
&nbsp;&nbsp;&nbsp;&nbsp;[Building on Windows](#building-on-windows)  
&nbsp;&nbsp;&nbsp;&nbsp;[Building on OSX](#building-on-osx)  
&nbsp;&nbsp;&nbsp;&nbsp;[Building on Linux](#building-on-linux)  
&nbsp;&nbsp;&nbsp;&nbsp;[Running tests](#running-tests)  
[Scripting Node.js from CLR](#scripting-nodejs-from-clr)  
&nbsp;&nbsp;&nbsp;&nbsp;[What you need](#what-you-need-1)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: Node.js hello, world](#how-to-nodejs-hello-world)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: integrate Node.js into CLR code](#how-to-integrate-nodejs-code-into-clr-code)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: use Node.js built-in modules](#how-to-use-nodejs-built-in-modules)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: use external Node.js modules](#how-to-use-external-nodejs-modules)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: handle Node.js events in .NET](#how-to-handle-nodejs-events-in-net)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: expose Node.js state to .NET](#how-to-expose-nodejs-state-to-net)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: use Node.js in ASP.NET application](#how-to-use-nodejs-in-aspnet-web-applications)  
&nbsp;&nbsp;&nbsp;&nbsp;[How to: debug Node.js code running in a CLR application](#how-to-debug-nodejs-code-running-in-a-clr-application)  
&nbsp;&nbsp;&nbsp;&nbsp;[Building Edge.js NuGet package](#building-edgejs-nuget-package)  
&nbsp;&nbsp;&nbsp;&nbsp;[Running tests of scripting Node.js in C#](#running-tests-of-scripting-nodejs-in-c)  
[Use cases and other resources](#use-cases-and-other-resources)  
[Contribution and derived work](#contribution-and-derived-work)  

## Introduction 

Edge.js allows you to run Node.js and .NET code in one process on Windows, MacOS, and Linux. 

You can call .NET functions from Node.js and Node.js functions from .NET. Edge.js takes care of marshalling data between CLR and V8. Edge.js also reconciles threading models of single threaded V8 and multi-threaded CLR. Edge.js ensures correct lifetime of objects on V8 and CLR heaps. The CLR code can be pre-compiled or specified as C#, F#, Python, or PowerShell source: Edge.js can compile CLR scripts at runtime. Edge can be extended to support other CLR languages or DSLs.

![Edge.js interop model](https://f.cloud.github.com/assets/822369/234085/b305625c-8768-11e2-8de0-e03ae98e7249.PNG)

Edge.js provides an asynchronous, in-process mechanism for interoperability between Node.js and .NET. You can use this mechanism to:

* script Node.js from a .NET application (console app, ASP.NET, etc.)
* script C# from a Node.js application on Windows, MacOS, and Linux
* access MS SQL from Node.js using ADO.NET [more...](http://blog.codeship.io/2014/04/22/leverage-sql-server-with-node-js-using-edge-js.html)  
* use CLR multi-threading from Node.js for CPU intensive work [more...](http://tomasz.janczuk.org/2013/02/cpu-bound-workers-for-nodejs.html)  
* write native extensions to Node.js in C# instead of C/C++  
* integrate existing .NET components into Node.js applications

Read more about the background and motivations of the project [here](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html). 

[Follow @tjanczuk](https://twitter.com/tjanczuk) for updates related to the module. 

## Scripting CLR from Node.js

If you are writing a Node.js application, this section explains how you include and run CLR code in your app. It works on Windows, MacOS, and Linux.

### What you need

Edge.js runs on Windows, Linux, and OSX and requires Node.js 5.x, 4.x, 0.12.x, 0.10.x, or 0.8.x, as well as .NET Framework 4.5 (Windows), Mono 4.0.4.1 (OSX, Linux), or Microsoft's CoreCLR (Windows, OSX, Linux). 

#### Windows

* Node.js 5.x, 4.x, 0.12.x, 0.10.x, or 0.8.x 
* [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653) and/or [CoreCLR](http://dotnet.github.io/core/getting-started/)
* to use Python, you also need [IronPython 2.7.3 or later](http://ironpython.codeplex.com/releases/view/81726)  
* to use F#, read [Dave Thomas blog post](http://7sharpnine.com/posts/i-node-something/)

If you have both desktop CLR and CoreCLR installed, read [using CoreCLR](#using-coreclr) for how to configure Edge to use one or the other. 

![image](https://cloud.githubusercontent.com/assets/822369/2808066/3707f37c-cd0d-11e3-9b4e-7257ffc27c9c.png)

#### Linux

* Node.js 5.x, 4.x, 0.12.x, 0.10.x, or 0.8.x
* Mono 4.x x64 and/or Microsoft's CoreCLR
* Follow [Linux setup instructions](#building-on-linux)

![image](https://cloud.githubusercontent.com/assets/822369/2808077/03f92874-cd0e-11e3-88ea-79f67b8b1d49.png)

#### OSX  

* Node.js 5.x, 4.x, 0.12.x, 0.10.x, or 0.8.x  
* Mono 4.0.4.1 x64 and/or Microsoft's CoreCLR
* Follow [OSX setup instructions](#building-on-osx)  

![image](https://cloud.githubusercontent.com/assets/822369/2808046/8f4ce378-cd0b-11e3-95dc-ef0842c28821.png)

#### Docker

Edge.js is available as a Docker image on the [tjanczuk/edgejs repository on Docker Hub](https://registry.hub.docker.com/u/tjanczuk/edgejs/). The image is based on Debian Jessie, and contains Node.js 4.2.3 x64, Mono 4.2.1 x64, CoreCLR 1.0.0 RC1 x64, and Edge.js:

By default Edge uses Mono to execute CLR code: 

```
> docker run -it tjanczuk/edgejs:5.0.0
> cd samples
> node 101_hello_lambda.js
.NET welcomes Node.js
```

Specify the EDGE_USE_CORECLR=1 environment variable to use CoreCLR instead: 

```
> docker run -it tjanczuk/edgejs:5.0.0
> cd samples
> EDGE_USE_CORECLR=1 node 101_hello_lambda.js
.NET welcomes Node.js
```

Alternatively, you can also specify the EDGE_USE_CORECLR when starting the container: 

```
> docker run -it -e EDGE_USE_CORECLR=1 tjanczuk/edgejs:5.0.0
```

### How to: C# hello, world

Follow setup instructions [for your platform](#what-you-need). 

Install edge:

```
npm install edge
```

In your server.js:

```javascript
var edge = require('edge');

var helloWorld = edge.func(function () {/*
    async (input) => { 
        return ".NET Welcomes " + input.ToString(); 
    }
*/});

helloWorld('JavaScript', function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Run and enjoy:

```
$>node server.js
.NET welcomes JavaScript
```

If you want to use CoreCLR as your .NET runtime and are running in a dual runtime environment (i.e. Windows with .NET 4.5 installed as well or Linux with Mono installed), you will need to tell edge to use CoreCLR by setting the `EDGE_USE_CORECLR` environment variable:

```
$>EDGE_USE_CORECLR=1 node server.js
.NET Welcomes JavaScript
```

### How to: integrate C# code into Node.js code

Edge provides several ways to integrate C# code into a Node.js application. Regardless of the way you choose, the entry point into the .NET code is normalized to a `Func<object,Task<object>>` delegate. This allows Node.js code to call .NET asynchronously and avoid blocking the Node.js event loop. 

Edge provides a function that accepts a reference to C# code in one of the supported representations, and returns a Node.js function which acts as a JavaScript proxy to the `Func<object,Task<object>>` .NET delegate:

```javascript
var edge = require('edge');

var myFunction = edge.func(...);
```

The function proxy can then be called from Node.js like any asynchronous function:

```javascript
myFunction('Some input', function (error, result) {
    //...
});
```

Alternatively, if you know the C# implementation will complete synchronously given the circumstances, you can call this function as any synchronous JavaScript function as follows:

```javascript
var result = myFunction('Some input', true);
```

The `true` parameter instead of a callback indicates that Node.js expects the C# implementation to complete synchronously. If the CLR function implementation does not complete synchronously, the call above will result in an exception. 

One representation of CLR code that Edge.js accepts is C# source code. You can embed C# literal representing a .NET async lambda expression implementing the `Func<object,Task<object>>` delegate directly inside Node.js code:

```javascript
var add7 = edge.func('async (input) => { return (int)input + 7; }');
``` 

In another representation, you can embed multi-line C# source code by providing a function with a body containing a multi-line comment. Edge extracts the C# code from the function body using regular expressions:

```javascript
var add7 = edge.func(function() {/*
    async (input) => {
        return (int)input + 7;
    }
*/});
```

If your C# code is more involved than a simple lambda, you can specify entire class definition. By convention, the class must be named `Startup` and it must have an `Invoke` method that matches the `Func<object,Task<object>>` delegate signature. This method is useful if you need to factor your code into multiple methods:

```javascript
var add7 = edge.func(function() {/*
    using System.Threading.Tasks;

    public class Startup
    {
        public async Task<object> Invoke(object input)
        {
            int v = (int)input;
            return Helper.AddSeven(v);
        }
    }

    static class Helper
    {
        public static int AddSeven(int v) 
        {
            return v + 7;
        }
    }
*/});
```

If your C# code grows substantially, it is useful to keep it in a separate file. You can save it to a file with `*.csx` or `*.cs` extension, and then reference from your Node.js application:

```javascript
var add7 = edge.func(require('path').join(__dirname, 'add7.csx'));
```

If you integrate C# code into your Node.js application by specifying C# source using one of the methods above, edge will compile the code on the fly. If you prefer to pre-compile your C# sources to a CLR assembly, or if your C# component is already pre-compiled, you can reference a CLR assembly from your Node.js code. In the most generic form, you can specify the assembly file name, the type name, and the method name when creating a Node.js proxy to a .NET method:

```javascript
var clrMethod = edge.func({
    assemblyFile: 'My.Edge.Samples.dll',
    typeName: 'Samples.FooBar.MyType',
    methodName: 'MyMethod' // This must be Func<object,Task<object>>
});
```

If you don't specify methodName, `Invoke` is assumed. If you don't specify typeName, a type name is constructed by assuming the class called `Startup` in the namespace equal to the assembly file name (without the `.dll`). In the example above, if typeName was not specified, it would default to `My.Edge.Samples.Startup`.

The assemblyFile is relative to the working directory. If you want to locate your assembly in a fixed location relative to your Node.js application, it is useful to construct the assemblyFile using `__dirname`. 

You can also create Node.js proxies to .NET functions specifying just the assembly name as a parameter:

```javascript
var clrMethod = edge.func('My.Edge.Samples.dll');
```

In that case the default typeName of `My.Edge.Samples.Startup` and methodName of `Invoke` is assumed as explained above. 

### How to: specify additional CLR assembly references in C# code

When you provide C# source code and let edge compile it for you at runtime, edge will by default reference only mscorlib.dll and System.dll assemblies.  If you're using CoreCLR, we automatically reference the most recent versions of the System.Runtime, System.Threading.Tasks, System.Dynamic.Runtime, and the compiler language packages, like Microsoft.CSharp. In applications that require additional assemblies you can specify them in C# code using a special hash pattern, similar to Roslyn. For example, to use ADO.NET you must reference System.Data.dll:

```javascript
var add7 = edge.func(function() {/*

    #r "System.Data.dll"

    using System.Data;
    using System.Threading.Tasks;

    public class Startup
    {
        public async Task<object> Invoke(object input)
        {
            // ...
        }
    }
*/});
```

If you prefer, instead of using comments you can specify references by providing options to the `edge.func` call:

```javascript
var add7 = edge.func({
    source: function() {/*

        using System.Data;
        using System.Threading.Tasks;

        public class Startup
        {
            public async Task<object> Invoke(object input)
            {
                // ...
            }
        }
    */},
    references: [ 'System.Data.dll' ]
);
```

If you are using CoreCLR, you must have a `project.json` file (specification [here](https://github.com/aspnet/Home/wiki/Project.json-file)) that specifies the dependencies for the application and you must have run the `dnu restore` command in that project's directory to generate a `project.lock.json` file.  This file must either be in the current working directory that `node` is executed in or you must specify its directory by setting the `EDGE_APP_ROOT` environment variable.  For example, if the `project.json` file is in the `c:\DotNet\MyProject` directory, you would run something like:

```
set EDGE_APP_ROOT=c:\DotNet\MyProject
node app.js
```

### How to: marshal data between C# and Node.js

Edge.js can marshal any JSON-serializable value between .NET and Node.js (although JSON serialization is not used in the process). Edge also supports marshalling between Node.js `Buffer` instance and a CLR `byte[]` array to help you efficiently pass binary data.

You can call .NET from Node.js and pass in a complex JavaScript object as follows:

```javascript
var dotNetFunction = edge.func('Edge.Sample.dll');

var payload = {
    anInteger: 1,
    aNumber: 3.1415,
    aString: 'foo',
    aBoolean: true,
    aBuffer: new Buffer(10),
    anArray: [ 1, 'foo' ],
    anObject: { a: 'foo', b: 12 }
};

dotNetFunction(payload, function (error, result) { });
```

In .NET, JavaScript objects are represented as dynamics (which can be cast to `IDictionary<string,object>` if desired), JavaScript arrays as `object[]`, and JavaScript `Buffer` as `byte[]`. Scalar JavaScript values have their corresponding .NET types (`int`, `double`, `bool`, `string`). Here is how you can access the data in .NET:

```c#
using System.Threading.Tasks;

public class Startup
{
    public async Task<object> Invoke(dynamic input)
    {
        int anInteger = (int)input.anInteger;
        double aNumber = (double)input.aNumber;
        string aString = (string)input.aString;
        bool aBoolean = (bool)input.aBoolean;
        byte[] aBuffer = (byte[])input.aBuffer;
        object[] anArray = (object[])input.anArray;
        dynamic anObject = (dynamic)input.anObject;

        return null;
    }
}

```

Similar type marshalling is applied when .NET code passes data back to Node.js code. In .NET code you can provide an instance of any CLR type that would normally be JSON serializable, including domain specific types like `Person` or anonymous objects. For example:

```c#
using System.Threading.Tasks;

public class Person
{
    public int anInteger = 1;
    public double aNumber = 3.1415;
    public string aString = "foo";
    public bool aBoolean = true;
    public byte[] aBuffer = new byte[10];
    public object[] anArray = new object[] { 1, "foo" };
    public object anObject = new { a = "foo", b = 12 };
}

public class Startup
{
    public async Task<object> Invoke(dynamic input)
    {
        Person person = new Person();
        return person;
    }
}
```

In your Node.js code that invokes this .NET method you can display the result object that the callback method receives:

```javascript
var edge = require('edge');

var getPerson = edge.func(function () {/*
    using System.Threading.Tasks;

    public class Person
    {
        public int anInteger = 1;
        public double aNumber = 3.1415;
        public string aString = "foo";
        public bool aBoolean = true;
        public byte[] aBuffer = new byte[10];
        public object[] anArray = new object[] { 1, "foo" };
        public object anObject = new { a = "foo", b = 12 };
    }

    public class Startup
    {
        public async Task<object> Invoke(dynamic input)
        {
            Person person = new Person();
            return person;
        }
    }
*/});

getPerson(null, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Passing this .NET object to Node.js generates a JavaScript object as follows:

```
$>node sample.js
{ anInteger: 1,
  aNumber: 3.1415,
  aString: 'foo',
  aBoolean: true,
  aBuffer: <Buffer 00 00 00 00 00 00 00 00 00 00>,
  anArray: [ 1, 'foo' ],
  anObject: { a: 'foo', b: 12 } }
```

When data is marshalled from .NET to Node.js, no checks for circular references are made. They will typically result in stack overflows. Make sure the object graph you are passing from .NET to Node.js is a tree and does not contain any cycles. 

**WINDOWS ONLY** When marshalling strongly typed objects (e.g. Person) from .NET to Node.js, you can optionally tell Edge.js to observe the [System.Web.Script.Serialization.ScriptIgnoreAttribute](http://msdn.microsoft.com/en-us/library/system.web.script.serialization.scriptignoreattribute.aspx). You opt in to this behavior by setting the `EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE` environment variable:

```
set EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE=1
```

Edge.js by default does not observe the ScriptIgnoreAttribute to avoid the associated performance cost. 

### How to: call Node.js from C#  

In addition to marshalling data, edge can marshal proxies to JavaScript functions when invoking .NET code from Node.js. This allows .NET code to call back into Node.js. 

Suppose the Node.js application passes an `add` function to the .NET code as a property of an object. The function receives two numbers and returns the sum of them via the provided callback:

```javascript
var edge = require('edge');

var addAndMultiplyBy2 = edge.func(function () {/*
    async (dynamic input) => {
        var add = (Func<object, Task<object>>)input.add;
        var twoNumbers = new { a = (int)input.a, b = (int)input.b };
        var addResult = (int)await add(twoNumbers);
        return addResult * 2;
    }   
*/});

var payload = {
    a: 2,
    b: 3,
    add: function (data, callback) {
        callback(null, data.a + data.b);
    }
};

addAndMultiplyBy2(payload, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

The .NET code implements the addAndMultiplyBy2 function. It extracts the two numbers passed from Node.js, calls back into the `add` function exported from Node.js to add them, multiplies the result by 2 in .NET, and returns the result back to Node.js:

```
$>node sample.js
10
```

The Node.js function exported from Node.js to .NET must follow the prescriptive async pattern of accepting two parameters: payload and a callback. The callback function accepts two parameters. The first one is the error, if any, and the second the result of the operation:

```javascript
function (payload, callback) {
    var error;  // must be null or undefined in the absence of error
    var result; 

    // do something

    callback(error, result);
}
```

The proxy to that function in .NET has the following signature:

```c#
Func<object,Task<object>>
```

Using TPL in CLR to provide a proxy to an asynchronous Node.js function allows the .NET code to use the convenience of the `await` keyword when invoking the Node.js functionality. The example above shows the use of the `await` keyword when calling the proxy of the Node.js `add` method.  

### How to: export C# function to Node.js

Similarly to marshalling functions from Node.js to .NET, Edge.js can also marshal functions from .NET to Node.js. The .NET code can export a `Func<object,Task<object>>` delegate to Node.js as part of the return value of a .NET method invocation. For example:

```javascript
var createHello = edge.func(function () {/*
    async (input) =>
    {
        return (Func<object,Task<object>>)(async (i) => { 
            Console.WriteLine("Hello from .NET"); 
            return null; 
        });
    }
*/});

var hello = createHello(null, true); 
hello(null, true); // prints out "Hello from .NET"
```

This mechanism in conjunction with a closure can be used to expose CLR class instances or CLR state in general to JavaScript. For example:

```javascript
var createCounter = edge.func(function () {/*
    async (input) =>
    {
        var k = (int)input; 
        return (Func<object,Task<object>>)(async (i) => { return ++k; });
    }
*/});

var counter = createCounter(12, true); // create counter with 12 as initial state
console.log(counter(null, true)); // prints 13
console.log(counter(null, true)); // prints 14
```

### How to: script Python in a Node.js application

**NOTE** This functionality requires IronPython and has been tested on Windows only. 

Edge.js enables you to run Python and Node.js in-process.

In addition to [platform specific prerequisites](#what-you-need) you need [IronPython 2.7.3](http://ironpython.codeplex.com/releases/view/81726) to proceed.

#### Hello, world

Install edge and edge-py modules:

```
npm install edge
npm install edge-py
```

In your server.js:

```javascript
var edge = require('edge');

var hello = edge.func('py', function () {/*
    def hello(input):
        return "Python welcomes " + input

    lambda x: hello(x)
*/});

hello('Node.js', function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Run and enjoy:

```
$>node py.js
Python welcomes Node.js
```

#### The interop model

Your Python script must evaluate to a lambda expression that accepts a single parameter. The parameter represents marshalled input from the Node.js code. The return value of the lambda expression is passed back as the result to Node.js code. The Python script can contain constructs (e.g. Python functions) that are used in the closure of the lambda expression. The instance of the script with associated state is created when `edge.func` is called in Node.js. Each call to the function referes to that instance.

The simplest *echo* Python script you can embed in Node.js looks like this:

```python
lambda x: x
```

To say hello, you can use something like this:

```python
lambda: x: "Hello, " + x
```

To maintain a running sum of numbers:

```python
current = 0

def add(x):
    global current
    current = current + x
    return current

lambda x: add(x)
```

#### Python in its own file

You can reference Python script stored in a *.py file instead of embedding Python code in a Node.js script.

In your hello.py file:

```python
def hello(input):
    return "Python welcomes " + input

lambda x: hello(x)
```

In your hello.js file:

```javascript
var edge = require('edge');

var hello = edge.func('py', 'hello.py');

hello('Node.js', function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Run and enjoy:

```
$>node hello.js
Python welcomes Node.js
```

#### To sync or to async, that is the question

In the examples above Python script was executing asynchronously on its own thread without blocking the singleton V8 thread on which the Node.js event loop runs. This means your Node.js application remains responsive while the Python code executes in the background. 

If you know your Python code is non-blocking, or if you know what you are doing, you can tell Edge.js to execute Python code on the singleton V8 thread. This will improve performance for non-blocking Python scripts embedded in a Node.js application:

```javascript
var edge = require('edge');

var hello = edge.func('py', {
    source: function () {/*
        def hello(input):
            return "Python welcomes " + input

        lambda x: hello(x)
    */},
    sync: true
});

console.log(hello('Node.js', true));
```

The `sync: true` property in the call to `edge.func` tells Edge.js to execute Python code on the V8 thread as opposed to creating a new thread to run Python script on. The `true` parameter in the call to `hello` requests that Edge.js does in fact call the `hello` function synchronously, i.e. return the result as opposed to calling a callback function. 

### How to: script PowerShell in a Node.js application

**NOTE** This functionality only works on Windows. 

Edge.js enables you to run PowerShell and Node.js in-process on Windows. [Edge-PS](https://github.com/dfinke/edge-ps) connects the PowerShell ecosystem with Node.js.

You need Windows, [Node.js](http://nodejs.org), [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653), and [PowerShell 3.0](http://www.microsoft.com/en-us/download/details.aspx?id=34595) to proceed.

### Hello, world

Install edge and edge-ps modules:

``` 
npm install edge
npm install edge-ps
```

In your server.js:

```javascript
var edge = require('edge');

var hello = edge.func('ps', function () {/*
"PowerShell welcomes $inputFromJS on $(Get-Date)"
*/});

hello('Node.js', function (error, result) {
    if (error) throw error;
    console.log(result[0]);
});
```

Run and enjoy:

```
C:\testEdgeps>node server
PowerShell welcomes Node.js on 05/04/2013 09:38:40
```

#### Tapping into PowerShell's ecosystem

Rather than embedding PowerShell directly, you can use PowerShell files, dot source them and even use *Import-Module*.

What you can do in native PowerShell works in Node.js.

#### Interop PowerShell and Python

Here you can reach out to IronPython from PowerShell from within Node.js on Windows. This holds true for working with JavaScript frameworks and C#.

```javascript
var edge = require('edge');

var helloPowerShell = edge.func('ps', function () {/*
	"PowerShell welcomes $inputFromJS"
*/});

var helloPython = edge.func('py', function () {/*
    def hello(input):
        return "Python welcomes " + input

    lambda x: hello(x)
*/});


helloPython('Node.js', function(error, result){
	if(error) throw error;

	helloPowerShell(result, function(error, result){
		if(error) throw error;
		console.log(result[0]);
	});
});
```

### How to: script F# in a Node.js application

**NOTE** This functionality has not been tested on non-Windows platforms. 

This section is coming up. In the meantime please refer to [Dave Thomas blog post](http://7sharpnine.com/posts/i-node-something/). This has been validated on Windows only. 

```javascript
var edge = require('edge');

var helloFs = edge.func('fs', function () {/*
    fun input -> async { 
        return "F# welcomes " + input.ToString()
    }
*/});

helloFs('Node.js', function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

### How to: script Lisp in a Node.js application

**NOTE** This functionality has not been tested on non-Windows platforms. 

The [edge-lsharp](https://github.com/richorama/edge-lsharp) extension uses [LSharp](https://github.com/RobBlackwell/LSharp) to compile and run Lisp to .NET.

Install edge and edge-lsharp modules:

```
npm install edge
npm install edge-lsharp
```

In your server.js:

```javascript
var edge = require('edge');
var fact = edge.func('lsharp', function(){/*

;; Factorial
(def fact(n) 
    (if (is n 0) 1 (* n (fact (- n 1)))))

*/});

fact([5], function(err, answer){
    console.log(answer);
    // = 120
});
```

An LSharp filename can be passed in instead of the Lisp string/comment:

```js
var edge = require('edge');
var lisp = edge.func('lsharp', 'lisp-func.ls');

lisp(['arg1', 'arg2'], function(err, result){
    
});
```

In Lisp you can specify either a function (as shown in the first example) or just return a value:

```js
var edge = require('edge');
var lisp = edge.func('lsharp', '(+ 2 3)');

lisp([], function(err, answer){
    console.log(answer);
    // = 5
});
```

### How to: script T-SQL in a Node.js application

**NOTE** This functionality has only been tested on Windows. Although ADO.NET exist in Mono, your mileage can vary. 

The edge-sql extension of Edge.js allows for accessing MS SQL databases by scripting T-SQL inside the Node.js application. The edge-sql extension uses async ADO.NET SQL client to access MS SQL. 

You need Windows, [Node.js](http://nodejs.org), and [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653). To run the sample code below you also need a connection string to the sample Northwind database that ships with MS SQL. 

#### Hello, world

Install edge and edge-sql modules:

```
npm install edge
npm install edge-sql
```

Set the connection string as an environment variable (your connection string may be different):

```
set EDGE_SQL_CONNECTION_STRING=Data Source=localhost;Initial Catalog=Northwind;Integrated Security=True
```

In your server.js:

```javascript
var edge = require('edge');

var getTop10Products = edge.func('sql', function () {/*
    select top 10 * from Products
*/});

getTop10Products(null, function (error, result) {
    if (error) throw error;
    console.log(result);
    console.log(result[0].ProductName);
    console.log(result[1].ReorderLevel);
});
```

Run and enjoy:

```
C:\projects\edge\samples>node server.js
[ { ProductID: 10,
    ProductName: 'New Ikura',
    SupplierID: 4,
    CategoryID: 8,
    QuantityPerUnit: '12 - 200 ml jars',
    UnitPrice: '31.000',
    UnitsInStock: 31,
    UnitsOnOrder: 0,
    ReorderLevel: 0,
    Discontinued: false },
    ...
]
New Ikura
12
```

#### Parameterized queries

You can construct a parameterized query once and provide parameter values on a per-call basis:

```javascript
var edge = require('edge');

var getProduct = edge.func('sql', function () {/*
    select * from Products 
    where ProductId = @myProductId
*/});

getProduct({ myProductId: 10 }, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

#### Basic CRUD support: select, update, insert, delete

The four basic CRUD operations are supported. For example, here is how an update can look like:

```javascript
var edge = require('edge');

var updateProductName = edge.func('sql', function () {/*
    update Products
    set ProductName = @newName 
    where ProductId = @myProductId
*/});

updateProductName({ myProductId: 10, newName: 'New Ikura' }, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

### How to: support for other CLR languages

Edge.js can work with any pre-compiled CLR assembly that contains the `Func<object,Task<object>>` delegate. Out of the box, Edge.js also allows you to embed C# source code in a Node.js application and compile it on the fly. 

To enable compilation of other CLR languages (e.g. F#) at runtime, or to support domain specific languages (DSLs) like T-SQL, you can use the compiler composability model provided by Edge.js. Please read the [add support for a CLR language](https://github.com/tjanczuk/edge/wiki/Add-support-for-a-CLR-language) guide to get started. 

### How to: exceptions

Edge.js marshals Node.js errors and exceptions to .NET as well as .NET exceptions to Node.js. 

CLR exceptions thrown in .NET code invoked from Node.js are marshalled as the `error` parameter to the Node.js callback function. Consider this example:

```javascript
var edge = require('edge');

var clrFunc = edge.func(function () {/*
    async (dynamic input) => {
        throw new Exception("Sample exception");
    }
*/});

clrFunc(null, function (error, result) {
    if (error) {
		console.log('Is Error?', error instanceof Error);
		console.log('-----------------');
		console.log(util.inspect(error, showHidden=true, depth=99, colorize=false));
		return;
	}
});
```

Running this Node.js application shows that the CLR exception was indeed received by the Node.js callback. The `error` parameter contains an Error object having most of the properties of the Exceptions copied over:
```
Is Error? true
-----------------
{ [System.AggregateException: One or more errors occurred.]
  message: 'One or more errors occurred.',
  name: 'System.AggregateException',
  InnerExceptions: 'System.Collections.ObjectModel.ReadOnlyCollection`1[[System.Exception, mscorlib, Version=4.0.0.0, Culture=neutral, PublicKeyToken=b77a5c561934e089]]',
  Message: 'One or more errors occurred.',
  Data: 'System.Collections.ListDictionaryInternal',
  InnerException:
   { [System.Exception: Sample exception]
     message: 'Sample exception',
     name: 'System.Exception',
     Message: 'Sample exception',
     Data: 'System.Collections.ListDictionaryInternal',
     TargetSite: 'System.Reflection.RuntimeMethodInfo',
     StackTrace: '   at Startup.<<Invoke>b__0>d__2.MoveNext() in c:\\Users\\User.Name\\Source\\Repos\\eCash2\\test\\edge2.js:line 7\r\n--- End of stack trace from previous location where exception was thrown ---\r\n   at System.Runtime.CompilerServices.TaskAwaiter.ThrowForNonSuccess(Task task)\r\n   at System.Runtime.CompilerServices.TaskAwaiter.HandleNonSuccessAndDebuggerNotification(Task task)\r\n   at System.Runtime.CompilerServices.TaskAwaiter`1.GetResult()\r\n   at Startup.<Invoke>d__4.MoveNext() in c:\\Users\\User.Name\\Source\\Repos\\eCash2\\test\\edge2.js:line 5',
     Source: 'cp2luegt',
     HResult: -2146233088 },
  HResult: -2146233088 }
```
The exception is copied back as Error object like every normal result object from the .NET world to JavaScript. 
Therefore all properties and their values are available on the Error object.

Additionally, the following happens during the mapping:
* To represent the Exception type, its full name is stored as `name`.
* To follow the [JavaScript convention for Errors](https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Error), the `Message` is also stored as the property `message`.
* `System::Reflection::RuntimeMethodInfo`s are not copied to avoid stack overflows

```
$>node sample.js

Edge.js:58
    edge.callClrFunc(appId, data, callback);
                     ^
System.Reflection.TargetInvocationException: Exception has been thrown by the target of an invocation. 
---> System.Exception: Sample exception
   at Startup.Invoke(Object input) in sample.cs:line 12
``` 

JavaScript exceptions thrown in Node.js code invoked from .NET are wrapped in a CLR exception and cause the asynchronous `Task<object>` to complete with a failure. Errors passed by Node.js code invoked from .NET code to the callback function's `error` parameter have the same effect. Consider this example:

```javascript
var edge = require('edge');

var multiplyBy2 = edge.func(function () {/*
    async (dynamic input) => {
        var aFunctionThatThrows = (Func<object, Task<object>>)payload.aFunctionThatThrows;
        try {
            var aResult = await aFunctionThatThrows(null);
        }
        catch(Exception e)
        {
            Console.WriteLine(e);
        }

        return null;
    }
*/});

var payload = {
    someParameter: 'arbitrary parameter',
    aFunctionThatThrows: function (data, callback) {
        throw new Error('Sample JavaScript error');
    }
};

multiplyBy2(payload, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Running the code shows the .NET code receiving a CLR exception as a result of the Node.js function throwing a JavaScript error. The exception shows the complete stack trace, including the part that executed in the Node.js code:

```
$>node sample.js
System.Exception: Error: Sample JavaScript error
    at payload.aFunctionThatThrows (sample.js:7:11)
   at System.Runtime.CompilerServices.TaskAwaiter.ThrowForNonSuccess(Task task)
   at System.Runtime.CompilerServices.TaskAwaiter.HandleNonSuccessAndDebuggerNotification(Task task)
   at Edge.Sample.Startup.<Invoke>d__0.MoveNext()
```

### How to: app.config

When running C# code within Node.js app, the app config file is node.exe.config and should be located right next to the node.exe file.

### How to: debugging

**NOTE** This is Windows-only functionality.

On Windows, you can debug the .NET code running as part of your Node.js application by attaching a managed code debugger (e.g. Visual Studio) to node.exe. You can debug .NET code in a pre-compiled CLR assembly as well C# literals embedded in the application and compiled by Edge.js at runtime. 

#### Debugging pre-compiled .NET code

If you have integrated .NET code into a Node.js application using a pre-compiled CLR assembly like this:

```javascript
var hello = edge.func('My.Assembly.dll');
```

then the best way to debug your .NET code is to attach a managed code debugger (e.g. Visual Studio) to the node.exe process. Since the node.exe process runs both native and managed code, make sure to select managed code type as target:

![debug](https://f.cloud.github.com/assets/822369/190564/a41bab2c-7efb-11e2-878f-82ae2325876c.PNG)

From there, you can set breakpoints in your .NET code and the debugger will stop when they are reached.

#### Debugging embedded C# code

Debugging embedded C# code (on Windows) requires that `EDGE_CS_DEBUG` environment variable is set in the environment of the node.exe process:

```
set EDGE_CS_DEBUG=1
```

Without this setting (the default), Edge.js will not generate debugging information when compiling embedded C# code.

You can debug C# code embedded into a Node.js application using a reference to a *.cs or *.csx file:

```javascript
var hello = edge.func('MyClass.cs');
```

You can also debug C# code embedded directly into a *.js file using the function comment syntax:

```javscript
var hello = edge.func(function () {/*
    async (input) =>
    {
        System.Diagnostics.Debugger.Break();
        var result = ".NET welcomes " + input.ToString();
        return result;
    }
*/});
```

You *cannot* debug C# code embedded as a simple string literal:

```javascript
var hello = edge.func('async (input) => { return 2 * (int)input; }');
```

After setting `EDGE_CS_DEBUG=1` environment variable before starting node.exe and attaching the managed debugger to the node.exe process, you can set breakpoints in C# code (which may appear as a JavaScript comment), or use `System.Diagnostics.Debugger.Break()` to break into the debugger from .NET code. 

![debug-inline](https://f.cloud.github.com/assets/822369/326781/923d870c-9b4a-11e2-8f45-201a6431afbf.PNG)

### Performance

Read more about [performance of Edge.js on the wiki](https://github.com/tjanczuk/edge/wiki/Performance). Here is the gist of the latency (smaller is better):

![edgejs-performance1](https://f.cloud.github.com/assets/822369/486393/645f696a-b920-11e2-8a20-9fa6932bb092.png)

### Building on Windows

You must have Visual Studio 2013 toolset, Python 2.7.x, and node-gyp installed for building.

To build and test the project against all supported versions of Node.js in x86 and x64 flavors, run the following:

```
tools\buildall.bat
test\testall.bat
npm run jshint
```

To build one of the versions of Node.js officially released by [Node.js](http://nodejs.org/dist), do the following:

```
cd tools
build.bat release 0.10.0
```

Note: the Node.js version number you provide must be version number corresponding to one of the subdirectories of http://nodejs.org/dist. The command will build both x32 and x64 architectures (assuming you use x64 machine). The command will also copy the edge\_\*.node executables to appropriate locations under lib\native directory where they are looked up from at runtime. The `npm install` step copies the C standard library shared DLL to the location of the edge\_\*.node files for the component to be ready to go.

To build the C++\CLI native extension using the version of Node.js installed on your machine, issue the following command:

```
npm install -g node-gyp
node-gyp configure --msvs_version=2013
node-gyp build -debug
```

You can then set the EDGE_NATIVE environment variable to the fully qualified file name of the built edge_\*.node binary (edge\_nativeclr.node if you're using the native CLR runtime or edge\_coreclr.node if you're using CoreCLR). It is useful during development, for example:

```
set EDGE_NATIVE=C:\projects\edge\build\Debug\edge_nativeclr.node
``` 

You can also set the `EDGE_DEBUG` environment variable to 1 to have the edge module generate debug traces to the console when it runs.

### Running tests

You must have mocha installed on the system. Then:

```
npm test
```

or, from the root of the enlistment:

```
mocha -R spec
```

**NOTE** in environments with both desktop CLR/Mono and CoreCLR installed, tests will b default use desktop CLR/Mono. To run tests against CoreCLR, use: 

```
EDGE_USE_CORECLR=1 npm test
```

#### Node.js version targeting on Windows

**NOTE** this is Windows only functionality.

If you want to run tests after building against a specific version of Node.js that one of the previous builds used, issue the following command:

```
cd test
test.bat ia32 0.10.0
```

Which will run the tests using Node.js x86 v0.10.0. Similarly:

```
cd test
test.bat x64 0.8.22
```

Would run tests against Node.js 0.8.22 on x64 architecture.

Lastly, you can run jshint on the project with:

```
npm run jshint
```

### Building on OSX

Prerequisities:

* [Homebrew](http://brew.sh/)  
* [Mono x64](http://www.mono-project.com/download/#download-mac) and/or [CoreCLR](https://dotnet.github.io/getting-started/) - see below  
* [Node.js x64](http://nodejs.org/) (tested with v4.1.1)  

You can use Edge.js on OSX with either Mono or CoreCLR installed, or both.

If you choose to [install Mono](http://www.mono-project.com/download/#download-mac), select the universal installer to make sure you get the x64 version. Edge.js requires Mono x64. 

If you choose to install CoreCLR], follow these steps: 

```bash
brew tap aspnet/dnx
brew update
brew install dnvm
source dnvm.sh
dnvm install latest -r coreclr -alias edge-coreclr
```

Then install and build Edge.js:

```bash
brew install pkg-config
dnvm use edge-coreclr
npm install edge
```

**NOTE** if the build process complains about being unable to locate mono libraries, you maby need to specify the search path explicitly. This may be installation dependent, but in most cases will look like: 

```bash
PKG_CONFIG_PATH=/Library/Frameworks/Mono.framework/Versions/Current/lib/pkgconfig \
  npm install egde
```

If you installed both Mono and CoreCLR, by default Edge will use Mono. You opt in to using CoreCLR with the `EDGE_USE_CORECLR` environment variable: 

```bash
EDGE_USE_CORECLR=1 node myapp.js
```

#### Building on OSX (advanced)

To build edge from a clone of the repository or source code:

```bash
node-gyp configure build
```

To build a debug build instead of release, you need to:

```bash
node-gyp configure build -debug
export EDGE_NATIVE=/Users/tomek/edge/build/Debug/edge_nativeclr.node
```

After installation, if you wish to run using CoreCLR, switch to it using:

````bash
dnvm use edge-coreclr
````

If you wish to run using Mono, switch to it using:

````bash
dnvm use edge-mono
````

### Building on Linux 

These instructions were tested on Ubuntu 14.04 x64 and Debian Wheezy x64. High level, you must have Node.js x64 and either Mono x64 or Microsoft's CoreCLR (or both) installed on the machine before you can install Edge.js. There are two ways of getting there.

### Debian or Ubuntu, starting from a clean VM (i.e. taking the high road)

If you have a fresh Debian Wheezy or Ubuntu 14.04 x64 installation, the most convenient way of installing Edge.js with all prerequisites is by running:

```bash
export USERNAME=YourUserNameHere
sudo bash -c 'bash <(wget -qO- https://raw.githubusercontent.com/tjanczuk/edge/master/tools/debian_ubuntu_clean_install.sh)'
```

This will do the following:

* Download Node.js v4.2.3 sources, build, and install Node.js x64
* Install Mono x64
* Install latest Microsoft's CoreCLR
* Download and install node-gyp and mocha
* Download Edge.js sources and build x64 release
* Run Edge.js tests using both Mono and CoreCLR

If successful, your machine will have all the prerequisites to `npm install edge`.

As you installed both Mono and CoreCLR, by default Edge will use Mono. You can opt in to using CoreCLR with the `EDGE_USE_CORECLR` environment variable: 

```bash
EDGE_USE_CORECLR=1 node myapp.js
```

#### Building on Ubuntu (advanced)

This method is adequate if you already have a Mono x64, CoreCLR, and/or Node.js x64 install on the machine and need to incrementally add Edge to it. 

Read through the [install script](https://raw.githubusercontent.com/tjanczuk/edge/mono/tools/debian_ubuntu_clean_install.sh) and cherry pick the steps you need. Here are some gotchas:

* If you need to build Mono, make sure to run `ldconfig` afterwards, otherwise the garbage collection libraries may not load.  
* To make sure Mono can load the standard C library, run `sudo ln -s -f /lib/x86_64-linux-gnu/libc.so.6 /lib/x86_64-linux-gnu/libc.so`. For background on that step, see how [Mono loads native libraries](http://www.mono-project.com/Interop_with_Native_Libraries) or just [cut to the chase](http://stackoverflow.com/questions/14359981/mono-and-unmanaged-code-in-ubuntu).  

To build a debug build instead of release, you need to:

```bash
node-gyp configure build -debug
export EDGE_NATIVE=/home/tomek/edge/build/Debug/edge_nativeclr.node
```

### Using CoreCLR

If you have only CoreCLR installed on your system and not Mono, you can run Edge with no changes.  However, if you have both runtimes installed, Edge will automatically use Mono unless directed otherwise.  To use CoreCLR in a dual-runtime entironment, set the `EDGE_USE_CORECLR=1` environment variable when starting node, i.e.

```bash
EDGE_USE_CORECLR=1 node sample.js
```

Edge will try to find the CLR runtime in the following locations:

 * The path in the `CORECLR_DIR` environment variable, if provided
 * The current directory
 * The directory containing `edge_*.node`
 * Directories in the `PATH` environment variable
  
If you've used `dnvm install` and `dnvm use` to set your preferred version of the CLR, you don't have to supply any additional parameters or environment variables when starting node.  However, if the CLR is another location or you want to use a version of the CLR other than the default that you've set, the best way to specify that is through the `CORECLR_DIR` environment variable, i.e.

```bash
EDGE_USE_CORECLR=1 \
CORECLR_DIR=/home/user/.dnx/runtimes/dnx-coreclr-linux-x64.1.0.0-beta6-11944/bin \
node sample.js
```

## Scripting Node.js from CLR

If you are writing a CLR application (e.g. a C# console application or ASP.NET web app), this section explains how you include and run Node.js code in your app. Currently it works on Windows using desktop CLR, but support for MacOS, and Linux as well as CoreCLR is coming soon. 

### What you need

You need Windows with:

* [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653)  
* [Edge.js NuGet package](https://www.nuget.org/packages/Edge.js)  
* [Node.js](http://nodejs.org) (optional, if you want to use additional NPM packages)

Edge.js support for scripting Node.js ships as a NuGet Package called `Edge.js`. It comes with everything you need to get started writing applications for x86 and x64 architectures. However, if you want to use additional Node.js packages from NPM, you must separately install Node.js runtime to access the NPM package manager. Edge.js NuGet package has been developed and tested with Node.js v4.1.1. Older Edge.js packages exist for prior versions of Node.js. If you choose a different version of Node.js to install NPM packages, your mileage can vary. 

**NOTE** you cannot use native Node.js extensions when scripting Node.js from CLR using Edge. 

You can install the [Edge.js NuGet package](https://www.nuget.org/packages/Edge.js) using the Visual Studio built-in NuGet package management functionality or using the stand-alone [NuGet client](http://docs.nuget.org/docs/start-here/installing-nuget). 

### How to: Node.js hello, world

Create a .NET 4.5 Console Application in Visual Studio. Add the Edge.js NuGet package to the project. Then in your code:

```c#
using System;
using System.Threading.Tasks;
using EdgeJs;

class Program
{
    public static async void Start()
    {
        var func = Edge.Func(@"
            return function (data, callback) {
                callback(null, 'Node.js welcomes ' + data);
            }
        ");

        Console.WriteLine(await func(".NET"));
    }

    static void Main(string[] args)
    {
        Task.Run((Action)Start).Wait();
    }
}
```

Compile and run:

```
C:\project\sample\bin\Debug> sample.exe
Node.js welcomes .NET
```

### How to: integrate Node.js code into CLR code

The Edge.js NuGet package contains a single managed assembly `EdgeJs.dll` with a single class `EdgeJs.Edge` exposing a single static function `Func`. The function accepts a string containing code in Node.js that constructs and *returns* a JavaScript function. The JavaScript function must have the signature required by Edge.js's prescriptive interop pattern: it must accept one parameter and a callback, and the callback must be called with an error and one return value: 

```c#
var func = Edge.Func(@"
    return function (data, callback) {
        callback(null, 'Hello, ' + data);
    }
");
```

Edge.js creates a `Func<object,Task<object>>` delegate in CLR that allows .NET code to call the Node.js function asynchronously. You can use the standard TPL mechanisms or the async/await keywords to conveniently await completion of the asynchornous Node.js function:

```c#
var result = await func(".NET");
// result == "Hello, .NET"
```

Note that the Node.js code snippet is not a function *definition*. Instead it must create and *return* a function instance. This allows you to initialize and maintain encapsulated Node.js state associated with the instance of the created function. The initialization code will execute only once when you call `Edge.Func`. Conceptually this is similar to defining a Node.js module that exports a single function (by returning it to the caller).  For example:

```c#
var increment = Edge.Func(@"
    var current = 0;

    return function (data, callback) {
        current += data;
        callback(null, current);
    }
");

Console.WriteLine(await increment(4)); // outputs 4
Console.WriteLine(await increment(7)); // outputs 11
```

Using multiline C# string literals is convenient for short Node.js code snippets, but you may want to store larger Node.js code in its own `*.js` file or files. 

One pattern is to store your Node.js code in a `myfunc.js` file:

```javascript
return function (data, callback) {
    callback(null, 'Node.js welcomes ' + data);
}
```

And then load such file into memory with `File`:

```c#
var func = Edge.Func(File.ReadAllText("myfunc.js"));
```

Another pattern is to define a Node.js module that itself is a function:

```javascript
module.exports = function (data, callback) {
    callback(null, 'Node.js welcomes ' + data);
};
```

And then load and return this module with a short snippet of Node.js:

```c#
var func = Edge.Func(@"return require('./../myfunc.js')");
```

(Note the relative location of the file).

### How to: use Node.js built-in modules

You can use Node.js built-in modules out of the box. For example, you can set up a Node.js HTTP server hosted in a .NET application and call it from C#:

```c#
var createHttpServer = Edge.Func(@"
    var http = require('http');

    return function (port, cb) {
        var server = http.createServer(function (req, res) {
            res.end('Hello, world! ' + new Date());
        }).listen(port, cb);
    };
");

await createHttpServer(8080);
Console.WriteLine(await new WebClient().DownloadStringTaskAsync("http://localhost:8080"));
```

### How to: use external Node.js modules

You can use external Node.js modules, for example modules installed from NPM. 

Note: Most Node.js modules are written in JavaScript and will execute in Edge as-is. However, some Node.js external modules are native binary modules, rebuilt by NPM on module installation to suit your local execution environment. Native binary modules will not run in Edge unless they are rebuilt to link against the NodeJS dll that Edge uses.

To install modules from NPM, you must first [install Node.js](http://nodejs.org) on your machine and use the `npm` package manager that comes with the Node.js installation. NPM modules must be installed in the directory where your build system places the Edge.js NuGet package (most likely the same location as the rest of your application binaries), or any ancestor directory. Alternatively, you can install NPM modules globally on the machine using `npm install -g`:

```
C:\projects\websockets> npm install ws
...
ws@0.4.31 node_modules\ws
├── tinycolor@0.0.1
├── options@0.0.5
├── nan@0.3.2
└── commander@0.6.1
```

You can then use the installed `ws` module to create a WebSocket server inside of a .NET application:

```c#
class Program
{
    public static async void Start()
    {
        var createWebSocketServer = Edge.Func(@"
            var WebSocketServer = require('ws').Server;

            return function (port, cb) {
                var wss = new WebSocketServer({ port: port });
                wss.on('connection', function (ws) {
                    ws.on('message', function (message) {
                        ws.send(message.toUpperCase());
                    });
                    ws.send('Hello!');
                });
                cb();
            };
        ");

        await createWebSocketServer(8080);
    }

    static void Main(string[] args)
    {
        Task.Run((Action)Start);
        new ManualResetEvent(false).WaitOne();
    }
}
```

This WebSocket server sends a *Hello* message to the client when a new connection is established, and then echos a capitalized version of every message it receives back to the client. You can test this webserver with the `wscat` tool included with the `ws` module. To make it convenient to use, first install the `ws` module globally:

```
npm install ws -g
```

Then start the .NET application conatining the WebSocket server and establish a connection to it with `wscat`:

```
C:\projects\websockets> wscat -c ws://localhost:8080/

connected (press CTRL+C to quit)

< Hello!
> foo
< FOO
> bar
< BAR
```

A self-contained Node.js WebSocket server, even if running within a .NET application, is rather unexciting. After all, the same could be accomplished with a stand-alone Node.js process. Ideally you could extablish a WebSocket server in Node.js, but handle the messages in .NET. Let's do it - read on. 

### How to: handle Node.js events in .NET

It is often useful to handle certain events raised by the Node.js code within .NET. For example, you may want to establish a WebSocket server in Node.js, and handle the incoming messages in the .NET part of your application. This can be accomplished by passig a .NET callback function to Node.js when the the WebSocket server is created:

```c#
class Program
{
    public static async void Start()
    {
        // Define an event handler to be called for every message from the client

        var onMessage = (Func<object, Task<object>>)(async (message) =>
        {
            return "Received string of length " + ((string)message).Length;
        });

        // The WebSocket server delegates handling of messages from clients
        // to the supplied .NET handler

        var createWebSocketServer = Edge.Func(@"
            var WebSocketServer = require('ws').Server;

            return function (options, cb) {
                var wss = new WebSocketServer({ port: options.port });
                wss.on('connection', function (ws) {
                    ws.on('message', function (message) {
                        options.onMessage(message, function (error, result) {
                            if (error) throw error;
                            ws.send(result);
                        });
                    });
                    ws.send('Hello!');
                });
                cb();
            };
        ");

        // Create a WebSocket server on a specific TCP port and using the .NET event handler

        await createWebSocketServer(new
        {
            port = 8080,
            onMessage = onMessage
        });
    }

    static void Main(string[] args)
    {
        Task.Run((Action)Start);
        new ManualResetEvent(false).WaitOne();
    }
}
```

Using `wscat`, you can verify the .NET handler is indeed invoked for every websocket message:

```
C:\projects\websockets> wscat -c ws://localhost:8080/

connected (press CTRL+C to quit)

< Hello!
> Foo
< Received string of length 3
> FooBar
< Received string of length 6
```

This example shows how Edge.js can create JavaScript proxies to .NET functions and marshal calls across the V8/CLR boundary in-process. Read more about [data marshaling between Node.js and CLR](#how-to-marshal-data-between-c-and-nodejs).

### How to: expose Node.js state to .NET

In the previous example [a Node.js HTTP server was created and started from .NET](#how-to-use-nodejs-built-in-modules). Suppose at some point you want to stop the HTTP server from your .NET code. Given that all references to it are embedded within Node.js code, it is not possible. However, just as Edge.js can [pass a .NET function to Node.js](#how-to-handle-nodejs-events-in-net), it also can export a Node.js function to .NET. Moreover, that function can be implemented as a closure over Node.js state. This is how it would work:

```c#
var createHttpServer = Edge.Func(@"
    var http = require('http');

    return function (port, cb) {
        var server = http.createServer(function (req, res) {
            res.end('Hello, world! ' + new Date());
        }).listen(port, function (error) {
            cb(error, function (data, cb) {
                server.close();
                cb();
            });
        });
    };
");

var closeHttpServer = (Func<object,Task<object>>)await createHttpServer(8080);
Console.WriteLine(await new WebClient().DownloadStringTaskAsync("http://localhost:8080"));
await closeHttpServer(null);
```

Notice how the `createHttpServer` function, in addition to starting an HTTP server in Node.js, is also returning a .NET proxy to a JavaScript function that allows that server to be stopped. 

### How to: use Node.js in ASP.NET web applications

Using Node.js via Edge.js in ASP.NET web applications is no different than in a .NET console application. The Edge.js NuGet package must be referenced in your ASP.NET web application. If you are using any external Node.js modules, the entire `node_modules` subdirectory structure must be binplaced to the `bin` folder of you web application, and deployed that way to the server. 

### How to: debug Node.js code running in a CLR application

The `EDGE_NODE_PARAMS` environment variable allows you to specify any options that are normally passed via command line to the node executable. This includes the `--debug` options necessary to use [node-inspector](https://github.com/node-inspector/node-inspector) to debug Node.js code. 

### Building Edge.js NuGet package

**Note** This mechanism requires hardening, expect the road ahead to be bumpy. 

These are unstructions for building the Edge.js NuGet package on Windows. The package will support running apps in both x86 and x64 architectures using a selected version of Node.js. The resulting NuGet package is all-inclusive with the only dependency being .NET 4.5. 

Preprequisties:

* Visual Studio 2013 Update 5  
* Node.js (tested with v4.1.1)  
* Python 2.7.x  
* node-gyp (tested with 3.0.1)  

To buid the NuGet package, open the Visual Studio 2013 Developer Command Prompt and call:

```
tools\build_double.bat 4.1.1
```

(you can substitite another version of Node.js, but no other than 0.10.28 were tested).

The script takes several minutes to complete and does the following:

* builds a few helper tools in C#  
* downloads sources of the selected Node.js version  
* downloads nuget.exe from http://nuget.org  
* builds Node.js shared library for the x86 and x64 flavor
* builds Edge.js module for the x86 and x64 flavor
* builds managed EdgeJs.dll library that bootstraps running Node.js in a CLR process and provides the Edge.Func programming model  
* packs everything into a NuGet package

If everything goes well, the resulting NuGet package is located in the `tools\build\nuget` directory. 

### Running tests of scripting Node.js in C#

There are functional tests in `test\double\double_test` and stress tests in `test\double\double_stress`. Before you can compile these tests, you must register the location of the built NuGet package as a local NuGet feed through the NuGet configuration manager in Visual Studio. 

After you have compiled the function tests, the best way to run them is from the command line:

```
C:\projects\edge\test\double\double_tests\bin\Release> mstest /testcontainer:double_test.dll /noisolation
```

After you have compiled the stress tests, simply launch the executable, attach resource monitor to the process, and leave it running to observe stability:

```
C:\projects\edge\test\double\double_stress\bin\Release> double_stress.exe
```

## Use cases and other resources

[Accessing MS SQL from Node.js via Edge.js](https://blog.codeship.com/node-js-sql-server-edge-js/) by [David Neal](https://twitter.com/reverentgeek)  
[Using ASP.NET and React on the server via Edge.js](http://navigation4asp.net/2015/10/13/progressive-enhancement-enhanced/) by [Graham Mendick](https://twitter.com/grahammendick)  

## Contribution and derived work

I do welcome contributions via pull request and derived work. 

The edge module is intended to remain a very small component with core functionality that supports interop between .NET and Node.js. Domain specific functionality (e.g. access to SQL, writing to ETW, writing connect middleware in .NET) should be implemented as separate modules with a dependency on edge. When you have a notable derived work, I would love to know about it to include a pointer here.  

## More

Issues? Feedback? You [know what to do](https://github.com/tjanczuk/edge/issues/new). Pull requests welcome.
