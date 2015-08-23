var http;
var url;

//iojs or nodejs?
var version = process.argv[3].split('.');
if(version[0] !== '0') {
	//iojs
	http = require('https');
	url = 'https://iojs.org/dist/v' + process.argv[3] + '/' 
	+ (process.argv[2] === 'x86' ? 'win-x86/iojs.exe' : 'win-x64/iojs.exe');
} else {
	//node
	http = require('http');
	url = 'http://nodejs.org/dist/v' + process.argv[3] + '/' 
	+ (process.argv[2] === 'x86' ? '/node.exe' : '/x64/node.exe');
}

http.get(url, function (res) {
	if (res.statusCode !== 200)
		throw new Error('HTTP response status code ' + res.statusCode);

	var stream = require('fs').createWriteStream(process.argv[4] + '/node.exe');
	res.pipe(stream);
});