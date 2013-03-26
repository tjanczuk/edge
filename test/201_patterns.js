var edge = require('../lib/edge.js')
    , assert = require('assert');

describe('call patterns', function () {

    it('sync call to exported .NET lambda', function () {
        var func = edge.func(function () {/*
            async (input) =>
            {
                return (Func<object,Task<object>>)(async (i) => { return ".NET welcomes " + i.ToString(); });
            }
        */});

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        var result = lambda('Node.js', true);
        assert.equal(result, '.NET welcomes Node.js');
    });

    it('async call to exported .NET lambda', function (done) {
        var func = edge.func(function () {/*
            async (input) =>
            {
                return (Func<object,Task<object>>)(async (i) => { return ".NET welcomes " + i.ToString(); });
            }
        */});

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        lambda('Node.js', function (error, result) {
            assert.ifError(error);
            assert.equal(result, '.NET welcomes Node.js');
            done();
        });
        
    });

    it('call exported .NET lambda with closure over CLR state', function () {
        var func = edge.func(function () {/*
            async (input) =>
            {
                var k = (int)input;
                return (Func<object,Task<object>>)(async (i) => { return ++k; });
            }
        */});

        var lambda = func(12, true);
        assert.equal(typeof lambda, 'function');
        assert.equal(lambda(null, true), 13);
        assert.equal(lambda(null, true), 14);
    });

    it('successfuly marshals .net exception thrown on V8 thread from exported CLR lambda', function () {
        var func = edge.func(function() {/*
            async (input) => 
            {
                return (Func<object,Task<object>>)(async (i) => { throw new Exception("Test .NET exception"); });
            }
        */});

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        assert.throws(
            function() { lambda(null, true); },
            /Test .NET exception/
        );  
    }); 

    it('successfuly marshals .net exception thrown on CLR thread from exported CLR lambda', function (done) {
        var func = edge.func(function() {/*
            async (input) => 
            {
                return (Func<object,Task<object>>)((i) => { 
                    Task<object> task = new Task<object>(() =>
                    {
                        throw new Exception("Test .NET exception");
                    });

                    task.Start();

                    return task; 
                });
            }
        */});

        var lambda = func(null, true);
        assert.equal(typeof lambda, 'function');
        lambda(null, function (error, result) {
            assert.throws(
                function () { throw error; },
                /Test .NET exception/
            );
            done();
        });
    });     

    // Regression test for https://github.com/tjanczuk/edge/issues/22
    it('two async callouts each with async callin (issue #22)', function (done) {
        var log = [];
        
        var callin = function (input, callback) {
            log.push(input);
            callback();
        };

        var callout = edge.func(function () {/*
            using System;
            using System.Collections.Generic;
            using System.Threading.Tasks;

            public class Startup {
                public async Task<object> Invoke(IDictionary<string, object> input) {
                    Func<object, Task<object>> callin = (Func<object, Task<object>>)input["callin"];
                    await callin(input["payload"]);
                    return input["payload"];
                }
            }
        */});

        var tryFinish = function() {
            if (log.length == 4) {
                assert.deepEqual(log, [ 'callin1', 'callin2', 'return', 'return' ]);
                done();
            }
        };

        var callback = function (error, result) {
            assert.ifError(error);
            log.push('return');
            tryFinish();
        };

        callout({ payload: 'callin1', callin: callin }, callback);
        callout({ payload: 'callin2', callin: callin }, callback);
    });
});
 