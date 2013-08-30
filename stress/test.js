// Tests calls from JS to CLR where CLR completes on V8 thread by invoking JS callback

(function () {
    var func = require('../lib/edge').func(function () {/*
        async (object input) => {
            return input;
        }
    */});

    var data = '{"data_package_id":"341f0a51-3521-400e-ba05-b86939433e9b","timestamp":1375988508262,"records":[{"id":"df63580b-5047-4ed2-a5f3-e928abd8414a","timestamp":1375988508118,"context":[],"target_users":[],"extension":{"eventRecordTime":1375988508118,"apiVersion":"v1","shortUrl":"shortUrl","httpMethod":"POST","callerUserAgent":"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.110 Safari/537.36","azureDeployName":"localhost","connectionId":"3f813fd4-aea9-4e7b-a44c-e4a3f75da38b"},"sampling_data":{},"context_ids":{"connectionId":"1bb5c8d4-acc2-4a01-aa7e-79317eedd5d5"},"event_type":"edf_trouter_request_check","type":"trouter"}],"ids":{"deployment":"TrouterTest"},"schema":1,"type":"Service","source":"Node Test","version":"1.0.0.0"}';

    function one() {
        func(data, function (error, data) {
            if (error) throw error;
            setTimeout(one, 1);
        });
    }

    one();
})();

// Tests calls from JS to CLR where CLR completes on V8 thread by returning value

(function () {
    var func = require('../lib/edge').func(function () {/*
        async (object input) => {
            return input;
        }
    */});

    var data = '{"data_package_id":"341f0a51-3521-400e-ba05-b86939433e9b","timestamp":1375988508262,"records":[{"id":"df63580b-5047-4ed2-a5f3-e928abd8414a","timestamp":1375988508118,"context":[],"target_users":[],"extension":{"eventRecordTime":1375988508118,"apiVersion":"v1","shortUrl":"shortUrl","httpMethod":"POST","callerUserAgent":"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.110 Safari/537.36","azureDeployName":"localhost","connectionId":"3f813fd4-aea9-4e7b-a44c-e4a3f75da38b"},"sampling_data":{},"context_ids":{"connectionId":"1bb5c8d4-acc2-4a01-aa7e-79317eedd5d5"},"event_type":"edf_trouter_request_check","type":"trouter"}],"ids":{"deployment":"TrouterTest"},"schema":1,"type":"Service","source":"Node Test","version":"1.0.0.0"}';

    function one() {
        func(data, true);
        setTimeout(one, 1);
    }

    one();
})();

// Tests calls from CLR to JS using a provided callback

(function () {
    var func = require('../lib/edge').func(function () {/*
        async (dynamic input) => {
            var jscallback = (Func<object,Task<object>>)input.jscallback;
            Task.Run(async () => {
                while (true) {
                    await Task.Delay(1);
                    await jscallback(input.data);
                }
            });
            return null;
        }
    */});

    func({
        data: '{"data_package_id":"341f0a51-3521-400e-ba05-b86939433e9b","timestamp":1375988508262,"records":[{"id":"df63580b-5047-4ed2-a5f3-e928abd8414a","timestamp":1375988508118,"context":[],"target_users":[],"extension":{"eventRecordTime":1375988508118,"apiVersion":"v1","shortUrl":"shortUrl","httpMethod":"POST","callerUserAgent":"Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.110 Safari/537.36","azureDeployName":"localhost","connectionId":"3f813fd4-aea9-4e7b-a44c-e4a3f75da38b"},"sampling_data":{},"context_ids":{"connectionId":"1bb5c8d4-acc2-4a01-aa7e-79317eedd5d5"},"event_type":"edf_trouter_request_check","type":"trouter"}],"ids":{"deployment":"TrouterTest"},"schema":1,"type":"Service","source":"Node Test","version":"1.0.0.0"}',
        jscallback: function (data, cb) {
            cb();
        } 
    }, true);
})();