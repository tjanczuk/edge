var fs = require('fs');
var jshint = require('jshint').JSHINT;

var foldersToLint = [
	'./lib',
	'./test'
];

var utils = {
	errors: 0,

	readFile: function (filename) {
		return fs.readFileSync(filename, 'utf8');
	},

	lintFile: function (filename, config) {
		var results = jshint(this.readFile(filename), config.options, config.globals);
		if (!results) {
			this.errors++;
			this.renderResults(filename);
		}
	},

	resolveFiles: function(folders) {
		var files = [];
		folders.forEach(function (dir) {
			var filesInFolder = fs.readdirSync(dir);
			var jsFiles = filesInFolder.filter(function (file) {
				return file.indexOf('.js') > 0;
			}).map(function (js) {
				return dir + '/' + js;
			});

			files = files.concat(jsFiles);
		});

		return files;
	},

	renderResults: function (filename) {
		jshint.errors.forEach(function (e) {
			if (!e) {
				return;
			}
			console.error(filename + ': ' + e.code + ' ' + e.raw + ' at line: ' + e.line + ' character: ' + e.character);
		});
	}
};

var config = JSON.parse(utils.readFile('.jshintrc'));
var filesToLint = utils.resolveFiles(foldersToLint);

filesToLint.forEach(function (filename) {
	utils.lintFile(filename, config);
});

if (utils.errors > 0) {
	return process.exit(1);
}

console.log('Success: no linting errors.');