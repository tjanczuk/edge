var edge = require('../lib/edge.js')
	, assert = require('assert');

var edgeTestDll = __dirname + '\\Edge.Tests.dll';

describe('sync call from node.js to .net', function () {

	it('succeeds for hello world', function () {
		var func = edge.funcSync('async (input) => { return ".NET welcomes " + input.ToString(); }');
		var result = func('Node.js');
		assert.equal(result, '.NET welcomes Node.js');
	});

	it('successfuly marshals data from node.js to .net', function () {
		var func = edge.funcSync({
			assemblyFile: edgeTestDll,
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
			i: function (payload, callback) { }
		}
		var result = func(payload);
		assert.equal(result, 'yes');
	});

	it('successfuly marshals .net exception thrown on v8 thread from .net to node.js', function () {
		var func = edge.funcSync(function() {/*
			async (input) => 
			{
				throw new Exception("Test .NET exception");
			}
		*/});
		assert.throws(
			func,
			/Test .NET exception/
		);	
	});

	it('fails if C# method does not complete synchronously', function () {
		var func = edge.funcSync(function() {/*
			async (input) => 
			{
				await Task.Delay(1000);
				return null;
			}
		*/});
		assert.throws(
			func,
			/The CLR function was declared as synchronous but it returned without completing the Task/
		);	
	});
});