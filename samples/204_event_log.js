// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var writeEventLog = edge.func(function() {/*
    using System.Collections.Generic;
    using System.Diagnostics;

    async (dynamic parameters) => 
    {
        var source = (string)parameters.source;
        var log = (string)parameters.log;
        if (!EventLog.SourceExists(source))
        {
            EventLog.CreateEventSource(source, log);
        }

        EventLog.WriteEntry(
            source,
            (string)parameters.message,
            (EventLogEntryType)Enum.Parse(
                typeof(EventLogEntryType), (string)parameters.type),
            (int)parameters.id
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
