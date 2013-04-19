// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var zipDirectory = edge.func(function() {/*
    //#r "System.IO.Compression.FileSystem.dll"

    using System;
    using System.Threading.Tasks;
    using System.Collections.Generic;
    using System.IO.Compression;

    class Startup
    {
        public async Task<object> Invoke(IDictionary<string,object> input)
        {
            await Task.Run(async () => {
                ZipFile.CreateFromDirectory((string)input["source"], (string)input["destination"]);
            });

            return null;
        }
    }
*/});

var params = { 
    source: '..\\samples', 
    destination: '..\\samples.zip' 
};

zipDirectory(params, function (error) {
    if (error) throw error;
    console.log('The samples directory has been compressed to ..\\samples.zip file.');
});
