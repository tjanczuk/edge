var fs = require('fs');
var path = require('path');

module.exports = function() {
    var pathSep = process.platform === 'win32' ? ';' : ':';

    var directories = process.env.PATH.split(pathSep);

    for (var i = 0; i < directories.length; i++) {
    	for (var j = 0; j < arguments.length; j++) {
    		var filename = arguments[j];
	        var filePath = path.join(directories[i], filename);

	        if (fs.existsSync(filePath)) {
	            return filePath;
	        }
	    }
    }

    // Return empty string '' when not found. This supports "if (whereis_result) {...}"
    // test, and also GYP condition test <(!node -e `...console.log(whereis_result)`) != ''
    return '';
}
