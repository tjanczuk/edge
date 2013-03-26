// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var createCounter = edge.func(function () {/*
    async (start) => 
    {
        var k = (int)start;
        return (Func<object,Task<object>>)(
            async (i) => 
            { 
                return k++;
            }
        );
    }
*/});

var nextNumber = createCounter(12, true);
console.log(nextNumber(null, true));
console.log(nextNumber(null, true));
console.log(nextNumber(null, true));
