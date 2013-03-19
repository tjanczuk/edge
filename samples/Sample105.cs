using System.Threading.Tasks;

namespace Sample105
{
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
}