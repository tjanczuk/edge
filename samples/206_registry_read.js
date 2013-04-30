var edge = require('../lib/edge');

var readRegistery = edge.func(function () {/*
		using System.Collections.Generic;
		using Microsoft.Win32;

		async (data) =>
		{
			var input = (IDictionary<string,object>)data;
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
	*/}
);

readRegistery({keyName: 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\MSBuild\\4.0', valueName: 'MSBuildOverrideTasksPath'}, function (err, result) {
	if (err) {
		throw new Error(err);
	}

	console.log(result);
});