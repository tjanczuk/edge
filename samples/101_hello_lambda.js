// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('edge');

var hello = edge.func('async (input) => { return ".NET welcomes " + input.ToString(); }');

hello('Node.js', function (error, result) {
	if (error) throw error;
	console.log(result);
});