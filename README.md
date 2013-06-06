Edge.js: run .NET and Node.js code in-process
====

An edge connects two nodes. This edge connects Node.js and .NET. V8 and CLR. Node.js, Python, and C# - in process.

## Before you dive in

See the [Edge.js overview](http://tjanczuk.github.com/edge).  
Read the [Edge.js introduction on InfoQ](http://www.infoq.com/articles/the_edge_of_net_and_node).  
Listen to the [Edge.js podcast on Herdingcode](http://herdingcode.com/herding-code-166-tomasz-janczuk-on-edge-js/). 

## Contents

[Introduction](https://github.com/tjanczuk/edge#introduction)  
[What you need](https://github.com/tjanczuk/edge#what-you-need)  
[How to: C# hello, world](https://github.com/tjanczuk/edge#how-to-c-hello-world)  
[How to: integrate C# code into Node.js code](https://github.com/tjanczuk/edge#how-to-integrate-c-code-into-nodejs-code)  
[How to: specify additional CLR assembly references in C# code](https://github.com/tjanczuk/edge#how-to-specify-additional-clr-assembly-references-in-c-code)  
[How to: marshal data between C# and Node.js](https://github.com/tjanczuk/edge#how-to-marshal-data-between-c-and-nodejs)  
[How to: call Node.js from C#](https://github.com/tjanczuk/edge#how-to-call-nodejs-from-c)  
[How to: export C# function to Node.js](https://github.com/tjanczuk/edge#how-to-export-c-function-to-nodejs)  
[How to: script Python in a Node.js application](https://github.com/tjanczuk/edge#how-to-script-python-in-a-nodejs-application)  
[How to: script PowerShell in a Node.js application](https://github.com/tjanczuk/edge#how-to-script-powershell-in-a-nodejs-application)  
[How to: script F# in a Node.js application](https://github.com/tjanczuk/edge#how-to-script-f-in-a-nodejs-application)  
[How to: script T-SQL in a node.js application](https://github.com/tjanczuk/edge#how-to-script-t-sql-in-a-nodejs-application)  
[How to: support for other CLR languages](https://github.com/tjanczuk/edge#how-to-support-for-other-clr-languages)  
[How to: exceptions](https://github.com/tjanczuk/edge#how-to-exceptions)  
[How to: debugging](https://github.com/tjanczuk/edge#how-to-debugging)  
[Performance](https://github.com/tjanczuk/edge#performance)  
[Building](https://github.com/tjanczuk/edge#building)  
[Running tests](https://github.com/tjanczuk/edge#running-tests)  
[Contribution and derived work](https://github.com/tjanczuk/edge#contribution-and-derived-work)  

## Introduction 

Edge.js allows you to run .NET and Node.js code in one process. You can call .NET functions from Node.js and Node.js functions from .NET. Edge.js takes care of marshaling data between CLR and V8. Edge.js also reconciles threading models of single threaded V8 and multi-threaded CLR. Edge.js ensures correct lifetime of objects on V8 and CLR heaps. The CLR code can be pre-compiled or specified as C#, F#, Python, or PowerShell source: edge.js can run CLR scripts at runtime. Edge can be extended to support other CLR languages.

![F# Python C# Node.js](https://f.cloud.github.com/assets/822369/468830/a293c728-b6a3-11e2-9fb3-99c7bf2bf6ed.png)

Edge.js provides a basic, prescriptive model and implementation for interoperability between .NET and Node.js in-process. You can built upon and extended this basic mechanism to support more specific scenarios, for example:
* implementing express.js handlers and connect middleware for Node.js application using .NET 4.5 ([read more](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html)),  
* implementing CPU-bound computations in .NET and running them in-process with Node.js application without blocking the event loop ([read more](http://tomasz.janczuk.org/2013/02/cpu-bound-workers-for-nodejs.html)),  
* using C# and IronPython and .NET instead of writing native Node.js extensions in C/C++ and Win32 to access Windows specific functionality from a Node.js application ([read more](http://tomasz.janczuk.org/2013/02/access-ms-sql-from-nodejs-application.html)). 

Read more about the background and motivations of the project [here](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html). 

[Follow @tjanczuk](https://twitter.com/tjanczuk) for updates related to the module. 

## What you need

* Windows
* node.js 0.6.x or later (developed and tested with v0.6.20, v0.8.22, and v0.10.0, both x32 and x64 architectures)  
* [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653)  
* to use Python, you also need [IronPython 2.7.3 or later](http://ironpython.codeplex.com/releases/view/81726)  
* to use F#, read [Dave Thomas blog post](http://7sharpnine.com/posts/i-node-something/)

## How to: C# hello, world

Install edge:

```
npm install edge
```

In your server.js:

```javascript
var edge = require('edge');

var helloWorld = edge.func('async (input) => { return ".NET Welcomes " + input.ToString(); }');

helloWorld('JavaScript', function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Run and enjoy:

```
C:\projects\barebones>node server.js
.NET welcomes JavaScript
```

## How to: integrate C# code into node.js code

Edge provies several ways to integrate C# code into a node.js application. Regardless of the way you choose, the entry point into the .NET code is normalized to a `Func<object,Task<object>>` delegate. This allows node.js code to call .NET asynchronoulsy and avoid blocking the node.js event loop. 

Edge provides a function that accepts a reference to C# code in one of the supported representations, and returns a node.js function which acts as a JavaScript proxy to the `Func<object,Task<object>>` .NET delegate:

```javascript
var edge = require('edge');

var myFunction = edge.func(...);
```

The function proxy can then be called from node.js like any asynchronous function:

```javascript
myFunction('Some input', function (error, result) {
    //...
});
```

Alternatively, if you know the C# implementation will complete synchronously given the circumstances, you can call this function as any synchronous JavaScript function as follows:

```javascript
var result = myFunction('Some input', true);
```

The `true` parameter instead of a callback indicates that node.js expects the C# implementation to complete synchronsouly. If the CLR function implementation does not complete synchronously, the call above will result in an exception. 

One representation of CLR code that edge.js accepts is C# source code. You can embed C# literal representing a .NET async lambda expression implementing the `Func<object,Task<object>>` delegate directly inside node.js code:

```javascript
var add7 = edge.func('async (input) => { return (int)input + 7; }');
``` 

In antoher representation, you can embed multi-line C# source code by providing a function with a body containing a multi-line comment. Edge extracts the C# code from the function body using regular expressions:

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
            return Helper::AddSeven(v);
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

If your C# code grows substantially, it is useful to keep it in a separate file. You can save it to a file with `*.csx` or `*.cs` extension, and then reference from your node.js application:

```javascript
var add7 = edge.func(__dirname + '/add7.csx');
```

If you integrate C# code into your node.js application by specifying C# source using one of he methods above, edge will compile the code on the fly. If you prefer to pre-compile your C# sources to a CLR assembly, or if your C# component is already pre-compiled, you can reference a CLR assembly from your node.js code. In the most generic form, you can specify the assembly file name, the type name, and the method name when creating a node.js proxy to a .NET method:

```javascript
var clrMethod = edge.func({
    assemblyFile: 'My.Edge.Samples.dll',
    typeName: 'Samples.FooBar.MyType',
    methodName: 'MyMethod'
});
```

If you don't specify methodName, `Invoke` is assumed. If you don't specify typeName, a type name is constucted by assuming the class called `Startup` in the namespace equal to the assembly file name (without the `.dll`). In the example above, if typeName was not specified, it would default to `My.Edge.Samples.Startup`.

The assemblyFile is relative to the working directory. If you want to locate your assembly in a fixed location relative to your node.js application, it is useful to constuct the assemblyFile using `__dirname`. 

You can also create node.js proxies to .NET functions specifying just the assembly name as a parameter:

```javascript
var clrMethod = edge.func('My.Edge.Samples.dll');
```

In that case the default typeName of `My.Edge.Samples.Startup` and methodName of `Invoke` is assumed as explained above. 

## How to: specify additional CLR assembly references in C# code

When you provide C# source code and let edge compile it for you at runtime, edge will by default reference only mscorlib.dll and System.dll assemblies. In applications that require additional assemblies you can specify them in C# code using a special comment pattern. For example, to use ADO.NET you must reference System.Data.dll:

```javascript
var add7 = edge.func(function() {/*

    //#r "System.Data.dll"

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

## How to: marshal data between C# and node.js

Edge.js can marshal any JSON-serializable value between .NET and node.js (although JSON serializaton is not used in the process). Edge also supports marshaling between node.js `Buffer` instance and a .NET `byte[]` array to help you efficiently pass binary data.

You can call .NET from node.js and pass in a complex JavaScript object as follows:

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

In .NET, JavaScript objects are represented as `IDictionary<string,object>`, JavaScript arrays as `object[]`, and JavaScript `Buffer` as `byte[]`. Scalar JavaScript values have their corresponding .NET types (`int`, `double`, `bool`, `string`). Here is how you can acces the data in .NET:

```c#
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Edge.Sample
{
    public class Startup
    {
        public async Task<object> Invoke(object input)
        {
            IDictionary<string, object> payload = (IDictionary<string,object>)input;
            int anInteger = (int)payload["anInteger"];
            double aNumber = (double)payload["aNumber"];
            string aString = (string)payload["aString"];
            bool aBoolean = (bool)payload["aBoolean"];
            byte[] aBuffer = (byte[])payload["aBuffer"];
            object[] anArray = (object[])payload["anArray"];
            IDictionary<string, object> anObject = (IDictionary<string,object>)payload["anObject"];

            return null;
        }
    }
}
```

Similar type marshaling is applied when .NET code passes data back to node.js code. In .NET code you can provide an instance of any CLR type that would normally be JSON serializable, including domain specific types like `Person` or anonymous objects. For example:

```c#
using System.Threading.Tasks;

namespace Edge.Sample
{
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
        public async Task<object> Invoke(object input)
        {
            Person person = new Person();
            return person;
        }
    }
}
```

In your node.js code that invokes this .NET method you can display the result object that the callback method receives:

```javascript
var edge = require('edge');

var getData = edge.func('Edge.Sample.dll');

getData(null, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Passing this .NET object to node.js generates a JavaScript object as follows:

```
C:\projects\barebones>node sample.js
{ anInteger: 1,
  aNumber: 3.1415,
  aString: 'foo',
  aBoolean: true,
  aBuffer: <Buffer 00 00 00 00 00 00 00 00 00 00>,
  anArray: [ 1, 'foo' ],
  anObject: { a: 'foo', b: 12 } }
```

When data is marshaled from .NET to node.js, no checks for circular references are made. They will typically result in stack overflows. Make sure the object graph you are passing from .NET to node.js is a tree and does not contain any cycles. 

When marshaling strongly typed objects (e.g. Person) form .NET to node.js, you can optionaly tell edge.js to observe the [System.Web.Script.Serialization.ScriptIgnoreAttribute](http://msdn.microsoft.com/en-us/library/system.web.script.serialization.scriptignoreattribute.aspx). You opt in to this behavior by setting the `EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE` environment variable:

```
set EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE=1
```

Edge.js by default does not observe the ScriptIgnoreAttribute to avoid the associated performance cost. 

## How to: call node.js from C#  

In addition to marshaling data, edge can marshal proxies to JavaScript functions when invoking .NET code from node.js. This allows .NET code to call back into node.js. 

Suppose the node.js application passes an `add` function to the .NET code as a property of an object. The function receives two numbers and returns the sum of them via the provided callback:

```javascript
var edge = require('edge');

var multiplyBy2 = edge.func('Edge.Sample.dll');

var payload = {
    someParameter: 'arbitrary parameter',
    add: function (data, callback) {
        callback(null, data.a + data.b);
    }
};

multiplyBy2(payload, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

The .NET code implements the multiplyBy2 function. It generates two numbers, calls back into the `add` function exported from node.js to add them, multiples the result by 2 in .NET, and returns the result back to node.js:

```c#
using System;
using System.Collections.Generic;
using System.Threading.Tasks;

namespace Edge.Sample
{
    public class Startup
    {
        public async Task<object> Invoke(IDictionary<string, object> input)
        {
            Func<object, Task<object>> add = (Func<object, Task<object>>)input["add"];
            var twoNumbers = new { a = 2, b = 3 };
            var addResult = (int)await add(twoNumbers);
            return addResult * 2;
        }
    }
}
```

The node.js function exported from node.js to .NET must follow the prescriptive async pattern of accepting two parameters: payload and a callback. The callback function accepts two parametrs. The first one is the error, if any, and the second the result of the operation:

```javasctipt
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

Using TPL in CLR to provide a proxy to an asynchronous node.js function allows the .NET code to use the convenience of the `await` keyword when invoking the node.js functionality. The example above shows the use of the `await` keyword when calling the proxy of the node.js `add` method.  

## How to: export C# function to node.js

Similarly to marshaling functions from node.js to .NET, edge.js can also marshal functions from .NET to node.js. The .NET code can export a `Func<object,Task<object>>` delegate to node.js as part of the return value of a .NET method invocation. For example:

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

## How to: script Python in a node.js application

Edge.js enables you to run Python and node.js in-process.

You need Windows, [node.js](http://nodejs.org) (any stable version 0.6.x or later), [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653), and [IronPython 2.7.3](http://ironpython.codeplex.com/releases/view/81726) to proceed.

### Hello, world

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
C:\projects\edgerepro>node py.js
Python welcomes Node.js
```

### The interop model

Your Python script must evaluate to a lamda expression that accepts a single parameter. The parameter represents marshalled input from the node.js code. The return value of the lambda expression is passed back as the result to node.js code. The Python script can contain constructs (e.g. Python functions) that are used in the closure of the lambda expression. The instance of the script with associated state is created when `edge.func` is called in node.js. Each call to the function referes to that instance.

The simplest *echo* Python script you can embed in node.js looks like this:

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

### Python in its own file

You can reference Python script stored in a *.py file instead of embedding Python code in a node.js script.

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
C:\projects\edgerepro>node hello.js
Python welcomes Node.js
```

### To sync or to async, that is the question

In the examples above Pythion script was executing asynchronously on its own thread without blocking the singleton V8 thread on which the node.js event loop runs. This means your node.js application remains reponsive while the Python code executes in the background. 

If know your Python code is non-blocking, or if your know what your are doing, you can tell edge.js to execute Python code on the singleton V8 thread. This will improve performance for non-blocking Python scripts embedded in a node.js application:

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

The `sync: true` property in the call to `edge.func` tells edge.js to execute Python code on the V8 thread as opposed to creating a new thread to run Python script on. The `true` parameter in the call to `hello` requests that edge.js does in fact call the `hello` function synchronously, i.e. return the result as opposed to calling a callback function. 

## How to: script PowerShell in a node.js application

Edge.js enables you to run PowerShell and node.js in-process. [Edge-PS](https://github.com/dfinke/edge-ps) connects the PowerShell ecosystem with 24k+ npm modules and more.

You need Windows, [node.js](http://nodejs.org) (any stable version 0.6.x or later), [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653), and [PowerShell 3.0](http://www.microsoft.com/en-us/download/details.aspx?id=34595) to proceed.

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

### Tapping into PowerShell's ecosystem

Rather than embedding PowerShell directly, you can use PowerShell files, dot source them and even use *Import-Module*.

What you can do in native PowerShell works in node.js.

### Interop PowerShell and Python

Here you can reach out to IronPython from PowerShell from within node.js. This holds true for working with JavaScript frameworks and C#.

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

## How to: script F# in a node.js application

This section is coming up. In the meantime please refer to [Dave Thomas blog post](http://7sharpnine.com/posts/i-node-something/).

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

## How to: script T-SQL in a node.js application

The edge-sql extension of edge.js allows for accessing MS SQL databases by scripting T-SQL inside the Node.js application. The edge-sql extension uses async ADO.NET SQL client to access MS SQL. 

You need Windows, [node.js](http://nodejs.org) (any stable version 0.6.x or later), and [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653). To run the sample code below you also need a connection string to the sample Northwind database that ships with MS SQL. 

### Hello, world

Install edge and edge-sql modules:

```
npm install edge
npm install edge-sql
```

Set the connection string as an enironment variable (your connection string may be different):

```
set EDGE_SQL_CONNECTION_STRING=Data Source=localhost;Initial Catalog=Northwind;Integrated Security=True
```

In your server.js:

```javascript
var edge = require('edge');

var getTop10Products = edge.func('sql', function () {/*
    select top 10 * from Products 
*/});

getTop10Product(null, function (error, result) {
    if (error) throw error;
    console.log(result);
});
```

Run and enjoy:

```
C:\projects\edge\samples>node server.js
[ [ 'ProductID',
    'ProductName',
    'SupplierID',
    'CategoryID',
    'QuantityPerUnit',
    'UnitPrice',
    'UnitsInStock',
    'UnitsOnOrder',
    'ReorderLevel',
    'Discontinued' ],
  [ 10,
    'New Ikura',
    4,
    8,
    '12 - 200 ml jars',
    '31.0000',
    '31',
    '0',
    '0',
    false ],
    ...
```

### Parameterized queries

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

### Basic CRUD support: select, update, insert, delete

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

## How to: support for other CLR languages

Edge.js can work with any pre-compiled CLR assembly that contains the `Func<object,Task<object>>` delegate. Out of the box, edge.js also allows you to embed C# source code in a node.js application and compile it on the fly. With the use of the `edge-py` module, edge.js can also execute embedded IronPython script. 

To enable compilation of other CLR languages (e.g. F#) at runtime or to support other idioms of constructing C# script, you can use the compiler composibility model provided by edge.js. Please read the [add support for a CLR language](https://github.com/tjanczuk/edge/wiki/Add-support-for-a-CLR-language) guide to get started. 

## How to: exceptions

Edge.js marshals node.js errors and exceptions to .NET as well as .NET exceptions to node.js. 

CLR exceptions thrown in .NET code invoked from node.js are marshaled as the `error` parameter to the node.js callback function. Consider this .NET code:

```c#
public Task<object> Invoke(object input)
{
    throw new Exception("Sample .NET exception");
}
```

And the node.js code that invokes this .NET function and re-throws the `error` parameter passed to the JavaScript callback function:

```javascript
var edge = require('edge');

var clrFunc = edge.func('Edge.Sample.dll');

clrFunc(null, function (error, result) {
    if (error) throw error;
});
```

Running this node.js application shows that the CLR exception was indeed received by the node.js callback. The `error` parameter contains the full stack trace including the CLR code path:

```
C:\projects\barebones>node sample.js

c:\projects\edge\lib\edge.js:58
                edge.callClrFunc(appId, data, callback);
                     ^
System.Reflection.TargetInvocationException: Exception has been thrown by the target of an invocation. 
---> System.Exception: Sample .NET exception
   at Edge.Sample.Startup.Invoke(Object input) in c:\projects\barebones\sample.cs:line 12
``` 

JavaScript exceptions thrown in node.js code invoked from .NET are wrapped in a CLR exception and cause the asynchronous `Task<object>` to complete with a failure. Errors passed by node.js code invoked from .NET code to the callback function's `error` parameter have the same effect. 

This node.js code invokes a .NET routine and exports the `aFunctionThatThrows` JavaScript function to it:

```javascript
var edge = require('edge.js');
var multiplyBy2 = edge.func('Edge.Sample.dll');

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

The .NET code calls the node.js function, catches any resulting CLR exceptions, and displays them:

```c#
public async Task<object> Invoke(object input)
{
    IDictionary<string, object> payload = (IDictionary<string, object>)input;
    Func<object, Task<object>> aFunctionThatThrows = (Func<object, Task<object>>)payload["aFunctionThatThrows"];
    try {
        var aResult = await aFunctionThatThrows(null);
    }
    catch(Exception e)
    {
        Console.WriteLine(e);
    }

    return null;
}
```

Running the code shows the .NET code receiving a CLR exception as a result of the node.js function throwing a JavaScript error. The exception shows the complete stack trace, including the part that executed in the node.js code:

```
C:\projects\barebones>node sample.js
System.Exception: Error: Sample JavaScript error
    at payload.aFunctionThatThrows (C:\projects\barebones\sample.js:7:11)
   at System.Runtime.CompilerServices.TaskAwaiter.ThrowForNonSuccess(Task task)
   at System.Runtime.CompilerServices.TaskAwaiter.HandleNonSuccessAndDebuggerNotification(Task task)
   at Edge.Sample.Startup.<Invoke>d__0.MoveNext()
```

## How to: debugging

You can debug the .NET code running as part of your node.js application by attaching a managed code debugger (e.g. Visual Studio) to node.exe. You can debug .NET code in a pre-compiled CLR assembly as well C# literals embedded in the application and compiled by edge.js at runtime. 

### Debugging pre-compiled .NET code

If you have integrated .NET code into a node.js application using a pre-compiled CLR assembly like this:

```javascript
var hello = edge.func('My.Assembly.dll');
```

then the best way to debug your .NET code is to attach a managed code debugger (e.g. Visual Studio) to the node.exe process. Since the node.exe process runs both native and managed code, make sure to select managed code type as target:

![debug](https://f.cloud.github.com/assets/822369/190564/a41bab2c-7efb-11e2-878f-82ae2325876c.PNG)

From there, you can set breakpoints in your .NET code and the debugger will stop when they are reached.

### Debugging embedded C# code

Debugging embedded C# code requires that `EDGE_CS_DEBUG` environment variable is set in the environment of the node.exe process:

```
set EDGE_CS_DEBUG=1
```

Without this setting (the default), edge.js will not generate debugging information when compiling embedded C# code.

You can debug C# code embedded into a node.js application using a reference to a *.cs or *.csx file:

```javacript
var hello = edge.func('MyClass.cs');
```

You can also debug C# code embeeded directly into a *.js file using the function comment syntax:

```javscript
var hello = edge.func(function () {/*
    async (input)
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

## Performance

Read more about [performance of edge.js on the wiki](https://github.com/tjanczuk/edge/wiki/Performance). Here is the gist of the latency (smaller is better):

![edgejs-performance1](https://f.cloud.github.com/assets/822369/486393/645f696a-b920-11e2-8a20-9fa6932bb092.png)

## Building

You must have Visual Studio 2012 toolset, Python 2.7.x, and node-gyp installed for building.

To build and test the project against all supported versions of node.js in x86 and x64 flavors, run the following:

```
tools\buildall.bat
test\testall.bat
npm run jshint
```

To build one of the versions of node.js officially released by [node.js](http://nodejs.org/dist), do the following:

```
cd tools
build.bat release 0.10.0
```

Note: the node.js version number you provide must be version number corresponding to one of the subdirectories of http://nodejs.org/dist. The command will build both x32 and x64 architectures (assuming you use x64 machine). The command will also copy the edge.node executables to appropriate locations under lib\native directory where they are looked up from at runtime. The `npm install` step copies the C standard library shared DLL to the location of the edge.node for the component to be ready to go.

To build the C++\CLI native extension using the version of node.js installed on your machine, issue the followig command:

```
npm install -g node-gyp
node-gyp configure --msvs_version=2012
node-gyp build -debug
```

You can then set the EDGE_NATIVE environment variable to the fully qualified file name of the built edge.node binary. It is useful during development, for example:

```
set EDGE_NATIVE=C:\projects\edge\build\Debug\edge.node
``` 

You can also set the `EDGE_DEBUG` environment variable to 1 to have the edge module generate debug traces to the console when it runs.

## Running tests

You must run tests from a place that has `csc.exe` to VS 2012 tooset on the PATH, for example the VS 2012 developer command prompt. To run the tests using the version node.js installed you your system:

```
npm test
```

This first builds a CLR assembly in C# that contains the .NET code of the tests, and then runs the tests with mocha.

If you want to run tests after building against a specific version of node.js that one of the previous builds used, issue the following command:

```
cd test
test.bat ia32 0.10.0
```

Which will run the tests using node.js x86 v0.1.0. Similarly:

```
cd test
test.bat x64 0.8.22
```

Would run tests against node.js 0.8.22 on x64 architecture.

Lastly, you can run jshint on the project with:

```
npm run jshint
```

## Contribution and derived work

I do welcome contributions via pull request and derived work. 

The edge module is intended to remain a very small component with core functionality that supports interop between .NET and node.js. Domain specific functionality (e.g. access to SQL, writing to ETW, writing connect middleware in .NET) should be implemented as separate modules with a dependency on edge. When you have a notable derived work, I would love to know about it to include a pointer here.  

## More

Issues? Feedback? You [know what to do](https://github.com/tjanczuk/edge/issues/new). Pull requests welcome.
