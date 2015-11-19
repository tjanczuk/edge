var edge = require('../lib/edge.js'), assert = require('assert')
    , path = require('path');

var edgeTestDll = process.env.EDGE_USE_CORECLR ? 'test' : path.join(__dirname, 'Edge.Tests.dll');

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
                if ((error instanceof Error) && error.message.match(/Invalid expression term '=>'|Unexpected symbol `=>'|The name 'async_foo' does not exist in the current context/)) {
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
                if ((error instanceof Error) && error.message.match(/Expected class, delegate, enum, interface, or|expecting `class', `delegate', `enum', `interface', `partial', or `struct'|The type or namespace name 'classes' could not be found/)) {
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
            source: process.env.EDGE_USE_CORECLR ?
                function () {/* 
                    //#r "System.Data.Common"

                    using System.Threading.Tasks;
                    using System.Data;

                    public class Startup 
                    {
                        public async Task<object> Invoke(object input) 
                        {
                            return "Hello, " + input.ToString();
                        }
                    }           
                */} :
                function () {/* 
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
            source: process.env.EDGE_USE_CORECLR ?
                function () {/* 
                    #r "System.Data.Common"

                    using System.Threading.Tasks;
                    using System.Data;

                    public class Startup 
                    {
                        public async Task<object> Invoke(object input) 
                        {
                            return "Hello, " + input.ToString();
                        }
                    }           
                */} :
                function () {/* 
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
            source: process.env.EDGE_USE_CORECLR ?
                function () {/* 
                    //#r "System.Data.Common"
                    
                    async (input) => 
                    {
                        return "Hello, " + input.ToString();
                    }
                */} :
                function () {/* 
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
            source: process.env.EDGE_USE_CORECLR ?
                function () {/* 
                    #r "System.Data.Common"
                    
                    async (input) => 
                    {
                        return "Hello, " + input.ToString();
                    }
                */} :
                function () {/* 
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
            source: process.env.EDGE_USE_CORECLR ?
                function () {/* 
                    #r "System.Data.Common"

                    using System.Data;
                    
                    async (input) => 
                    {
                        return input.ToString() + " is " + DbType.Single.ToString();
                    }
                */} :
                function () {/* 
                    #r "System.Data.dll"

                    using System.Data;
                    
                    async (input) => 
                    {
                        return input.ToString() + " is " + DbType.Single.ToString();
                    }
                */}
        });
        func("JavaScript", function (error, result) {
            assert.ifError(error);
            assert.equal(result, 'JavaScript is Single');
            done();
        });
    });

    it('succeeds with dynamic input to async lambda', function (done) {
        var func = edge.func({
            source: function () {/* 
                async (dynamic input) => 
                {
                    string inputText = input.text;
                    return inputText + " works";
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
                    string inputText = input.nested.text;
                    return inputText + " works";
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
                        string inputText = input.text;
                        return inputText + " works";
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
                        string inputText = input.nested.text;
                        return inputText + " works";
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

    it('fails with a reference to a non-existent assembly without comment in class', function () {
        assert.throws(function() {
            edge.func({
                source: process.env.EDGE_USE_CORECLR ?
                    function () {/* 
                        #r "Package.Doesnt.Exist"

                        using System.Threading.Tasks;
                        using System.Data;

                        public class Startup 
                        {
                            public async Task<object> Invoke(object input) 
                            {
                                return "Hello, " + input.ToString();
                            }
                        }           
                    */} :
                    function () {/* 
                        #r "Package.Doesnt.Exist.dll"

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
        },
        function (error) {
            if ((error instanceof Error) && error.message.match(/Unable to resolve reference to Package\.Doesnt\.Exist|Package\.Doesnt\.Exist\.dll' could not be found/)) {
                return true;
            }
            return false;
        },
        'Unexpected result');
    });

    if (process.env.EDGE_USE_CORECLR) {
        it('fails when dynamically loading an assembly that doesn\'t exist', function () {
            assert.throws(function() {
                var func = edge.func({
                    source: function () {/* 
                        #r "System.Reflection"
                        #r "System.Runtime.Loader"

                        using System.Runtime.Loader;
                        using System.Threading.Tasks;
                        using System.Reflection;

                        public class Startup 
                        {
                            public async Task<object> Invoke(object input) 
                            {
                                var assembly = AssemblyLoadContext.Default.LoadFromAssemblyName(new AssemblyName("Package.Doesnt.Exist"));
                                return "Hello, " + input.ToString();
                            }
                        }           
                    */}
                });

                func("JavaScript");
            },
            function (error) {
                if ((error instanceof Error) && error.message.match(/Could not load file or assembly 'Package\.Doesnt\.Exist/)) {
                    return true;
                }
                return false;
            },
            'Unexpected result');
        });
    }
});
