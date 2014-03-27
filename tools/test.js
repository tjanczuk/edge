var spawn = require('child_process').spawn
	, path = require('path')
	, testDir = path.resolve(__dirname, '../test')
	, input = path.resolve(testDir, 'tests.cs')
	, output = path.resolve(testDir, 'Edge.Tests.dll')
	, buildParameters = ['-target:library', '/debug', '-out:' + output, input]
	, winBuild = []
	, monoBuild = ['-sdk:4.5']
	, mocha = path.resolve(__dirname, '../node_modules/mocha/bin/mocha')
	;

if(process.platform === 'win32') {
	spawn('csc', winBuild.concat(buildParameters), { stdio: 'inherit' })
		.on('close', runOnSuccess);
} else {
	spawn('dmcs', monoBuild.concat(buildParameters), { stdio: 'inherit' })
		.on('close', runOnSuccess);
}

function runOnSuccess(code, signal) {
	if(code === 0) {
		spawn('node', [mocha, testDir, '-R', 'spec'], { stdio: 'inherit' })
			.on('error', function(err) { console.log(err); });
	}
}