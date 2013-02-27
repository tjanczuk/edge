var owin;

if (process.platform === 'win32') {
	owin = require('./native/' + process.platform + '/' + process.arch + '/owin')
}
else {
	throw new Error('The owin module is currently only supported on Windows. I do take contributions. '
		+ 'https://github.com/tjanczuk/owin');
}

var sqlOwinOptions = {
	assemblyFile: __dirname + '\\clr\\Owinjs.dll',
	typeName: 'Owinjs.Sql'
};

var sqlOwinKey = getOwinAppKey(sqlOwinOptions);
var workerApps = {};

function createOwinEnv(payload) {
	var inputString = JSON.stringify(payload);
	return {
		'owin.RequestMethod': 'GET',
		'owin.RequestPath': '/',
		'owin.RequestPathBase': '',
		'owin.RequestProtocol': 'HTTP/1.1',
		'owin.RequestQueryString': '',
		'owin.RequestScheme': 'http',
		'owin.RequestHeaders': {
			'Content-Type': 'application/json', 
			'Content-Length': '' + Buffer.byteLength(inputString)
		},
		'owin.RequestBody': inputString
	};
}

function createOwinCallback(callback) {
	return function (error, result) {
		if (typeof callback !== 'function') {
			return;
		}

		if (error) {
			return callback(error);
		}

		var statusCode = result['owin.ResponseStatusCode'];
		if (statusCode !== 200) {
			return callback(new Error('OWIN handler did not provide a successful response status code.'));
		}

		var response;
		try {
			response = JSON.parse(result['owin.ResponseBody'].toString('utf8'));
		} catch (e) {
			return callback(new Error('OWIN handler did not return JSON object in the response: ' + e))
		}

		return callback(null, response);
	};
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

	return options;
}

function getOwinAppKey(options) {
	return options.typeName + ', ' + options.assemblyFile;
}

module.exports = function(options) {
	options = normalizeOwinSpec(options);
	var appId = owin.initializeOwinApp(options);

	return function (req, res, next) {

		if (typeof req.body !== 'object') {
			throw new Error('The request body is not present or is not an object. '+
				'Please make sure the bodyParser middleware is installed.')
		}

		var env = {
			'owin.RequestMethod': req.method,
			'owin.RequestPath': req._parsedUrl.pathname,
			'owin.RequestPathBase': '',
			'owin.RequestProtocol': 'HTTP/' + req.httpVersion,
			'owin.RequestQueryString': req._parsedUrl.query || '',
			'owin.RequestScheme': req.protocol,
			'owin.RequestHeaders': req.headers,
			'owin.RequestBody': JSON.stringify(req.body)
		};

		owin.callOwinApp(appId, env, function (error, result) {
			var statusCode = result['owin.ResponseStatusCode'];
			if (error || typeof statusCode !== 'number') {
				return next(error);
			}

			try {
				var headers = result['owin.ResponseHeaders'];
				if (typeof headers === 'object') {
					for (var i in headers) {
						res.set(i, headers[i]);
					}
				}

				var body = result['owin.ResponseBody'];
				res.send(statusCode, body); // body can be undefined 
			} catch (e) {
				return next(e);
			}
		});
	};
};

module.exports.worker = function (options, input, callback) {
	if (typeof input !== 'object') {
		throw new Error('The input parameter must be an object with string properties.');
	}

	for (var i in input) {
		if (typeof input[i] !== 'string' && typeof input[i] !== 'number' && typeof input[i] !== 'boolean') {
			throw new Error('The input parameter must be an object with primitive properties.');
		}
	}

	options = normalizeOwinSpec(options);
	var key = getOwinAppKey(options);
	var appId = workerApps[key];
	if (!appId) {	
		appId = owin.initializeOwinApp(options);
		workerApps[key] = appId;
	}

	owin.callOwinApp(appId, createOwinEnv(input), createOwinCallback(callback));
};

module.exports.sql = function (options, callback) {
	if (typeof options === 'string') {
		options = { command: options };
	}

	if (typeof options !== 'object') {
		throw new Error('The first parameter must be a string SQL command or an options object.');
	}

	if (typeof options.command !== 'string') {
		throw new Error('The options object must specify a command parameter as a string.');
	}

	if (options.connectionString === undefined) {
		options.connectionString = process.env.OWIN_SQL_CONNECTION_STRING;
	}

	if (typeof options.connectionString !== 'string') {
		throw new Error('The options object must specify a connectionString parameter or the '
			+ 'SQL connection string must be provided in OWIN_SQL_CONNECTION_STRING environment variable.');
	}

	var appId = workerApps[sqlOwinKey];
	if (!appId) {	
		appId = owin.initializeOwinApp(sqlOwinOptions);
		workerApps[sqlOwinKey] = appId;
	}

	owin.callOwinApp(appId, createOwinEnv(options), createOwinCallback(callback));
};
