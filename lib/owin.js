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
		if (options.match(/\.dll$/i)) {
			options = { assemblyFile: options };
		}
		else if (options.match(/\.csx?$/i)) {
			options = { csx: require('fs').readFileSync(options, 'utf8') };
		}
		else {
			options = { csx: options };
		}
	}
	else if (typeof options === 'function') {
		options = { csx: options }			
	}
	else if (typeof options != 'object') {
		throw new Error('Specify the file name of the CLR DLL or provide an options object.');
	}

	if (!options.assemblyFile && !options.csx) {
		throw new Error('Provide DLL or CSX file name or C# literal as a string parmeter, or specify an options object '
			+ 'with assemblyFile or csx string property.');
	}
	else if (options.assemblyFile && options.csx) {
		throw new Error('Provide either an asseblyFile or csx property, but not both.');
	}

	if (typeof options.csx === 'function') {
		var match = options.csx.toString().match(/[^]*\/\*([^]*)\*\/\}$/);
		if (match) {
			options.csx = match[1];
		}
		else {
			throw new Error('If C# source is provided as JavaScript function, function body must be a /* ... */ comment.')
		}				
	}

	if (options.references !== undefined) {
		if (!Array.isArray(options.references)) {
			throw new Error('The references property must be an array of strings.');
		}

		options.references.forEach(function (ref) {
			if (typeof ref !== 'string') {
				throw new Error('The references property must be an array of strings.');
			}
		});
	}

	if (typeof options.csx === 'string') {
		var matches = options.csx.match(/\/\/\#r\s+\"[^\"]+\"\s*/g) || [];
		matches.forEach(function (match) {
			var ref = match.match(/\/\/\#r\s+\"([^\"]+)\"\s*/);
			if (ref) {
				if (options.references) {
					options.references.push(ref[1]);
				}
				else {
					options.references = [ ref[1] ];
				}
			}
		});
	}

	if (options.assemblyFile) {
		if (!options.typeName) {
			var match = options.assemblyFile.match(/([^\\\/]+)\.dll$/i);
			if (!match) {
				throw new Error('Unable to determine the namespace name based on assembly file name. ' +
					'Specify typeName parameter as a namespace qualified CLR type name of the application class.');
			}

			options.typeName = match[1] + '.Startup';
		}
	}
	else if (!options.typeName) {
		options.typeName = "Startup";
	}

	if (!options.methodName) {
		options.methodName = 'Invoke';
	}	

	return options;
}

function getOwinAppKey(options) {
	return options.csx || (options.typeName + ', ' + options.methodName + ', ' + options.assemblyFile);
}

exports.func = function(options) {
	options = normalizeOwinSpec(options);
	var key = getOwinAppKey(options);
	var appId = workerApps[key];
	if (!appId) {	
		appId = owin.initializeClrFunc(options);
		workerApps[key] = appId;
	}

	return function (data, callback) {
		owin.callClrFunc(appId, data, callback);
	};
}
