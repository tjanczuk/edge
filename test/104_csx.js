var edge = require('../lib/edge.js'), assert = require('assert');

var edgeTestDll = __dirname + '\\Edge.Tests.dll';

describe('csx', function () {

	it('succeeds with literal lambda', function (done) {
		var func = edge.func('async (input) => { return "Hello, " + input.ToString(); }');
		func("JavaScript", function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'Hello, JavaScript');
			done();
		});
	});

	it('succeeds with csx file with lambda', function (done) {
		var func = edge.func(__dirname + '\\hello_lambda.csx');
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
		var func = edge.func(__dirname + '\\hello_class.csx');
		func("JavaScript", function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'Hello, JavaScript');
			done();
		});
	});

	it('succeeds with cs file with class', function (done) {
		var func = edge.func(__dirname + '\\hello_class.cs');
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
			csx: function () {/* 
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
			/Invalid expression term '=>'/
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
			/Expected class, delegate, enum, interface, or/
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
			/Unable to access the CLR method to wrap through reflection/
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
			/Could not load type 'Startup' from assembly/
		);
	});

	it('succeeds with System.Data.dll reference', function (done) {
		var func = edge.func({
			csx: function () {/* 
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
});
