// Overview of edge.js: http://tjanczuk.github.com/edge

// Compile Sample105.dll with
// - on Windows (.NET Framework):
//      csc.exe /target:library /debug Sample105.cs
// - on MacOS/Linux (Mono):
//      mcs -sdk:4.5 Sample105.cs -target:library

var edge = require('../lib/edge');

var add7 = edge.func('Sample105.dll');

add7(12, function (error, result) {
	if (error) throw error;
	console.log(result);
});