process.env['EDGE_NATIVE'] = process.env['EDGE_NATIVE'] ||
    __dirname + (process.arch === 'x64' ? '\\x64\\edge.node' : '\\x86\\edge.node');

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
