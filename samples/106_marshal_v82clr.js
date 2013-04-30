// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var hello = edge.func(function () {/*
	using System.Collections.Generic;

	async (data) =>
	{
		Console.WriteLine("-----> In .NET:");
		foreach (var kv in (IDictionary<string,object>)data)
		{
			Console.WriteLine(kv.Key + ": " + kv.Value.GetType());
		}

		return null;
	}
*/});

var payload = {
	anInteger: 1,
	aNumber: 3.1415,
	aString: 'foobar',
	aBool: true,
	anObject: {},
	anArray: [ 'a', 1, true ],
	aBuffer: new Buffer(1024)
}

console.log('-----> In node.js:');
console.log(payload);

hello(payload, function (error, result) {
	if (error) throw error;
});
