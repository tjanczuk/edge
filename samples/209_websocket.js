// Overview of edge.js: http://tjanczuk.github.io/edge

// This sample shows using Node.js to expose WebSocket protocol to .NET applications.
// It works whenever you can run Node.js and .NET Framework 4.5, including
// Windows Server 2008 and Windows 7 which do not offer native WebSocket support.

var http = require('http')
    , edge = require('../lib/edge')
    , WebSocketServer = require('ws').Server; // npm install ws

// The createMyNetWebSocket function creates an instance of MyNetWebSocket
// class in .NET to handle a single WebSocket connection that Node.js accepted.
// The createMyNetWebSocket function takes a single parameter - a JavaScript function .NET code 
// can call to send a WebSocket message to the client over the WebSocket connection. 
// The createMyNetWebSocket function returns a single value - a .NET function Node.js
// code should call whenever a WebSocket message is received from the client.

var createMyNetWebSocket = edge.func(function () {/*
    using System;
    using System.Threading.Tasks;

    // The NetWebSocket class is an adapter between the prescriptive interop model
    // Edge.js provides and a usable .NET APIs of ReceiveAsync and SendAsync. 

    public abstract class NetWebSocket
    {
        Func<object,Task<object>> SendImpl { get; set; }

        public Func<object,Task<object>> ReceiveImpl 
        {
            get 
            {
                return async (input) => 
                {
                    await this.ReceiveAsync((string)input);
                    return Task.FromResult<object>(null);
                };
            }
        }

        public NetWebSocket(Func<object,Task<object>> sendImpl) {
            this.SendImpl = sendImpl;
        }

        public abstract Task ReceiveAsync(string message);

        public async Task SendAsync(string message)
        {
            await this.SendImpl(message);
            return;
        }
    }

    // The MyNetWebSocket embeds application specific websocket logic:
    // it sends a message back to the client for every message it receives
    // from the client.

    public class MyNetWebSocket : NetWebSocket
    {
        public MyNetWebSocket(Func<object,Task<object>> sendImpl)
            : base(sendImpl) {}

        public override async Task ReceiveAsync(string message) 
        {
            Console.WriteLine(message);
            await this.SendAsync("Hello from .NET server on " + Environment.OSVersion.ToString() + 
                " at " + DateTime.Now.ToString());
            return;
        }
    }

    // The Startup class with Invoke method provide the implementation of the
    // createMyNetWebSocket function. 

    public class Startup
    {
        public async Task<object> Invoke(Func<object,Task<object>> sendImpl)
        {
            var ws = new MyNetWebSocket(sendImpl);
            return ws.ReceiveImpl;
        }
    }
*/});

// Create an HTTP server that returns an HTML page to the browser client.
// The JavaScript on the HTML page establishes a WebSocket connection
// back to the server it came from, and sends a message to the server
// every second. Messages received from the server are displayed in the
// browser.

var server = http.createServer(function (req, res) {
    res.writeHead(200, { 'Content-Type': 'text/html' });
    res.end((function () {/*
        <!DOCTYPE html>
        <html>
          <body>
            <script>
                var ws = new WebSocket('ws://' + window.document.location.host);
                ws.onmessage = function (event) {
                    var div = document.createElement('div');
                    div.appendChild(document.createTextNode(event.data));
                    document.getElementsByTagName('body')[0].appendChild(div);   
                };
                ws.onopen = function () {
                  setInterval(function () {
                    ws.send('Hello from client at ' + new Date().toString());
                  }, 1000);
                };
            </script>
          </body>
        </html>        
    */}).toString().match(/[^]*\/\*([^]*)\*\/\}$/)[1]);
});

// Create a WebSocket server associated with the HTTP server.

var wss = new WebSocketServer({ server : server });

// For every new WebSocket connection, create an instance of the
// WebSocket handler in .NET using the createMyNetWebSocket function.
// Pass all incoming WebSocket messages to .NET, and allow .NET
// to send messages back to the client over the connection.

wss.on('connection', function(ws) {
    var sendImpl = function (message, callback) {
        ws.send(message);
        callback();
    };

    var receiveHandler = createMyNetWebSocket(sendImpl, true);

    ws.on('message', function (message) {
        receiveHandler(message);
    });
});

server.listen(process.env.PORT || 8080);