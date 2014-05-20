var callCount = process.env.EDGE_CALL_COUNT || 10000;

var measure = function (func) {
	var start = Date.now();
	var i = 0;

	function one() {
		func({
			title: 'Run .NET and node.js in-process with edge.js',
			author: {
				first: 'Tomasz',
				last: 'Janczuk'
			},
			year: 2013,
			price: 24.99,
			available: true, 
			description: 'Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus posuere tincidunt felis, et mattis mauris ultrices quis. Cras molestie, quam varius tincidunt tincidunt, mi magna imperdiet lacus, quis elementum ante nibh quis orci. In posuere erat sed tellus lacinia luctus. Praesent sodales tellus mauris, et egestas justo. In blandit, metus non congue adipiscing, est orci luctus odio, non sagittis erat orci ac sapien. Proin ut est id enim mattis volutpat. Vivamus ultrices dapibus feugiat. In dictum tincidunt eros, non pretium nisi rhoncus in. Duis a lacus et elit feugiat ullamcorper. Mauris tempor turpis nulla. Nullam nec facilisis elit.',
			picture: new Buffer(16000),
			tags: [ '.NET', 'node.js', 'CLR', 'V8', 'interop']			
		}, function (error, callbck) {
			if (error) throw error;
			if (++i < callCount) setImmediate(one);
			else finish();
		});
	}

	function finish() {
		var delta = Date.now() - start;
		var result = process.memoryUsage();
		result.latency = delta / callCount;
		console.log(result);
	}

	one();
};

var baseline = function () {
	measure(function (input, callback) {
		var book = {
			title: 'Run .NET and node.js in-process with edge.js',
			author: {
				first: 'Tomasz',
				last: 'Janczuk'
			},
			year: 2013,
			price: 24.99,
			available: true, 
			description: 'Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus posuere tincidunt felis, et mattis mauris ultrices quis. Cras molestie, quam varius tincidunt tincidunt, mi magna imperdiet lacus, quis elementum ante nibh quis orci. In posuere erat sed tellus lacinia luctus. Praesent sodales tellus mauris, et egestas justo. In blandit, metus non congue adipiscing, est orci luctus odio, non sagittis erat orci ac sapien. Proin ut est id enim mattis volutpat. Vivamus ultrices dapibus feugiat. In dictum tincidunt eros, non pretium nisi rhoncus in. Duis a lacus et elit feugiat ullamcorper. Mauris tempor turpis nulla. Nullam nec facilisis elit.',
			picture: new Buffer(16000),
			tags: [ '.NET', 'node.js', 'CLR', 'V8', 'interop']			
		}
		callback(null, book);
	});
};

var clr2v8 = function () {
	measure(require('../lib/edge').func(function () {/*
		async (input) => 
		{
			var book = new {
				title = "Run .NET and node.js in-process with edge.js",
				author = new {
					first = "Tomasz",
					last = "Janczuk"
				},
				year = 2013,
				price = 24.99,
				available = true, 
				description = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus posuere tincidunt felis, et mattis mauris ultrices quis. Cras molestie, quam varius tincidunt tincidunt, mi magna imperdiet lacus, quis elementum ante nibh quis orci. In posuere erat sed tellus lacinia luctus. Praesent sodales tellus mauris, et egestas justo. In blandit, metus non congue adipiscing, est orci luctus odio, non sagittis erat orci ac sapien. Proin ut est id enim mattis volutpat. Vivamus ultrices dapibus feugiat. In dictum tincidunt eros, non pretium nisi rhoncus in. Duis a lacus et elit feugiat ullamcorper. Mauris tempor turpis nulla. Nullam nec facilisis elit.",
				picture = new byte[16000],
				tags = new object[] { ".NET", "node.js", "CLR", "V8", "interop" }
			};
			return book;			
		}
	*/}));
};

var crossProcess = function () {
	var http = require('http');
	measure(function (input, callback) {
		http.get("http://localhost:31415/api/book", function(res) {
			if (res.statusCode !== 200) {
				return callback(new Error('Status code: ' + res.statusCode));
			}

			var body = '';
			res.on('data', function (chunk) { body += chunk; });
			res.on('end', function () {
				callback(null, JSON.parse(body));
			})
		}).on('error', callback);
	});
};

var cases = {
	js: baseline,
	edge: clr2v8,
	xproc: crossProcess
};

if (!cases[process.argv[2]]) {
	console.log('Usage: marshal_clr2v8.js ' + Object.getOwnPropertyNames(cases).join('|'));
	process.exit(1);
}
else {
	cases[process.argv[2]]();
}
