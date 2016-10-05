var spawn = require('child_process').spawn;
var path = require('path');
var testDir = path.resolve(__dirname, '../test');
var input = path.resolve(testDir, 'tests.cs');
var output = path.resolve(testDir, 'Edge.Tests.dll');
var buildParameters = ['-target:library', '/debug', '-out:' + output, input];
var mocha = path.resolve(__dirname, '../node_modules/mocha/bin/mocha');
var fs = require('fs');

if (!process.env.EDGE_USE_CORECLR) {
	if (process.platform !== 'win32') {
		buildParameters = buildParameters.concat(['-sdk:4.5']);
	}

	spawn(process.platform === 'win32' ? 'csc' : 'dmcs', buildParameters, { 
		stdio: 'inherit' 
	}).on('close', runOnSuccess);
} 

else {
	spawn(process.platform === 'win32' ? 'dotnet.exe' : 'dotnet', ['restore'], { 
		stdio: 'inherit', 
		cwd: testDir 
	}).on('close', function(code, signal) {
		if (code === 0) {
			spawn(process.platform === 'win32' ? 'dotnet.exe' : 'dotnet', ['build'], { 
				stdio: 'inherit', 
				cwd: testDir 
			}).on('close', runOnSuccess);
		}
	});
}

function runOnSuccess(code, signal) {
	if (code === 0) {
		process.env['EDGE_APP_ROOT'] = path.join(testDir, 'bin', 'Debug', 'netcoreapp1.0');

		spawn('node', [mocha, testDir, '-R', 'spec', '-t', '10000', '-gc'], { 
			stdio: 'inherit' 
		}).on('error', function(err) { 
			console.log(err); 
		});
	}
}
