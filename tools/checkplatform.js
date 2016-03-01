var
  path = require('path'),
  spawn = require('child_process').spawn;

try {
	require('../lib/edge.js');
}
catch (e) {
  if (process.platform === 'win32' && /module has not been pre-compiled/.exec(e)) {
    spawn(path.resolve(__dirname, 'build.bat'), ['release'], { stdio: 'inherit' });
  } else {
    console.log('***************************************');
    console.log(e);
    console.log('***************************************');
  }
}

console.log('Success: platform check for edge.js: node.js ' + process.arch + ' v' + process.versions.node);
