var edge = require('../lib/edge.js'), assert = require('assert');

var edgeTestDll = __dirname + '\\Edge.Tests.dll';

describe('async call from node.js to .net', function () {

	it('succeeds for hello world', function (done) {
		var func = edge.func(edgeTestDll);
		func('Node.js', function (error, result) {
			assert.ifError(error);
			assert.equal(result, '.NET welcomes Node.js');
			done();
		});
	});

	it('successfuly marshals data from node.js to .net', function (done) {
		var func = edge.func({
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
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'yes');
			done();
		});
	});

	it('successfuly marshals data from .net to node.js', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'MarshalBack'
		});
		func(null, function (error, result) {
			assert.ifError(error);
			assert.equal(typeof result, 'object');
			assert.ok(result.a === 1);
			assert.ok(result.b === 3.1415);
			assert.ok(result.c === 'foo');
			assert.ok(result.d === true);
			assert.ok(result.e === false);
			assert.equal(typeof result.f, 'object');
			assert.ok(Buffer.isBuffer(result.f));
			assert.equal(result.f.length, 10);
			assert.ok(Array.isArray(result.g));
			assert.equal(result.g.length, 2);
			assert.ok(result.g[0] === 1);
			assert.ok(result.g[1] === 'foo');
			assert.equal(typeof result.h, 'object');
			assert.ok(result.h.a === 'foo');
			assert.ok(result.h.b === 12);
			assert.equal(typeof result.i, 'function');
			done();
		});
	});

	it('successfuly marshals .net exception thrown on v8 thread from .net to node.js', function () {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'NetExceptionTaskStart'
		});
		assert.throws(
			func,
			/Test .NET exception/
		);
	});

	it('successfuly marshals .net exception thrown on CLR thread from .net to node.js', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'NetExceptionCLRThread'
		});
		func(null, function (error, result) {
			assert.equal(result, undefined);
			assert.equal(typeof error, 'string');
			assert.ok(error.indexOf('Test .NET exception') > 0);
			done();
		});
	});
});