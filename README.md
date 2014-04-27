Edge.js: .NET and Node.js in-process
====

An edge connects two nodes:

![image](https://cloud.githubusercontent.com/assets/822369/2807996/94b3ff4e-cd07-11e3-833c-b0474d25119a.png)

This edge connects Node.js and .NET. V8 and CLR/Mono - in process. On Windows, MacOS, and Linux. 

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

## Before you dive in

See the [Edge.js overview](http://tjanczuk.github.com/edge).  
Read the [Edge.js introduction on InfoQ](http://www.infoq.com/articles/the_edge_of_net_and_node).  
Listen to the [Edge.js podcast on Herdingcode](http://herdingcode.com/herding-code-166-tomasz-janczuk-on-edge-js/). 

![image](https://cloud.githubusercontent.com/assets/822369/2808101/87f73a5c-cd0f-11e3-9f7a-f53be86641be.png)

## Contents

[Introduction](#introduction)  
[What you need](#what-you-need)  
[How to: C# hello, world](#how-to-c-hello-world)  
[How to: integrate C# code into Node.js code](#how-to-integrate-c-code-into-nodejs-code)  
[How to: specify additional CLR assembly references in C# code](#how-to-specify-additional-clr-assembly-references-in-c-code)  
[How to: marshal data between C# and Node.js](#how-to-marshal-data-between-c-and-nodejs)  
[How to: call Node.js from C#](#how-to-call-nodejs-from-c)  
[How to: export C# function to Node.js](#how-to-export-c-function-to-nodejs)  
[How to: script Python in a Node.js application](#how-to-script-python-in-a-nodejs-application)  
[How to: script PowerShell in a Node.js application](#how-to-script-powershell-in-a-nodejs-application)  
[How to: script F# in a Node.js application](#how-to-script-f-in-a-nodejs-application)  
[How to: script T-SQL in a Node.js application](#how-to-script-t-sql-in-a-nodejs-application)  
[How to: support for other CLR languages](#how-to-support-for-other-clr-languages)  
[How to: exceptions](#how-to-exceptions)  
[How to: debugging](#how-to-debugging)  
[Performance](#performance)  
[Building on Windows](#building-on-windows)  
[Building on OSX](#building-on-osx)  
[Building on Ubuntu](#building-on-ubuntu)  
[Running tests](#running-tests)  
[Contribution and derived work](#contribution-and-derived-work)  

## Introduction 

Edge.js allows you to run Node.js and .NET code in one process on Windows, MacOS, and Linux. 

You can call .NET functions from Node.js and Node.js functions from .NET. Edge.js takes care of marshaling data between CLR and V8. Edge.js also reconciles threading models of single threaded V8 and multi-threaded CLR. Edge.js ensures correct lifetime of objects on V8 and CLR heaps. The CLR code can be pre-compiled or specified as C#, F#, Python, or PowerShell source: Edge.js can compile CLR scripts at runtime. Edge can be extended to support other CLR languages or DSLs.

![Edge.js interop model](https://f.cloud.github.com/assets/822369/234085/b305625c-8768-11e2-8de0-e03ae98e7249.PNG)

Edge.js provides an asynchronous, in-process mechanism for interoperability between Node.js and .NET. You can use this mechanism to:

* access MS SQL from Node.js using ADO.NET [more...](http://blog.codeship.io/2014/04/22/leverage-sql-server-with-node-js-using-edge-js.html)  
* use CLR multi-threading from Node.js for CPU intensive work [more...](http://tomasz.janczuk.org/2013/02/cpu-bound-workers-for-nodejs.html)  
* write native extensions to Node.js in C# instead of C/C++  
* intergate existing .NET components into Node.js applications

Read more about the background and motivations of the project [here](http://tomasz.janczuk.org/2013/02/hosting-net-code-in-nodejs-applications.html). 

[Follow @tjanczuk](https://twitter.com/tjanczuk) for updates related to the module. 

## What you need

Edge.js runs on Windows, Linux, and MacOS and requires Node.js 0.8 or later, as well as .NET Framework 4.5. or Mono 3.4.0. 

### Windows

* Node.js 0.8.x or later (developed and tested with v0.8.22, and v0.10.0, both x32 and x64 architectures)  
* [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653)  
* to use Python, you also need [IronPython 2.7.3 or later](http://ironpython.codeplex.com/releases/view/81726)  
* to use F#, read [Dave Thomas blog post](http://7sharpnine.com/posts/i-node-something/)

![image](https://cloud.githubusercontent.com/assets/822369/2808066/3707f37c-cd0d-11e3-9b4e-7257ffc27c9c.png)

### Linux

* Node.js 0.8.x or later (developed and tested with v0.10.26 x64)  
* Mono 3.4.0 x64  
* Check out [Ubuntu 12.04 setup instructions](#building-on-ubuntu)

![image](https://cloud.githubusercontent.com/assets/822369/2808077/03f92874-cd0e-11e3-88ea-79f67b8b1d49.png)

### MacOS  

* Node.js 0.8.x or later (developed and tested with v0.10.26 x64)  
* Mono 3.4.0 x64
* Check out [Mac OS setup instructions](#building-on-osx)  

![image](https://cloud.githubusercontent.com/assets/822369/2808046/8f4ce378-cd0b-11e3-95dc-ef0842c28821.png)

## How to: C# hello, world

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

## How to: integrate C# code into Node.js code

Edge provides several ways to integrate C# code into a Node.js application. Regardless of the way you choose, the entry point into the .NET code is normalized to a `Func<object,Task<object>>` delegate. This allows Node.js code to call .NET asynchronoulsy and avoid blocking the Node.js event loop. 

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

The `true` parameter instead of a callback indicates that Node.js expects the C# implementation to complete synchronsouly. If the CLR function implementation does not complete synchronously, the call above will result in an exception. 

One representation of CLR code that Edge.js accepts is C# source code. You can embed C# literal representing a .NET async lambda expression implementing the `Func<object,Task<object>>` delegate directly inside Node.js code:

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

If your C# code grows substantially, it is useful to keep it in a separate file. You can save it to a file with `*.csx` or `*.cs` extension, and then reference from your Node.js application:

```javascript
var add7 = edge.func(require('path').join(__dirname, 'add7.csx'));
```

If you integrate C# code into your Node.js application by specifying C# source using one of the methods above, edge will compile the code on the fly. If you prefer to pre-compile your C# sources to a CLR assembly, or if your C# component is already pre-compiled, you can reference a CLR assembly from your Node.js code. In the most generic form, you can specify the assembly file name, the type name, and the method name when creating a Node.js proxy to a .NET method:

```javascript
var clrMethod = edge.func({
    assemblyFile: 'My.Edge.Samples.dll',
    typeName: 'Samples.FooBar.MyType',
    methodName: 'MyMethod'
});
```

If you don't specify methodName, `Invoke` is assumed. If you don't specify typeName, a type name is constucted by assuming the class called `Startup` in the namespace equal to the assembly file name (without the `.dll`). In the example above, if typeName was not specified, it would default to `My.Edge.Samples.Startup`.

The assemblyFile is relative to the working directory. If you want to locate your assembly in a fixed location relative to your Node.js application, it is useful to constuct the assemblyFile using `__dirname`. 

You can also create Node.js proxies to .NET functions specifying just the assembly name as a parameter:

```javascript
var clrMethod = edge.func('My.Edge.Samples.dll');
```

In that case the default typeName of `My.Edge.Samples.Startup` and methodName of `Invoke` is assumed as explained above. 

## How to: specify additional CLR assembly references in C# code

When you provide C# source code and let edge compile it for you at runtime, edge will by default reference only mscorlib.dll and System.dll assemblies. In applications that require additional assemblies you can specify them in C# code using a special hash pattern, similar to Roslyn. For example, to use ADO.NET you must reference System.Data.dll:

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

## How to: marshal data between C# and Node.js

Edge.js can marshal any JSON-serializable value between .NET and Node.js (although JSON serializaton is not used in the process). Edge also supports marshaling between Node.js `Buffer` instance and a CLR `byte[]` array to help you efficiently pass binary data.

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

Similar type marshaling is applied when .NET code passes data back to Node.js code. In .NET code you can provide an instance of any CLR type that would normally be JSON serializable, including domain specific types like `Person` or anonymous objects. For example:

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

When data is marshaled from .NET to Node.js, no checks for circular references are made. They will typically result in stack overflows. Make sure the object graph you are passing from .NET to Node.js is a tree and does not contain any cycles. 

**WINDOWS ONLY** When marshaling strongly typed objects (e.g. Person) form .NET to Node.js, you can optionaly tell Edge.js to observe the [System.Web.Script.Serialization.ScriptIgnoreAttribute](http://msdn.microsoft.com/en-us/library/system.web.script.serialization.scriptignoreattribute.aspx). You opt in to this behavior by setting the `EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE` environment variable:

```
set EDGE_ENABLE_SCRIPTIGNOREATTRIBUTE=1
```

Edge.js by default does not observe the ScriptIgnoreAttribute to avoid the associated performance cost. 

## How to: call Node.js from C#  

In addition to marshaling data, edge can marshal proxies to JavaScript functions when invoking .NET code from Node.js. This allows .NET code to call back into Node.js. 

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

The Node.js function exported from Node.js to .NET must follow the prescriptive async pattern of accepting two parameters: payload and a callback. The callback function accepts two parametrs. The first one is the error, if any, and the second the result of the operation:

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

Using TPL in CLR to provide a proxy to an asynchronous Node.js function allows the .NET code to use the convenience of the `await` keyword when invoking the Node.js functionality. The example above shows the use of the `await` keyword when calling the proxy of the Node.js `add` method.  

## How to: export C# function to Node.js

Similarly to marshaling functions from Node.js to .NET, Edge.js can also marshal functions from .NET to Node.js. The .NET code can export a `Func<object,Task<object>>` delegate to Node.js as part of the return value of a .NET method invocation. For example:

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

## How to: script Python in a Node.js application

**NOTE** This functionality requires IronPython and has been tested on Windows only. 

Edge.js enables you to run Python and Node.js in-process.

In addition to [platform specific prerequisities](#what-you-need) you need [IronPython 2.7.3](http://ironpython.codeplex.com/releases/view/81726) to proceed.

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
$>node py.js
Python welcomes Node.js
```

### The interop model

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

### Python in its own file

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

### To sync or to async, that is the question

In the examples above Pythion script was executing asynchronously on its own thread without blocking the singleton V8 thread on which the Node.js event loop runs. This means your Node.js application remains reponsive while the Python code executes in the background. 

If know your Python code is non-blocking, or if your know what your are doing, you can tell Edge.js to execute Python code on the singleton V8 thread. This will improve performance for non-blocking Python scripts embedded in a Node.js application:

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

## How to: script PowerShell in a Node.js application

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

### Tapping into PowerShell's ecosystem

Rather than embedding PowerShell directly, you can use PowerShell files, dot source them and even use *Import-Module*.

What you can do in native PowerShell works in Node.js.

### Interop PowerShell and Python

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

## How to: script F# in a Node.js application

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

## How to: script T-SQL in a Node.js application

**NOTE** This functionality has only been tested on Windows. Although ADO.NET exist in Mono, your mileage can vary. 

The edge-sql extension of Edge.js allows for accessing MS SQL databases by scripting T-SQL inside the Node.js application. The edge-sql extension uses async ADO.NET SQL client to access MS SQL. 

You need Windows, [Node.js](http://nodejs.org), and [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653). To run the sample code below you also need a connection string to the sample Northwind database that ships with MS SQL. 

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

Edge.js can work with any pre-compiled CLR assembly that contains the `Func<object,Task<object>>` delegate. Out of the box, Edge.js also allows you to embed C# source code in a Node.js application and compile it on the fly. 

To enable compilation of other CLR languages (e.g. F#) at runtime, or to support domain specific languages (DSLs) like T-SQL, you can use the compiler composibility model provided by Edge.js. Please read the [add support for a CLR language](https://github.com/tjanczuk/edge/wiki/Add-support-for-a-CLR-language) guide to get started. 

## How to: exceptions

Edge.js marshals Node.js errors and exceptions to .NET as well as .NET exceptions to Node.js. 

CLR exceptions thrown in .NET code invoked from Node.js are marshaled as the `error` parameter to the Node.js callback function. Consider this example:

```javascript
var edge = require('edge');

var clrFunc = edge.func(function () {/*
    async (dynamic input) => {
        throw new Exception("Sample exception");
    }
*/});

clrFunc(null, function (error, result) {
    if (error) throw error;
});
```

Running this Node.js application shows that the CLR exception was indeed received by the Node.js callback. The `error` parameter contains the full stack trace including the CLR code path:

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

## How to: debugging

**NOTE** This is Windows-only functionality.

On Windows, you can debug the .NET code running as part of your Node.js application by attaching a managed code debugger (e.g. Visual Studio) to node.exe. You can debug .NET code in a pre-compiled CLR assembly as well C# literals embedded in the application and compiled by Edge.js at runtime. 

### Debugging pre-compiled .NET code

If you have integrated .NET code into a Node.js application using a pre-compiled CLR assembly like this:

```javascript
var hello = edge.func('My.Assembly.dll');
```

then the best way to debug your .NET code is to attach a managed code debugger (e.g. Visual Studio) to the node.exe process. Since the node.exe process runs both native and managed code, make sure to select managed code type as target:

![debug](https://f.cloud.github.com/assets/822369/190564/a41bab2c-7efb-11e2-878f-82ae2325876c.PNG)

From there, you can set breakpoints in your .NET code and the debugger will stop when they are reached.

### Debugging embedded C# code

Debugging embedded C# code (on Windows) requires that `EDGE_CS_DEBUG` environment variable is set in the environment of the node.exe process:

```
set EDGE_CS_DEBUG=1
```

Without this setting (the default), Edge.js will not generate debugging information when compiling embedded C# code.

You can debug C# code embedded into a Node.js application using a reference to a *.cs or *.csx file:

```javacript
var hello = edge.func('MyClass.cs');
```

You can also debug C# code embeeded directly into a *.js file using the function comment syntax:

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

## Performance

Read more about [performance of Edge.js on the wiki](https://github.com/tjanczuk/edge/wiki/Performance). Here is the gist of the latency (smaller is better):

![edgejs-performance1](https://f.cloud.github.com/assets/822369/486393/645f696a-b920-11e2-8a20-9fa6932bb092.png)

## Building on Windows

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

Note: the Node.js version number you provide must be version number corresponding to one of the subdirectories of http://nodejs.org/dist. The command will build both x32 and x64 architectures (assuming you use x64 machine). The command will also copy the edge.node executables to appropriate locations under lib\native directory where they are looked up from at runtime. The `npm install` step copies the C standard library shared DLL to the location of the edge.node for the component to be ready to go.

To build the C++\CLI native extension using the version of Node.js installed on your machine, issue the followig command:

```
npm install -g node-gyp
node-gyp configure --msvs_version=2013
node-gyp build -debug
```

You can then set the EDGE_NATIVE environment variable to the fully qualified file name of the built edge.node binary. It is useful during development, for example:

```
set EDGE_NATIVE=C:\projects\edge\build\Debug\edge.node
``` 

You can also set the `EDGE_DEBUG` environment variable to 1 to have the edge module generate debug traces to the console when it runs.

## Running tests

You must have mocha installed on the system. Then:

```
npm test
```

or, from the root of the enlistment:

```
mocha -R spec
```

### Node.js version targeting on Windows

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

## Building on OSX

Prerequisities:

* [Homebrew](http://brew.sh/)  
* [Node.js x64](http://nodejs.org/) (tested with v0.10.26)  

First build Mono x64:

```bash
brew install https://raw.githubusercontent.com/tjanczuk/edge/master/tools/mono64.rb

# at this point I fried an omelette on the lid of my Mac Book Air which was running hot 
# for about 15 minutes, compiling and installing Mono 64bit; 
# it was delicious (the omelette, not the MacBook)
```

Then install and build Edge.js:

```bash
brew install pkg-config
sudo npm install node-gyp -g
npm install edge
```

To build edge from a clone of the repository or source code:

```bash
node-gyp configure build
```

To build a debug build instead of release, you need to:

```bash
node-gyp configure build -debug
export EDGE_NATIVE=/Users/tomek/edge/build/Debug/edge.node
```

## Building on Ubuntu

These instructions were tested on Ubuntu 12.04 x64. High level, you must have Node.js x64 and Mono x64 installed on the machine before you can install Edge.js. There are two ways of getting there.

### Ubuntu, starting from a clean VM (i.e. taking the high road)

If you have a fresh Ubuntu 12.04 x64 installation, the most convenient way of installing Edge.js with all prerequisities is by running:

```bash
sudo bash -c 'bash <(wget -qO- https://raw.githubusercontent.com/tjanczuk/edge/master/tools/ubuntu_12.04_clean_install.sh)'
```

This will do the following:

* Download Node.js v0.10.26 sources, build, and install Node.js x64  
* Download Mono 3.4.0 sources, build, and install Mono x64  
* Download and install node-gyp and mocha  
* Download Edge.js sources and build x64 release  
* Run Edge.js tests  

This process takes about 25 minutes on a Ubuntu 12.04 x64 VM running on a 2 core VM with 4GB RAM within Fusion on a MacBook Pro. If successful, your machine will have all the prerequisities to `npm install edge`. 

### Ubuntu, manual install 

This method is adequate if you already have a Mono x64 or Node.js x64 install on the machine and need to incrementally add Edge to it. 

Read through the [install script](https://raw.githubusercontent.com/tjanczuk/edge/mono/tools/ubuntu_12.04_clean_install.sh) and cherry pick the steps you need. Here are some gotchas:

* The Mono 3.4.0 source code tarball misses one file which the install script manually adds. For background on the missing file see [here](http://stackoverflow.com/questions/22844569/build-error-mono-3-4-0-centos).  
* If you need to build Mono, make sure to run `ldconfig` afterwards, otherwise the garbage collection libraries may not load.  
* To make sure Mono can load the standard C library, run `sudo ln -s -f /lib/x86_64-linux-gnu/libc.so.6 /lib/x86_64-linux-gnu/libc.so`. For background on that step, see how [Mono loads native libraries](http://www.mono-project.com/Interop_with_Native_Libraries) or just [cut to the chase](http://stackoverflow.com/questions/14359981/mono-and-unmanaged-code-in-ubuntu).  

### Debug build of Edge.js

To build a debug build instead of release, you need to:

```bash
node-gyp configure build -debug
export EDGE_NATIVE=/home/tomek/edge/build/Debug/edge.node
```

## Contribution and derived work

I do welcome contributions via pull request and derived work. 

The edge module is intended to remain a very small component with core functionality that supports interop between .NET and Node.js. Domain specific functionality (e.g. access to SQL, writing to ETW, writing connect middleware in .NET) should be implemented as separate modules with a dependency on edge. When you have a notable derived work, I would love to know about it to include a pointer here.  

## More

Issues? Feedback? You [know what to do](https://github.com/tjanczuk/edge/issues/new). Pull requests welcome.
