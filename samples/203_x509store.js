// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var listCertificates = edge.func(function() {/*
    #r "System.dll"

    using System.Collections.Generic;
    using System.Security.Cryptography.X509Certificates;

    async (dynamic data) =>
    {
        X509Store store = new X509Store(
            (string)data.storeName, 
            (StoreLocation)Enum.Parse(typeof(StoreLocation), (string)data.storeLocation));
        store.Open(OpenFlags.ReadOnly);
        try
        {
            List<string> result = new List<string>();
            foreach (X509Certificate2 certificate in store.Certificates)
            {
                result.Add(certificate.Subject);
            }

            return result;
        }
        finally
        {
            store.Close();
        }
    }
*/});

var result = listCertificates({ storeName: 'My', storeLocation: 'LocalMachine' }, true);
console.log(result);
