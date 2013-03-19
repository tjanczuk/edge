// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var hello = edge.func('async (input) => { return ".NET welcomes " + input.ToString(); }');

// call the function synchronously
var result = hello('Node.js', true);
console.log(result);

// call the same function asynchronously
hello('JavaScript', function (error, result) {
	if (error) throw error;
	console.log(result);
});
