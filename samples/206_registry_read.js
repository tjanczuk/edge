var edge = require('../lib/edge');

var readRegistery = edge.func(function () {/*
	using Microsoft.Win32;

	async (dynamic input) =>
	{
		return Registry.GetValue((string) input.keyName, (string) input.valueName, null);
	}
*/});

readRegistery({
	keyName: 'HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\MSBuild\\4.0', 
	valueName: 'MSBuildOverrideTasksPath'
}, function (err, result) {
	if (err) {
		throw err;
	}

	console.log(result);
});