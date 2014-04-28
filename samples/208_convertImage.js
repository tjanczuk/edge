// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge')
    , path = require('path');

var convertImage = edge.func(function() {/*
    #r "System.Drawing.dll"

    using System;
    using System.Threading.Tasks;
    using System.Collections.Generic;
    using System.Drawing;
    using System.Drawing.Imaging;

    class Startup
    {
        static IDictionary<string, ImageFormat> formats = new Dictionary<string, ImageFormat> 
        {
            { "jpg", ImageFormat.Jpeg },
            { "bmp", ImageFormat.Bmp },
            { "gif", ImageFormat.Gif },
            { "tiff", ImageFormat.Tiff },
            { "png", ImageFormat.Png }
        };

        public async Task<object> Invoke(IDictionary<string,object> input)
        {
            await Task.Run(async () => {
                using (Image image = Image.FromFile((string)input["source"]))
                {
                    image.Save((string)input["destination"], formats[(string)input["toType"]]);
                }
            });

            return null;
        }
    }
*/});

var params = { 
    source: path.join(__dirname, 'edge.png'), 
    destination: path.join(__dirname, 'edge.jpg'),
    toType: 'jpg'
};

convertImage(params, function (error) {
    if (error) throw error;
    console.log('The edge.png has been asynchronously converted to edge.jpg.');
});
