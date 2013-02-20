if (process.platform === 'win32')
	module.exports = require('./native/' + process.platform + '/' + process.arch + '/owin')
else
	throw new Error('The tripwire module is currently only supported on Windows. I do take contributions. '
		+ 'https://github.com/tjanczuk/owin');
