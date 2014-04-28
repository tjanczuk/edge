// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge')
    , path = require('path');

var unzipDirectory = edge.func(function() {/*
    #r "System.IO.Compression.FileSystem.dll"

    using System.IO.Compression;

    async (dynamic input) =>
    {
        await Task.Run(async () => {
            ZipFile.ExtractToDirectory((string)input.source, (string)input.destination);
        });

        return null;
    }
*/});

var params = { 
    source: path.join(__dirname, '..', 'samples.zip'), 
    destination: path.join(__dirname, '..', 'samples_tmp')
};

unzipDirectory(params, function (error) {
    if (error) throw error;
    console.log('The ..\\samples.zip archive has been decompressed to ..\\samples_tmp directory.');
});
