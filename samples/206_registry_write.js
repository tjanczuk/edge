var edge = require('../lib/edge');

var writeRegistery = edge.func(function () {/*
		using System.Collections.Generic;
		using Microsoft.Win32;

		async (data) =>
		{
			var input = (IDictionary<string,object>)data;
			var keyName = (string)input["keyName"];
			var valueName = (string)input["valueName"];
			var value = input["value"];

			Registry.SetValue(keyName, valueName, value);

			return null;
		}
	*/}
);

writeRegistery({keyName: 'HKEY_CURRENT_USER\\Environment', valueName: 'MyCustomValue', value: 1050 }, function (err) {
	if (err) {
		throw new Error(err);
	}

	console.log('Done!');
});