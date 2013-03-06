var owin;

if (process.env.OWIN_NATIVE) {
	owin = require(process.env.OWIN_NATIVE);
}
else if (process.platform === 'win32') {
	owin = require('./native/' + process.platform + '/' + process.arch + '/owin')
}
else {
	throw new Error('The owin module is currently only supported on Windows. I do take contributions. '
		+ 'https://github.com/tjanczuk/owin');
}

var workerApps = {};

function normalizeOwinSpec(options) {
	if (typeof options === 'string') {
		options = { assemblyFile: options };
	}
	else if (typeof options != 'object') {
		throw new Error('Specify the file name of the OWIN DLL or provide an options object.');
	}
	else if (typeof options.assemblyFile !== 'string') {
		throw new Error('OWIN assembly file name must be provided as a string parameter or assemblyFile options property.');
	}

	if (!options.typeName) {
		var match = options.assemblyFile.match(/([^\\\/]+)\.dll$/i);
		if (!match) {
			throw new Error('Unable to determine the namespace name based on assembly file name. ' +
				'Specify typeName parameter as a namespace qualified CLR type name of the OWIN application class.');
		}

		options.typeName = match[1] + '.Startup';
	}

	if (!options.methodName) {
		options.methodName = 'Invoke';
	}

	return options;
}

function getOwinAppKey(options) {
	return options.typeName + ', ' + options.methodName + ', ' + options.assemblyFile;
}

exports.func = function(options) {
	options = normalizeOwinSpec(options);
	var key = getOwinAppKey(options);
	var appId = workerApps[key];
	if (!appId) {	
		appId = owin.initializeOwinApp(options);
		workerApps[key] = appId;
	}

	return function (data, callback) {
		owin.callOwinApp(appId, data, callback);
	};
}
