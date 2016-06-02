// Fix #176 for GUI applications on Windows
try {
    var stdout = process.stdout;
}
catch (e) {
    // This is a Windows GUI application without stdout and stderr defined.
    // Define process.stdout and process.stderr so that all output is discarded. 
    (function () {
        var stream = require('stream');
        var NullStream = function (o) {
            stream.Writable.call(this);
            this._write = function (c, e, cb) { cb && cb(); };
        }
        require('util').inherits(NullStream, stream.Writable);
        var nullStream = new NullStream();
        process.__defineGetter__('stdout', function () { return nullStream; });
        process.__defineGetter__('stderr', function () { return nullStream; });
    })();
}

process.env['EDGE_NATIVE'] = process.env['EDGE_NATIVE'] ||
    __dirname + (process.arch === 'x64' ? '\\x64\\edge_nativeclr.node' : '\\x86\\edge_nativeclr.node');

var edge = require('./edge.js');

var initialize = edge.func({
    assemblyFile: __dirname + '\\..\\EdgeJs.dll',
    typeName: 'EdgeJs.Edge',
    methodName: 'InitializeInternal'
});

var compileFunc = function (data, callback) {
    var wrapper = '(function () { ' + data + ' })';
    var funcFactory = eval(wrapper);
    var func = funcFactory();
    if (typeof func !== 'function') {
        throw new Error('Node.js code must return an instance of a JavaScript function. '
            + 'Please use `return` statement to return a function.');
    }

    callback(null, func);
};

initialize(compileFunc, function (error, data) {
    if (error) throw error;
});

// prevent the V8 thread from exiting for the lifetime of the CLR application
setInterval(function () {}, 3600000); 
