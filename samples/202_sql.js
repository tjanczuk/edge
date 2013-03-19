// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var sql = edge.func('202_sql.csx');

sql('select top 2 * from Products', function (error, result) {
	if (error) throw error;
	console.log(result);
});
