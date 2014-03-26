var spawn = require('child_process').spawn
	, path = require('path')
	, input = path.resolve(__dirname, 'tests.cs')
	, output = path.resolve(__dirname, 'Edge.Tests.dll')
	, buildParameters = ['-target:library', '/debug', '-out:' + output, input]
	, winBuild = []
	, monoBuild = ['-sdk:4.5']
	, mocha = path.resolve(__dirname, '../node_modules/mocha/bin/mocha')
	;

if(process.platform === 'win32') {
	spawn('csc', winBuild.concat(buildParameters))
		.on('close', runOnSuccess);
} else {
	spawn('dmcs', monoBuild.concat(buildParameters))
		.on('close', runOnSuccess);
}

function runOnSuccess(code, signal) {
	if(code === 0) {
		spawn('node', [mocha, '-R', 'spec'], { stdio: 'inherit' })
			.on('error', function(err) { console.log(err); });
	}
}