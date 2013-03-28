var edge = require('../lib/edge.js'), assert = require('assert');

var edgeTestDll = __dirname + '\\Edge.Tests.dll';

describe('edge.func', function () {

	it('exists', function () {
		assert.equal(typeof edge.func, 'function');
	});

	it('fails without parameters', function () {
		assert.throws(
			edge.func,
			/Specify the file name of the CLR DLL or provide an options object/
		);
	});

	it('fails with a wrong parameter', function () {
		assert.throws(
			function () { edge.func(12); },
			/Specify the file name of the CLR DLL or provide an options object/
		);
	});

	it('fails with missing assemblyFile or csx', function () {
		assert.throws(
			function () { edge.func({}); },
			/Provide DLL or CSX file name or C# literal as a string parmeter/
		);
	});

	it('fails with both assemblyFile or csx', function () {
		assert.throws(
			function () { edge.func({ assemblyFile: 'foo.dll', csx: 'async (input) => { return null; }'}); },
			/Provide either an asseblyFile or csx property, but not both/
		);
	});

	it('fails with nonexisting assemblyFile', function () {
		assert.throws(
			function () { edge.func('idontexist.dll'); },
			/System.IO.FileNotFoundException/
		);
	});

	it('succeeds with assemblyFile as string', function () {
		var func = edge.func(edgeTestDll);
		assert.equal(typeof func, 'function');
	});

	it('succeeds with assemblyFile as options property', function () {
		var func = edge.func({ assemblyFile: edgeTestDll });
		assert.equal(typeof func, 'function');
	});

	it('succeeds with assemblyFile and type name', function () {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			typeName: 'Edge.Tests.Startup'
		});
		assert.equal(typeof func, 'function');
	});

	it('fails with assemblyFile and nonexisting type name', function () {
		assert.throws(
			function () {
				edge.func({
					assemblyFile: edgeTestDll,
					typeName: 'Edge.Tests.idontexist'
				});
			},
			/Could not load type 'Edge.Tests.idontexist'/
		);
	});

	it('succeeds with assemblyFile, type name, and method name', function () {
		var func = edge.func({
			assemblyFile: edgeTestDll,
			typeName: 'Edge.Tests.Startup',
			methodName: 'Invoke'
		});
		assert.equal(typeof func, 'function');
	});

	it('fails with assemblyFile, type name and nonexisting method name', function () {
		assert.throws(
			function () {
				edge.func({
					assemblyFile: edgeTestDll,
					typeName: 'Edge.Tests.Startup',
					methodName: 'idontexist'
				});
			},
			/Unable to access the CLR method to wrap through reflection/
		);
	});

});