// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge')
    , path = require('path');

var zipDirectory = edge.func(function() {/*
    #r "System.IO.Compression.FileSystem.dll"

    using System.IO.Compression;

    async (dynamic input) =>
    {
        await Task.Run(async () => {
            ZipFile.CreateFromDirectory((string)input.source, (string)input.destination);
        });

        return null;
    }
*/});

var params = { 
    source: path.join(__dirname, '..', 'samples'), 
    destination: path.join(__dirname, '..', 'samples.zip') 
};

zipDirectory(params, function (error) {
    if (error) throw error;
    console.log('The samples directory has been compressed to ..\\samples.zip file.');
});
