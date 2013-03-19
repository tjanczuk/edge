// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var hello = edge.func(function () {/*
	async (input) => 
	{ 
		// we are on V8 thread here

		return await Task.Run<object>(async () => {

			// we are on CLR thread pool thread here

			// simulate long running operation
			await Task.Delay(5000); 
			
			return ".NET welcomes " + input.ToString();
		});
	}
*/});

console.log('Starting CPU bound operation...');
hello('Node.js', function (error, result) {
	if (error) throw error;
	console.log(result);
});

setInterval(function() {
	console.log('Node.js event loop is alive');
}, 1000);
