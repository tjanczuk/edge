// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge')
    , http = require('http');

var authenticate = edge.func(function() {/*
    using System;
    using System.Threading.Tasks;
    using System.Runtime.InteropServices;
    using System.Security.Principal;

    class Startup
    {

        [DllImport("advapi32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
        public static extern bool LogonUser(string lpszUsername, string lpszDomain, string lpszPassword,
            int dwLogonType, int dwLogonProvider, out IntPtr phToken);

        [DllImport("kernel32.dll", CharSet = CharSet.Auto)]
        public extern static bool CloseHandle(IntPtr handle);

        public async Task<object> Invoke(dynamic input)
        {
            return await Task<object>.Run(() => { 
                IntPtr token;
                if (!Startup.LogonUser(input.user, null, input.password, 3, 0, out token))
                {
                    throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
                }

                try {
                    using (WindowsIdentity id = new WindowsIdentity(token))
                    {
                        return new {
                            name = id.Name,
                            sid = id.User.ToString()
                        };
                    }
                }
                finally {
                    Startup.CloseHandle(token);
                }
            });
        }
    }
*/});

http.createServer(function (req, res) {
    function challange() {
        res.writeHead(401, { 'WWW-Authenticate': 'Basic realm=""' });
        res.end();
    }

    if (req.headers.authorization && req.headers.authorization.indexOf('Basic ') === 0) {
        var tokens = new Buffer(req.headers.authorization.substring(6), 'base64').toString('utf8').split(':');
        if (!tokens || tokens.length !== 2  || tokens[0] === 'noone' && tokens[1] === 'noone') {
            challange();
        }
        else {
            authenticate({ user: tokens[0], password: tokens[1] }, function (error, result) {
                if (error) {
                    console.log(error);
                    challange();
                }
                else {
                    res.writeHead(200, { 'Content-Type': 'text/html' });
                    res.write('<p>Welcome ' + result.name + ' (' + result.sid + ')');
                    res.end('<p><a href="http://noone:noone@' + req.headers.host + '/">Logout</a>');
                }
            })
        }
    }
    else {
        challange();
    }
}).listen(process.env.PORT || 8080);
