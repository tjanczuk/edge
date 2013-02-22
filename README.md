Hosting .NET OWIN applications in node.js
====

Owin allows you to implement express.js handlers and connect middleware for node.js application using .NET 4.5 languages. 

Owin is a native node.js module for Windows. It hosts [OWIN](http://owin.org/) handlers written in .NET 4.5 (think C#) in a node.js application. Owin allows integration of .NET code into express.js applications by providing a connect wrapper around OWIN .NET handlers. 

## What you need

* Windows x64  
* node.js 0.8.x x64 (developed and tested with [v0.8.19](http://nodejs.org/dist/v0.8.19/))  
* [.NET 4.5](http://www.microsoft.com/en-us/download/details.aspx?id=30653)  

## Hello, world

First get the owin module and express.js

```
npm install owin
npm install express
```

Implement your Startup.cs handler in .NET as follows:

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
            var responseHeaders = new Dictionary<string, string[]>();
            responseHeaders["Content-Type"] = new string[] { "text/html" };
            env["owin.ResponseHeaders"] = responseHeaders;
            env["owin.ResponseBody"] = new MemoryStream(
                Encoding.UTF8.GetBytes("Hello, from C#. Time on server is " + DateTime.Now.ToString()));
            env["owin.ResponseStatusCode"] = 200;

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

Now open the web browser and navigate to `http://localhost:3000/jazz`. Welcome to .NET! Now navigate to `http://localhost:3000/rocknroll`. Hello JavaScript!

## More

Issues? Feedback? You [know what to do](https://github.com/tjanczuk/owin/issues/new). 
Pull requests welcome.