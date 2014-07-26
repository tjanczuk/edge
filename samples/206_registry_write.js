var edge = require('../lib/edge');

var writeRegistery = edge.func(function () {/*
	using Microsoft.Win32;

	async (dynamic input) =>
	{
		Registry.SetValue((string)input.keyName, (string)input.valueName, input.value);
		return null;
	}
*/});

writeRegistery({
	keyName: 'HKEY_CURRENT_USER\\Environment', 
	valueName: 'MyCustomValue', 
	value: 1050 
}, function (err) {
	if (err) {
		throw err;
	}

	console.log('Done!');
});