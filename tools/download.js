var http = require('http');

var url = 'http://nodejs.org/dist/v' + process.argv[3] + '/' 
	+ (process.argv[2] === 'x86' ? '/node.exe' : '/x64/node.exe');

console.log(url);
http.get(url, function (res) {
	if (res.statusCode !== 200)
		throw new Error('HTTP response status code ' + res.statusCode);

	var stream = require('fs').createWriteStream(process.argv[4] + '/node.exe');
	res.pipe(stream);
});