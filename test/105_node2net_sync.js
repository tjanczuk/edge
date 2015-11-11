var edge = require('../lib/edge.js'), assert = require('assert')
    , path = require('path');

var edgeTestDll = process.env.EDGE_USE_CORECLR ? 'test' : path.join(__dirname, 'Edge.Tests.dll');

describe('sync call from node.js to .net', function () {

    it('succeeds for hello world', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'Invoke'
        });
        var result = func('Node.js', true);
        assert.equal(result, '.NET welcomes Node.js');
    });

    it('succeeds for hello world when called sync and async', function (done) {
        // create the func
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'Invoke'
        });

        // call the func synchronously
        var result = func('Node.js', true);
        assert.equal(result, '.NET welcomes Node.js');

        // call the same func asynchronously
        func('Node.js', function (error, result) {
            assert.ifError(error);
            assert.equal(result, '.NET welcomes Node.js');
            done();
        });
    });

    it('successfuly marshals data from node.js to .net', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'MarshalIn'
        });
        var payload = {
            a: 1,
            b: 3.1415,
            c: 'foo',
            d: true,
            e: false,
            f: new Buffer(10),
            g: [ 1, 'foo' ],
            h: { a: 'foo', b: 12 },
            i: function (payload, callback) { },
            j: new Date(Date.UTC(2013,07,30))
        };
        var result = func(payload, true);
        assert.equal(result, 'yes');
    });

    it('successfuly marshals .net exception thrown on v8 thread from .net to node.js', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'NetExceptionTaskStart'
        });
        assert.throws(
            function() { func(null, true); },
            function (error) {
                if ((error instanceof Error) && error.Message.match(/Test .NET exception/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('fails if C# method does not complete synchronously', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'DelayedReturn'
        });
        assert.throws(
            function() { func(null, true); },
            / The JavaScript function was called synchronously but/
        );
    });
});