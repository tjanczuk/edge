// Overview of edge.js: http://tjanczuk.github.com/edge

// Compile Sample105.dll with
// csc.exe /target:library /debug Sample105.cs

var edge = require('../lib/edge');

var add7 = edge.func('Sample105.dll');

add7(12, function (error, result) {
	if (error) throw error;
	console.log(result);
});