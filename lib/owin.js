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

function stringifyException(e) {
	if (typeof e === 'object') {
		return e.stack || e.toString();
	}
	else {
		return '' + e;
	}
}

function wrapFunction(func) {
	if (func._owinWrapped) {
		return func;
	}

	var result = function (correlator, payload, callback) {
		try {
			func(payload, function (error, result) {
				if (error) {
					callback(correlator, stringifyException(error), null);
				}
				else {
					callback(correlator, null, result);
				}
			});
		}
		catch (e) {
			callback(correlator, stringifyException(e), null);
		}
	};

	result._owinWrapped = true;

	return result;
}

function wrapFunctions(data) {
	if (typeof data == 'function') {
		return wrapFunction(data);
	}
	else if (typeof data == 'object' || Array.isArray(data)) {
		for (var i in data) {
			data[i] = wrapFunctions(data[i]);
		}
	}

	return data;
}

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
		// TODO: perform wrapping during marshaling from V8 to CLR in C++\CLI
		data = wrapFunctions(data);
		owin.callOwinApp(appId, data, callback);
	};
}
