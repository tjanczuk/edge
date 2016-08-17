/**
 * Created by kagentes on 8/17/2016.
 */
'use strict';
(function () {
    var resolved_npm = require.resolve('npm/node_modules/node-gyp/bin/node-gyp');
    process.stdout.write(resolved_npm);
})();
