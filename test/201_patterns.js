var edge = require('../lib/edge.js'), assert = require('assert'), path = require('path');

var edgeTestDll = process.env.EDGE_USE_CORECLR ? 'test' : path.join(__dirname, 'Edge.Tests.dll');

describe('call patterns', function () {

    it('sync call to exported .NET lambda', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ReturnAsyncLambda'
        });

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        var result = lambda('Node.js', true);
        assert.equal(result, '.NET welcomes Node.js');
    });

    it('async call to exported .NET lambda', function (done) {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ReturnAsyncLambda'
        });

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        lambda('Node.js', function (error, result) {
            assert.ifError(error);
            assert.equal(result, '.NET welcomes Node.js');
            done();
        });
    });

    it('call exported .NET lambda with closure over CLR state', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ReturnLambdaWithClosureOverState'
        });

        var lambda = func(12, true);
        assert.equal(typeof lambda, 'function');
        assert.equal(lambda(null, true), 13);
        assert.equal(lambda(null, true), 14);
    });

    it('successfuly marshals .net exception thrown on V8 thread from exported CLR lambda', function () {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ReturnLambdaThatThrowsException'
        });

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        assert.throws(
            function () { lambda(null, true); },
            function (error) {
                if ((error instanceof Error) && error.Message.match(/Test .NET exception/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('successfuly marshals .net exception thrown on CLR thread from exported CLR lambda', function (done) {
        var func = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ReturnLambdaThatThrowsAsyncException'
        });

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        lambda(null, function (error, result) {
            assert.ok(error instanceof Error);
            assert.ok(error.Message.match(/Test .NET exception/));
            done();
        });
    });

    // Regression test for https://github.com/tjanczuk/edge/issues/39
    it('large number of concurrent callbacks from C# to JavaScript (issue #39)', function (done) {
        var edgetest = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'LargeNumberOfConcurrentCallbacks'
        });

        var results = [];

        edgetest(
            function (data, callback) {
                results.push(data);
                callback();
            },
            function (error, result) {
                assert.ifError(error);
                assert.equal(results.length, 5000);
                assert.equal(result, 5000);
                done();
            }
        );
    }); 

    // Regression test for https://github.com/tjanczuk/edge/issues/22
    it('two async callouts each with async callin (issue #22)', function (done) {
        var log = [];

        var callin = function (input, callback) {
            log.push(input);
            callback();
        };

        var callout = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'CallbackToNodejs'
        });

        var tryFinish = function() {
            if (log.length === 4) {
                assert.deepEqual(log.sort(), [ 'callin1', 'callin1-return', 'callin2', 'callin2-return' ]);
                done();
            }
        };

        var callback = function (error, result) {
            assert.ifError(error);
            log.push(result + '-return');
            tryFinish();
        };

        callout({ payload: 'callin1', callin: callin }, callback);
        callout({ payload: 'callin2', callin: callin }, callback);
    });

    it('call JS func exported to .NET as a result of calling a JS func from .NET', function (done) {
        var callout = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'DoubleLayerCallbackToNodejs'
        });

        callout(
            function (data, callback) {
                callback(null, function (data, callback) {
                    callback(null, 'Func2 called');
                });
            }, 
            function (error, result) {
                assert.ifError(error);
                assert.equal(result, 'Func2 called');
                done();
            }
        );
    });

    it('exception when marshaling CLR data to V8 when calling exported JS function', function (done) {
        var callout = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ThrowExceptionMarshallingForCallback'
        });

        callout(
            function (data, callback) {
                callback();
            },
            function (error, result) {
                assert.ifError(error);
                assert.equal(result, 'Expected failure');
                done();
            } 
        );
    }); 

    it('exception when marshaling CLR data to V8 when completing a synchronous call from JS to .NET', function () {
        var callout = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ThrowExceptionMarshallingForReturn'
        });

        assert.throws(
            function () { callout(null, true); }, 
            function (error) {
                if ((error instanceof Error) && error.message.match(/I have no name/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('exception when marshaling CLR data to V8 when completing an asynchronous call from JS to .NET', function (done) {
        var callout = edge.func({
            assemblyFile: edgeTestDll,
            typeName: 'Edge.Tests.Startup',
            methodName: 'ThrowExceptionMarshallingForAsyncReturn'
        });

        callout(false, function (error, result) {
            assert.equal(true, error instanceof Error);
            assert.ok(error.message.match(/I have no name/));
            done();
        });

    });


    if (process.env.EDGE_USE_CORECLR) {
        it('merged dependencies choose correct version', function (done) {
            var func = edge.func({
                assemblyFile: edgeTestDll,
                typeName: 'Edge.Tests.Startup',
                methodName: 'CorrectVersionOfNewtonsoftJsonUsed'
            });

            func(null, function (error, result) {
                assert.equal(result, "9.0.0.0");
                done();
            });
        });

        it('can use DependencyContext.Default', function (done) {
            var func = edge.func({
                assemblyFile: edgeTestDll,
                typeName: 'Edge.Tests.Startup',
                methodName: 'CanUseDefaultDependencyContext'
            });

            func(null, function (error, result) {
                assert.equal(result, "9.0.1");
                done();
            });
        });

        it('can use native libraries', function (done) {
            var func = edge.func({
                assemblyFile: edgeTestDll,
                typeName: 'Edge.Tests.Startup',
                methodName: 'CanUseNativeLibraries'
            });

            func(null, function (error, result) {
                assert.ok(result > 0);
                done();
            });
        });

        it('can deserialize using XmlSerializer', function (done) {
            var func = edge.func({
                assemblyFile: edgeTestDll,
                typeName: 'Edge.Tests.Startup',
                methodName: 'DeserializeObject'
            });

            func(null, function (error, result) {
                assert.equal(result.AttributeValue, "My attribute value");
                assert.equal(result.ElementValue, "This is an element value");
                done();
            });
        });
    }
});