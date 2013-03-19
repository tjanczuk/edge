// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var writeEventLog = edge.func(function() {/*
    async (input) => 
    {
        var parameters = (System.Collections.Generic.IDictionary<string,object>)input;
        var source = (string)parameters["source"];
        var log = (string)parameters["log"];
        if (!System.Diagnostics.EventLog.SourceExists(source))
        {
            System.Diagnostics.EventLog.CreateEventSource(source, log);
        }

        System.Diagnostics.EventLog.WriteEntry(
            source,
            (string)parameters["message"],
            (System.Diagnostics.EventLogEntryType)Enum.Parse(
                typeof(System.Diagnostics.EventLogEntryType), (string)parameters["type"]),
            (int)parameters["id"]
        );

        return null;
    }
*/});

writeEventLog({
    source: 'Edge.js sample application',
    log: 'Application',
    type: 'Information',
    message: 'Hello from node.js. The time is ' + new Date(),
    id: 31415
}, true);

console.log('Success. Check EventViewer.');
