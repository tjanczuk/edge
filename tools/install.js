var fs = require('fs')
	, path = require('path');

if(process.platform === 'win32') {
	var libroot = path.resolve(__dirname, '../lib/native/win32')
		, lib32bit = path.resolve(libroot, 'ia32')
		, lib64bit = path.resolve(libroot, 'x64');

	function copyDll(dllPath, dllname) {
		return function(copyToDir) {
			fs.writeFileSync(path.resolve(copyToDir, dllname), fs.readFileSync(dllPath));
		}
	}

	function isDirectory(info) {
		return info.isDirectory;
	}

	function getInfo(basedir) {
		return function(file) {
			var filepath = path.resolve(basedir, file)
			return {
				path: filepath,
				isDirectory: fs.statSync(filepath).isDirectory()
			}
		}
	}

	function getPath(info) {
		return info.path;
	}

	var dest32dirs = fs.readdirSync(lib32bit)
		.map(getInfo(lib32bit))
		.filter(isDirectory)
		.map(getPath);

	['msvcr120.dll', 'msvcp120.dll'].forEach(function (dllname) {
		var dll32bit = path.resolve(lib32bit, dllname);
		dest32dirs.forEach(copyDll(dll32bit, dllname));
	});
		
	var dest64dirs = fs.readdirSync(lib64bit)
		.map(getInfo(lib64bit))
		.filter(isDirectory)
		.map(getPath);

	['msvcr120.dll', 'msvcp120.dll'].forEach(function (dllname) {
		var dll64bit = path.resolve(lib64bit, dllname);
		dest64dirs.forEach(copyDll(dll64bit, dllname));
	});

	require('./checkplatform');
} else {
	require('child_process')
		.spawn('node-gyp', ['configure', 'build'], { stdio: 'inherit' });
}
