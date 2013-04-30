// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var unzipDirectory = edge.func(function() {/*
    #r "System.IO.Compression.FileSystem.dll"

    using System.Collections.Generic;
    using System.IO.Compression;

    async (data) =>
    {
        var input = (IDictionary<string,object>)data;
        await Task.Run(async () => {
            ZipFile.ExtractToDirectory((string)input["source"], (string)input["destination"]);
        });

        return null;
    }
*/});

var params = { 
    source: '..\\samples.zip', 
    destination: '..\\samples_tmp' 
};

unzipDirectory(params, function (error) {
    if (error) throw error;
    console.log('The ..\\samples.zip archive has been decompressed to ..\\samples_tmp directory.');
});
