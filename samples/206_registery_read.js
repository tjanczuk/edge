var edge = require('../lib/edge');

var readRegistery = edge.func(function () {/*
		using System;
		using System.Collections.Generic;
		using System.Threading.Tasks;
		using Microsoft.Win32;

		public class Startup
		{
			public async Task<object> Invoke(IDictionary<string, object> input)
			{
				return await Task.Run(() => ReadRegisteryKey(input));
			}

			private static object ReadRegisteryKey(IDictionary<string, object> input)
			{
				var keyName = (string) input["keyName"];
				var valueName = (string) input["valueName"];
				var defaultValue = (string) null;
				if (input.ContainsKey("defaultValue"))
				{
					defaultValue = (string) input["defaultValue"];
				}

				var readValue = Registry.GetValue(keyName, valueName, defaultValue);

				return readValue;
			}
		}
	*/}
);

readRegistery({keyName: 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\MSBuild\\4.0', valueName: 'MSBuildOverrideTasksPath'}, function (err, result) {
	if (err) {
		throw new Error(err);
	}

	console.log(result);
});