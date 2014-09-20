var edge = require('../lib/edge.js'), assert = require('assert');

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
            assert.ok(error instanceof Error);
            assert.ok(error.Message.match(/Test .NET exception/));
            done();
        });
    });

    // Regression test for https://github.com/tjanczuk/edge/issues/39
    it('large number of concurrent callbacks from C# to JavaScript (issue #39)', function (done) {
        var edgetest = edge.func(function () {/*
            using System;
            using System.Collections.Generic;
            using System.Threading.Tasks;

            public class Startup
            {
                public async Task<object> Invoke(object input)
                {
                    Func<object, Task<object>> rowCallback = (Func<object, Task<object>>)input;
                    IList<Task<object>> rowEvents = new List<Task<object>>();

                    for (int i = 0; i < 5000; ++i)
                    {
                        IDictionary<string, object> row = new Dictionary<string, object>();
                        for (int j = 0; j < 25; j++) row.Add(j.ToString(), j);
                        rowEvents.Add(rowCallback(row));
                    }

                    await Task.WhenAll(rowEvents);

                    return rowEvents.Count;
                }
            }            
        */});

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
        var callout = edge.func(function () {/*
            async (input) => {
                Func<object,Task<object>> func1 = (Func<object,Task<object>>)input;
                Func<object,Task<object>> func2 = (Func<object,Task<object>>)(await func1(null));
                return await func2(null);
            }
        */});

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
        var callout = edge.func(function () {/*
            using System;
            using System.Collections.Generic;
            using System.Threading.Tasks;

            public class BadPerson 
            {
                public string Name 
                {
                    get 
                    {
                        throw new InvalidOperationException("I have no name");
                    }
                }
            }

            public class Startup {
                public async Task<object> Invoke(Func<object, Task<object>> callin) 
                {
                    try 
                    {
                        await callin(new BadPerson());
                        return "Unexpected success";
                    }
                    catch (Exception e)
                    {
                        while (e.InnerException != null)
                            e = e.InnerException;

                        if (e.Message == "I have no name")
                        {
                            return "Expected failure";
                        }   
                        else 
                        {
                            return "Unexpected failure: " + e.ToString();
                        }
                    }
                }
            }
        */});

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
        var callout = edge.func(function () {/*
            using System;
            using System.Collections.Generic;
            using System.Threading.Tasks;

            public class BadPerson 
            {
                public string Name 
                {
                    get 
                    {
                        throw new InvalidOperationException("I have no name");
                    }
                }
            }

            public class Startup {
                public async Task<object> Invoke(object input) 
                {
                    return new BadPerson();
                }
            }
        */});

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
        var callout = edge.func(function () {/*
            using System;
            using System.Collections.Generic;
            using System.Threading.Tasks;

            public class BadPerson 
            {
                public string Name 
                {
                    get 
                    {
                        throw new InvalidOperationException("I have no name");
                    }
                }
            }

            public class Startup {
                public async Task<object> Invoke(object input) 
                {
                    await Task.Delay(200);
                    return new BadPerson();
                }
            }
        */});

        callout(false, function (error, result) {
            assert.equal(true, error instanceof Error);
            assert.ok(error.message.match(/I have no name/));
            done();
        });

    }); 

});