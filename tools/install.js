var fs = require('fs')
	, path = require('path');

if(process.platform === 'win32') {
	var libroot = path.resolve(__dirname, '../lib/native/win32')
		, dllname = 'msvcr120.dll'
		, lib32bit = path.resolve(libroot, 'ia32')
		, dll32bit = path.resolve(lib32bit, dllname)
		, lib64bit = path.resolve(libroot, 'x64')
		, dll64bit = path.resolve(lib64bit, dllname)

	function copyDll(dllPath) {
		return function(copyToDir) {
			fs.writeFileSync(path.resolve(copyToDir, dllname), fs.readFileSync(dllPath))
		}
	}

	function isDirectory(info) {
		return info.isDirectory
	}

	function getInfo(basedir) {
		return function(file) {
			var filepath = path.resolve(basedir, file)
			return {
				path: filepath
			, isDirectory: fs.statSync(filepath).isDirectory()
			}
		}
	}

	function getPath(info) {
		return info.path
	}

	fs.readdirSync(lib32bit)
		.map(getInfo(lib32bit))
		.filter(isDirectory)
		.map(getPath)
		.forEach(copyDll(dll32bit))

	fs.readdirSync(lib64bit)
		.map(getInfo(lib64bit))
		.filter(isDirectory)
		.map(getPath)
		.forEach(copyDll(dll64bit))


	require('./checkplatform')
} else {
	require('child_process')
		.spawn('node-gyp', ['configure', 'build'], { stdio: 'inherit' });
}
