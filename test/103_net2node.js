/**
 * Portions Copyright (c) Microsoft Corporation. All rights reserved. 
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *  http://www.apache.org/licenses/LICENSE-2.0  
 *
 * THIS CODE IS PROVIDED *AS IS* BASIS, WITHOUT WARRANTIES OR CONDITIONS
 * OF ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION 
 * ANY IMPLIED WARRANTIES OR CONDITIONS OF TITLE, FITNESS FOR A PARTICULAR 
 * PURPOSE, MERCHANTABLITY OR NON-INFRINGEMENT. 
 *
 * See the Apache Version 2.0 License for specific language governing 
 * permissions and limitations under the License.
 */
var edge = require('../lib/edge.js'), assert = require('assert')
	, path = require('path');

var edgeTestDll = path.join(__dirname, 'Edge.Tests.dll');

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
				assert.equal(typeof result.j, 'object');
				assert.ok(result.j.valueOf() === Date.UTC(2013, 07, 30));

				callback(null, 'yes');
			}
		};
		func(payload, function (error, result) {
			assert.ifError(error);
			assert.equal(result, 'yes');
			done();
		});
	});

	it('successfuly marshals object hierarchy from .net to node.js', function (done) {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'MarshalObjectHierarchy'
		});
		func(null, function (error, result) {
			assert.ifError(error);
			assert.equal(result.A_field, 'a_field');
			assert.equal(result.A_prop, 'a_prop');
			assert.equal(result.B_field, 'b_field');
			assert.equal(result.B_prop, 'b_prop');
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
					h: { a: 'foo', b: 12 },
					j: new Date(Date.UTC(2013, 07, 30))
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
				var next = global.setImmediate || process.nextTick;
				next(function () {
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

	it('successfuly marshals empty buffer', function (done) {
		var func = edge.func(function () {/*
			async (object input) => {
				return new byte[] {};
			}
		*/});

		func(null, function (error, result) {
			assert.ifError(error);
			assert.ok(Buffer.isBuffer(result));
			assert.ok(result.length === 0)
			done();
		})
	});
});

describe('delayed call from node.js to .net', function () {

	it('succeeds for one callback after Task', function (done) {
		var expected = [
			'InvokeBackAfterCLRCallHasFinished#EnteredCLR',
			'InvokeBackAfterCLRCallHasFinished#LeftCLR',
			'InvokeBackAfterCLRCallHasFinished#ReturnedToNode',
			'InvokeBackAfterCLRCallHasFinished#CallingCallbackFromDelayedTask',
		]

		var func = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'InvokeBackAfterCLRCallHasFinished'
		});

		var ensureNodejsFuncIsCollected = edge.func({
			assemblyFile: edgeTestDll,
			methodName: 'EnsureNodejsFuncIsCollected'
		});

		var trace = [];
		var payload = {
			eventCallback: function(result, callback) {
				trace.push(result);
				callback();
				assert.ok(expected.length === trace.length);
				for (var i = 0; i < expected.length; i++) {
					assert.ok(expected[i] === trace[i]);
				}
				ensureNodejsFuncIsCollected(null, function(error, result) {
					assert.ifError(error);
					assert.ok(result);
					done();
				});
			}
		}

		func(payload, function (error, result) {
			assert.ifError(error);
			result.forEach(function(entry) {
				trace.push(entry);
			});
			trace.push('InvokeBackAfterCLRCallHasFinished#ReturnedToNode');
		});
	});
});