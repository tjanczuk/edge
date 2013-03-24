var edge = require('../lib/edge.js'), assert = require('assert');

describe('call patterns', function () {

	// Regression test for https://github.com/tjanczuk/edge/issues/22
	it('two async callouts each with async callin (issue #22)', function (done) {
		var log = [];

		var callin = function (input, callback) {
			log.push(input);
			callback();
		};

		var callout = edge.func(function () {/*
			using System;
			using System.Collections.Generic;
			using System.Threading.Tasks;

			public class Startup {
				public async Task<object> Invoke(IDictionary<string, object> input) {
					Func<object, Task<object>> callin = (Func<object, Task<object>>)input["callin"];
					await callin(input["payload"]);
					return input["payload"];
				}
			}
		*/});

		var tryFinish = function() {
			if (log.length === 4) {
				assert.deepEqual(log, [ 'callin1', 'callin2', 'return', 'return' ]);
				done();
			}
		};

		var callback = function (error, result) {
			assert.ifError(error);
			log.push('return');
			tryFinish();
		};

		callout({ payload: 'callin1', callin: callin }, callback);
		callout({ payload: 'callin2', callin: callin }, callback);
	});
});