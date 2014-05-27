// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge')
	, async = require('async');

var helloCs = edge.func(function () {/*
	async (input) => 
	{ 
		return "C# welcomes " + input.ToString(); 
	}
*/});

var helloPy = edge.func('py', function () {/*
    def hello(input):
        return "Python welcomes " + input

    lambda x: hello(x)
*/});

var helloFs = edge.func('fs', function () {/*
    fun input -> async { 
        return "F# welcomes " + input.ToString()
    }
*/});

var helloPs = edge.func('ps', function () {/*
	"PowerShell welcomes $inputFromJS"
*/});

async.waterfall([
	function (cb) { cb(null, 'Node.js'); },

	helloFs,
	helloCs,
	helloPy,
	helloPs
], function (error, result) {
	console.log(error || result[0]);
});