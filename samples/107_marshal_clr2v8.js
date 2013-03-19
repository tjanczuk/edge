// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var hello = edge.func(function () {/*
	async (input) => 
	{
		var result = new {
			anInteger = 1,
			aNumber = 3.1415,
			aString = "foobar",
			aBool = true,
			anObject = new { a = "b", c = 12 },
			anArray = new object[] { "a", 1, true },
			aBuffer = new byte[1024]
		};

		return result;
	}
*/});

hello(null, function (error, result) {
	if (error) throw error;
	console.log('-----> In node.js:');
	console.log(result);
});
