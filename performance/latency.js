function help() {
    console.log('Usage: node latency.js i|x <N>');
    console.log('   i - 1 compilation, in-process loop of N calls');
    console.log('   c - in-process loop of N compilations and calls');
    console.log('   x - child process loop of N iterations');
    console.log('e.g. node latency.js i 1000');
    process.exit(1);    
}

if (process.argv.length !== 4)
    help();

if (isNaN(process.argv[3]))
    help();

var N = +process.argv[3];

if (N < 1)
    help();

if (process.argv[2] === 'i') {
    var func = require('../lib/edge').func(function () {/*
        async (input) => {
            return ".NET welcomes " + input.ToString();
        }
    */});
    var M = N;
    var start = Date.now();
    one_i();
    function one_i() {
        func('Node.js', function (error, result) {
            if (error) throw error;
            if (--M <= 0) {
                var delta = Date.now() - start;
                console.log(delta, delta / N);
            }
            else 
                setImmediate(one_i);
        });
    }
}else if (process.argv[2] === 'c') {
    var csharp = 'async (input) => { return ".NET welcomes " + input.ToString(); } /*';

    var edge = require('../lib/edge');
    var M = N;
    var start = Date.now();
    one_c();
    function one_c() {
        var code = csharp + M +  '*/'; // force cache miss and recompile
        var func = edge.func(code);
        func('Node.js', function (error, result) {
            if (error) throw error;
            if (--M <= 0) {
                var delta = Date.now() - start;
                console.log(delta, delta / N);
            }
            else 
                setImmediate(one_c);
        });
    }
}
else if (process.argv[2] === 'x') {
    var child_process = require('child_process');
    var cmd = [process.argv[0], process.argv[1], 'i', '1'].join(' ');
    var M = N;
    var start = Date.now();
    one_x();
    function one_x() {
        child_process.exec(cmd, function (error) {
            if (error) throw error;
            if (--M <= 0) {
                var delta = Date.now() - start;
                console.log(delta, delta / N);
            }
            else 
                setImmediate(one_x);
        });
    }
}
else
    help();
