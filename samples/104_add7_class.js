// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var add7 = edge.func(function () {/*
	using System.Threading.Tasks;

	public class Startup
	{
		public async Task<object> Invoke(object input)
		{
			return this.Add7((int)input);
		}

		int Add7(int v) 
		{
			return Helper.Add7(v);
		}
	}

	static class Helper
	{
		public static int Add7(int v)
		{
			return v + 7;
		}
	}
*/});

add7(12, function (error, result) {
	if (error) throw error;
	console.log(result);
});