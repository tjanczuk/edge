var edge = require('../lib/edge.js')
	, assert = require('assert');

var edgeTestDll = __dirname + '\\Edge.Tests.dll';

describe('async call from .net to node.js', function () {

	it('succeeds for hello world', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'InvokeBack'
		});
		var payload = {
			hello: function (payload, callback) {
				callback(null, 'Node.js welcomes ' + payload);
			}			
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'Node.js welcomes .NET');
			done();
		});
	});

	it('successfuly marshals data from .net to node.js', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'MarshalInFromNet'
		});
		var payload = {
			hello: function (result, callback) {
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
				callback(null, 'yes');
			}			
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'yes');
			done();
		});
	});	

	it('successfuly marshals data from node.js to .net', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'MarshalBackToNet'
		});
		var payload = {
			hello: function (result, callback) {
				var payload = {
					a: 1,
					b: 3.1415,
					c: 'foo',
					d: true,
					e: false,
					f: new Buffer(10),
					g: [ 1, 'foo' ],
					h: { a: 'foo', b: 12 }
				};
				callback(null, payload);
			}			
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'yes');
			done();
		});
	});

	it('successfuly marshals v8 exception on invoking thread', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'MarshalException'
		});
		var payload = {
			hello: function (result, callback) {
				throw new Error('Sample Node.js exception');
			}			
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(typeof result, 'string');
			assert.ok(result.indexOf('Sample Node.js exception') > 0);
			done();
		});
	});			

	it('successfuly marshals v8 exception in callback', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'MarshalException'
		});
		var payload = {
			hello: function (result, callback) {
				process.nextTick(function () {
					callback(new Error('Sample Node.js exception'));
				});
			}			
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(typeof result, 'string');
			assert.ok(result.indexOf('Sample Node.js exception') > 0);
			done();
		});
	});				
});