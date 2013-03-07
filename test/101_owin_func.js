var owin = require('../lib/owin.js')
	, assert = require('assert');

var owinTestDll = __dirname + '\\Owin.Tests.dll';

describe('owin.func', function () {

	it('exists', function () {
		assert.equal(typeof owin.func, 'function');
	});

	it('fails without parameters', function () {
		assert.throws(
			owin.func,
			/Specify the file name of the CLR DLL or provide an options object/
		);		
	});

	it('fails with a wrong parameter', function () {
		assert.throws(
			function () { owin.func(12); },
			/Specify the file name of the CLR DLL or provide an options object/
		);		
	});

	it('fails with missing assemblyFile', function () {
		assert.throws(
			function () { owin.func({}); },
			/CLR assembly file name must be provided as a string parameter or assemblyFile options property/
		);		
	});

	it('fails with nonexisting assemblyFile', function () {
		assert.throws(
			function () { owin.func('idontexist.dll'); },
			/System.IO.FileNotFoundException/
		);		
	});

	it('succeeds with assemblyFile as string', function () {
		var func = owin.func(owinTestDll);
		assert.equal(typeof func, 'function');
	});

	it('succeeds with assemblyFile as options property', function () {
		var func = owin.func({ assemblyFile: owinTestDll });
		assert.equal(typeof func, 'function');
	});

	it('succeeds with assemblyFile and type name', function () {
		var func = owin.func({ 
			assemblyFile: owinTestDll, 
			typeName: 'Owin.Tests.Startup' 
		});
		assert.equal(typeof func, 'function');
	});

	it('fails with assemblyFile and nonexisting type name', function () {
		assert.throws(
			function () { 
				owin.func({ 
					assemblyFile: owinTestDll, 
					typeName: 'Owin.Tests.idontexist' 
				}); 
			},
			/Could not load type 'Owin.Tests.idontexist'/
		);			
	});

	it('succeeds with assemblyFile, type name, and method name', function () {
		var func = owin.func({ 
			assemblyFile: owinTestDll, 
			typeName: 'Owin.Tests.Startup',
			methodName: 'Invoke'
		});
		assert.equal(typeof func, 'function');
	});

	it('fails with assemblyFile, type name and nonexisting method name', function () {
		assert.throws(
			function () { 
				owin.func({ 
					assemblyFile: owinTestDll, 
					typeName: 'Owin.Tests.Startup',
					methodName: 'idontexist' 
				}); 
			},
			/Unable to access the CLR method to wrap through reflection/
		);			
	});

});