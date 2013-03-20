// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var convertKilograms = edge.func('205_soap.csx');

convertKilograms(123, function (error, result) {
	if (error) throw error;
	console.log(result);
});
