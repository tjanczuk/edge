// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var addAndMultiplyBy2 = edge.func(function () {/*
	using System.Collections.Generic;

	async (dynamic data) => 	
	{
		int sum = (int)data.a + (int)data.b;
		var multiplyBy2 = (Func<object,Task<object>>)data.multiplyBy2;
		return await multiplyBy2(sum);
	}
*/});

var payload = {
	a: 2,
	b: 3,
	multiplyBy2: function(input, callback) {
		callback(null, input * 2);
	}
};

addAndMultiplyBy2(payload, function (error, result) {
	if (error) throw error;
	console.log(result);
});
