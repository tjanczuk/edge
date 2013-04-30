// Overview of edge.js: http://tjanczuk.github.com/edge

var edge = require('../lib/edge');

var listCertificates = edge.func(function() {/*
    #r "System.dll"

    using System.Collections.Generic;
    using System.Security.Cryptography.X509Certificates;

    async (data) =>
    {
        var input = (IDictionary<string,object>)data;
        X509Store store = new X509Store(
            (string)input["storeName"], 
            (StoreLocation)Enum.Parse(typeof(StoreLocation), (string)input["storeLocation"]));
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

var result = listCertificates({ storeName: 'My', storeLocation: 'CurrentUser' }, true);
console.log(result);
