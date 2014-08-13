var edge = require('../lib/edge.js'), assert = require('assert')
    , path = require('path');

var edgeTestDll = path.join(__dirname, 'Edge.Tests.dll');

describe('edge-cs', function () {

    it('succeeds with literal lambda', function (done) {
        var func = edge.func('async (input) => { return "Hello, " + input.ToString(); }');
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with csx file with lambda', function (done) {
        var func = edge.func(path.join(__dirname, 'hello_lambda.csx'));
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with lambda in function', function (done) {
        var func = edge.func(function () {/* async (input) => { return "Hello, " + input.ToString(); } */});
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with literal class', function (done) {
        var func = edge.func('using System.Threading.Tasks; public class Startup { ' +
            ' public async Task<object> Invoke(object input) { return "Hello, " + input.ToString(); } }');
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with csx file with class', function (done) {
        var func = edge.func(path.join(__dirname, 'hello_class.csx'));
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with cs file with class', function (done) {
        var func = edge.func(path.join(__dirname, 'hello_class.cs'));
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with class in function', function (done) {
        var func = edge.func(function () {/* 
            using System.Threading.Tasks;

            public class Startup 
            {
                public async Task<object> Invoke(object input) 
                {
                    return "Hello, " + input.ToString();
                }
            }
        */});
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with custom class and method name', function (done) {
        var func = edge.func({
            source: function () {/* 
                using System.Threading.Tasks;

                namespace Foo 
                {
                    public class Bar 
                    {
                        public async Task<object> InvokeMe(object input) 
                        {
                            return "Hello, " + input.ToString();
                        }
                    }
                }           
            */},
            typeName: 'Foo.Bar',
            methodName: 'InvokeMe'
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('fails with malformed literal lambda', function () {
        assert.throws(
            function () { edge.func('async_foo (input) => { return "Hello, " + input.ToString(); }'); },
            function (error) {
                if ((error instanceof Error) && error.message.match(/Invalid expression term '=>'|Unexpected symbol `=>'/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('fails with malformed class in function', function () {
        assert.throws(
            function () {
                edge.func(function () {/* 
                    using System.Threading.Tasks;

                    public classes Startup 
                    {
                        public async Task<object> Invoke(object input) 
                        {
                            return "Hello, " + input.ToString();
                        }
                    }
                */});
            },
            function (error) {
                if ((error instanceof Error) && error.message.match(/Expected class, delegate, enum, interface, or|expecting `class', `delegate', `enum', `interface', `partial', or `struct'/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('fails when Invoke method is missing', function () {
        assert.throws(
            function () {
                edge.func(function () {/* 
                    using System.Threading.Tasks;

                    public class Startup 
                    {
                        public async Task<object> Invoke_Foo(object input) 
                        {
                            return "Hello, " + input.ToString();
                        }
                    }
                */});
             },
            function (error) {
                if ((error instanceof Error) && error.message.match(/Unable to access CLR method to wrap through reflection/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('fails when Startup class is missing', function () {
        assert.throws(
            function () {
                edge.func(function () {/* 
                    using System.Threading.Tasks;

                    public class Startup_Bar
                    {
                        public async Task<object> Invoke(object input) 
                        {
                            return "Hello, " + input.ToString();
                        }
                    }           
                */});
            },
            function(error) {
                if ( (error instanceof Error) && error.message.match(/Could not load type 'Startup'/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result'
        );
    });

    it('succeeds with System.Data.dll reference as comment in class', function (done) {
        var func = edge.func({
            source: function () {/* 
                //#r "System.Data.dll"

                using System.Threading.Tasks;
                using System.Data;

                public class Startup 
                {
                    public async Task<object> Invoke(object input) 
                    {
                        return "Hello, " + input.ToString();
                    }
                }           
            */}
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with System.Data.dll reference without comment in class', function (done) {
        var func = edge.func({
            source: function () {/* 
                #r "System.Data.dll"

                using System.Threading.Tasks;
                using System.Data;

                public class Startup 
                {
                    public async Task<object> Invoke(object input) 
                    {
                        return "Hello, " + input.ToString();
                    }
                }           
            */}
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });    

    it('succeeds with System.Data.dll reference as comment in async lambda', function (done) {
        var func = edge.func({
            source: function () {/* 
                //#r "System.Data.dll"
                
                async (input) => 
                {
                    return "Hello, " + input.ToString();
                }
            */}
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with System.Data.dll reference without comment in async lambda', function (done) {
        var func = edge.func({
            source: function () {/* 
                #r "System.Data.dll"
                
                async (input) => 
                {
                    return "Hello, " + input.ToString();
                }
            */}
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Hello, JavaScript');
            done();
        });
    });

    it('succeeds with System.Data.dll reference and a using statement in async lambda', function (done) {
        var func = edge.func({
            source: function () {/* 
                #r "System.Data.dll"

                using System.Data;
                
                async (input) => 
                {
                    return input.ToString() + " is " + SqlDbType.Real.ToString();
                }
            */}
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'JavaScript is Real');
            done();
        });
    });

    it('succeeds with dynamic input to async lambda', function (done) {
        var func = edge.func({
            source: function () {/* 
                async (dynamic input) => 
                {
                    return input.text + " works";
                }
            */}
        });
        func({ text: 'Dynamic' }, function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Dynamic works');
            done();
        });
    });

    it('succeeds with nested dynamic input to async lambda', function (done) {
        var func = edge.func({
            source: function () {/* 
                async (dynamic input) => 
                {
                    return input.nested.text + " works";
                }
            */}
        });
        func({ nested: { text: 'Dynamic' } }, function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Dynamic works');
            done();
        });
    });

    it('succeeds with dynamic input to Invoke method', function (done) {
        var func = edge.func({
            source: function () {/* 
                using System.Threading.Tasks;

                public class Startup 
                {
                    public async Task<object> Invoke(dynamic input) 
                    {
                        return input.text + " works";
                    }
                }    
            */}
        });
        func({ text: 'Dynamic' }, function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Dynamic works');
            done();
        });
    });    

    it('succeeds with nested dynamic input to Invoke method', function (done) {
        var func = edge.func({
            source: function () {/* 
                using System.Threading.Tasks;

                public class Startup 
                {
                    public async Task<object> Invoke(dynamic input) 
                    {
                        return input.nested.text + " works";
                    }
                }    
            */}
        });
        func({ nested: { text: 'Dynamic' } }, function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Dynamic works');
            done();
        });
    }); 

    it('succeeds with dictionary input to Invoke method', function (done) {
        var func = edge.func({
            source: function () {/* 
                using System.Threading.Tasks;
                using System.Collections.Generic;

                public class Startup 
                {
                    public async Task<object> Invoke(IDictionary<string,object> input) 
                    {
                        return ((IDictionary<string,object>)input["nested"])["text"] + " works";
                    }
                }    
            */}
        });
        func({ nested: { text: 'Dictionary' } }, function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'Dictionary works');
            done();
        });
    });    
});
