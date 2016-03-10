// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var subscribe = edge.func(function() {/*
    async (dynamic input) =>
    {
        // Create a timer with the specifed interval. 
        // Conceptually this can be any event source. 

        var timer = new System.Timers.Timer(input.interval);

        // Hook up the Elapsed event for the timer and delegate 
        // the call to a Node.js event handler. 
        // Depending on the EventArgs, the data may need to be transformed 
        // if it cannot be directly marshaled by Edge.js.

        timer.Elapsed += (Object source, System.Timers.ElapsedEventArgs e) => {
            ((Func<object,Task<object>>)input.event_handler)(e).Start();
        };

        // Start the timer
        
        timer.Enabled = true;

        // Return a function that can be used by Node.js to 
        // unsubscribe from the event source.

        return (Func<object,Task<object>>)(async (dynamic data) => {
            timer.Enabled = false;
            return null;
        });
    }
*/});

subscribe({
    interval: 2000,
    event_handler: function (data, cb) {
        console.log('Received event', data);
        cb();
    } 
}, function (error, unsubscribe) {
    if (error) throw error;
    console.log('Subscribed to .NET events. Unsubscribing in 7 seconds...');
    setTimeout(function () {
        unsubscribe(null, function (error) {
            if (error) throw error;
            console.log('Unsubscribed from .NET events.');
            console.log('Waiting 5 seconds before exit to show that no more events are generated...')
            setTimeout(function () {}, 5000);
        });
    }, 7000);
});
