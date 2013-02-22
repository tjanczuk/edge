var owin;

if (process.platform === 'win32') {
	owin = require('./native/' + process.platform + '/' + process.arch + '/owin')
}
else {
	throw new Error('The owin module is currently only supported on Windows. I do take contributions. '
		+ 'https://github.com/tjanczuk/owin');
}

module.exports = function(options) {
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
				if (Array.isArray(body)) {
					body = Buffer.concat(body);
					res.send(statusCode, body);
				}
				else {
					res.send(statusCode);
				}
			} catch (e) {
				return next(e);
			}
		});
	};
};
