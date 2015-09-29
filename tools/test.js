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
	spawn('dnu', ['restore'], { 
		stdio: 'inherit', 
		cwd: testDir 
	}).on('close', function(code, signal) {
		if (code === 0) {
			spawn('dnu', ['build'], { 
				stdio: 'inherit', 
				cwd: testDir 
			}).on('close', function(code, signal) {
				var outputStream = fs.createWriteStream(output);
				fs.createReadStream(path.join(testDir, 'bin', 'Debug', 'dnxcore50', 'test.dll')).pipe(outputStream);

				outputStream.on('close', function() {
					runOnSuccess(0);
				});
			});
		}
	});
}

function runOnSuccess(code, signal) {
	if (code === 0) {
		process.env['EDGE_APP_ROOT'] = testDir;

		spawn('node', [mocha, testDir, '-R', 'spec'], { 
			stdio: 'inherit' 
		}).on('error', function(err) { 
			console.log(err); 
		});
	}
}
