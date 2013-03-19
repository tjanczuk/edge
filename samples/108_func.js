// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var addAndMultiplyBy2 = edge.func(function () {/*
	using System;
	using System.Threading.Tasks;
	using System.Collections.Generic;

	public class Startup
	{
		public async Task<object> Invoke(IDictionary<string,object> input)
		{
			int sum = (int)(input["a"]) + (int)(input["b"]);
			var multiplyBy2 = (Func<object,Task<object>>)input["multiplyBy2"];
			return await multiplyBy2(sum);
		}
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
