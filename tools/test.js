var spawn = require('child_process').spawn
	, path = require('path')
	, testDir = path.resolve(__dirname, '../test')
	, input = path.resolve(testDir, 'tests.cs')
	, output = path.resolve(testDir, process.env.EDGE_USE_CORECLR ? 'Edge.CoreClr.Tests.dll' : 'Edge.Tests.dll')
	, buildParameters = ['-target:library', '/debug', '-out:' + output, input]
	, winBuild = []
	, monoBuild = ['-sdk:4.5']
	, mocha = path.resolve(__dirname, '../node_modules/mocha/bin/mocha')
	, fs = require('fs')
	;

if(process.env.EDGE_USE_CORECLR && process.platform !== 'win32') {
	var dnxPath = path.dirname(whereis('dnx'));

	monoBuild = monoBuild.concat([
		'-nostdlib',
		'-noconfig',
		'-r:' + path.join(dnxPath, 'System.Collections.dll'),
		'-r:' + path.join(dnxPath, 'mscorlib.dll'),
		'-r:' + path.join(dnxPath, 'System.Dynamic.Runtime.dll'),
		'-r:' + path.join(dnxPath, 'Microsoft.CSharp.dll'),
		'-r:' + path.join(dnxPath, 'System.ObjectModel.dll'),
		'-r:' + path.join(dnxPath, 'System.Runtime.dll'),
		'-r:' + path.join(dnxPath, 'System.Linq.Expressions.dll')
	]);
}

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

function whereis(filename){
    var pathSep = process.platform === 'win32' ? ';' : ':';

    var directories = process.env.PATH.split(pathSep);

    for (var i = 0; i < directories.length; i++) {
        var path = directories[i] + '/' + filename;

        if (fs.existsSync(path)) {
            return path;
        }
    }

    return null;
}
