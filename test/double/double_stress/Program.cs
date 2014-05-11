using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using EdgeJs;

namespace double_stress
{
    class Program
    {
        static string data = "{'data_package_id':'341f0a51-3521-400e-ba05-b86939433e9b','timestamp':1375988508262,'records':[{'id':'df63580b-5047-4ed2-a5f3-e928abd8414a','timestamp':1375988508118,'context':[],'target_users':[],'extension':{'eventRecordTime':1375988508118,'apiVersion':'v1','shortUrl':'shortUrl','httpMethod':'POST','callerUserAgent':'Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.110 Safari/537.36','azureDeployName':'localhost','connectionId':'3f813fd4-aea9-4e7b-a44c-e4a3f75da38b'},'sampling_data':{},'context_ids':{'connectionId':'1bb5c8d4-acc2-4a01-aa7e-79317eedd5d5'},'event_type':'edf_trouter_request_check','type':'trouter'}],'ids':{'deployment':'TrouterTest'},'schema':1,'type':'Service','source':'Node Test','version':'1.0.0.0'}";

        static Func<object, Task<object>> callToNode = Edge.Func(@"
            return function (data, cb) {
                cb(null, data);
            }
        ");

        static Func<object, Task<object>> callToNodeThatThrows = Edge.Func(@"
            return function (data, cb) {
                throw new Error('Some error');
            }
        ");

        static Func<object, Task<object>> callToNodeThatReturnsError = Edge.Func(@"
            return function (data, cb) {
                cb(new Error('Some error'), data);
            }
        ");

        static Func<object, Task<object>> callToNodeThatReturnsFunction = Edge.Func(@"
            return function (data, cb) {
                cb(null, function (data, cb) {});
            }
        ");

        static Func<object, Task<object>> callToNodeCallBack = Edge.Func(@"
            return function (func, cb) {
                var data = ""{'data_package_id':'341f0a51-3521-400e-ba05-b86939433e9b','timestamp':1375988508262,'records':[{'id':'df63580b-5047-4ed2-a5f3-e928abd8414a','timestamp':1375988508118,'context':[],'target_users':[],'extension':{'eventRecordTime':1375988508118,'apiVersion':'v1','shortUrl':'shortUrl','httpMethod':'POST','callerUserAgent':'Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.110 Safari/537.36','azureDeployName':'localhost','connectionId':'3f813fd4-aea9-4e7b-a44c-e4a3f75da38b'},'sampling_data':{},'context_ids':{'connectionId':'1bb5c8d4-acc2-4a01-aa7e-79317eedd5d5'},'event_type':'edf_trouter_request_check','type':'trouter'}],'ids':{'deployment':'TrouterTest'},'schema':1,'type':'Service','source':'Node Test','version':'1.0.0.0'}"";

                function one() {
                    func(data, function (error, data) {
                        if (error) throw error;
                        setTimeout(one, 1);
                    });
                }

                one();
            }
        ");

        static Func<object, Task<object>> callToNodeCallBackWithException = Edge.Func(@"
            return function (func, cb) {
                var data = ""{'data_package_id':'341f0a51-3521-400e-ba05-b86939433e9b','timestamp':1375988508262,'records':[{'id':'df63580b-5047-4ed2-a5f3-e928abd8414a','timestamp':1375988508118,'context':[],'target_users':[],'extension':{'eventRecordTime':1375988508118,'apiVersion':'v1','shortUrl':'shortUrl','httpMethod':'POST','callerUserAgent':'Mozilla/5.0 (Windows NT 6.1; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/27.0.1453.110 Safari/537.36','azureDeployName':'localhost','connectionId':'3f813fd4-aea9-4e7b-a44c-e4a3f75da38b'},'sampling_data':{},'context_ids':{'connectionId':'1bb5c8d4-acc2-4a01-aa7e-79317eedd5d5'},'event_type':'edf_trouter_request_check','type':'trouter'}],'ids':{'deployment':'TrouterTest'},'schema':1,'type':'Service','source':'Node Test','version':'1.0.0.0'}"";

                function one() {
                    func(data, function (error, data) {
                        if (!error) throw new Error('Unexpected success');
                        setTimeout(one, 1);
                    });
                }

                one();
            }
        ");

        static void Main(string[] args)
        {
            // Calls to Node.js and passes a chunk of data
            new Thread(() =>
            {
                Console.WriteLine("Calls to Node.js and passes a chunk of data...");
                while (true)
                {
                    var r = callToNode(data).Result;
                }
            }).Start();

            // Calls to Node.js that returns a function
            new Thread(() =>
            {
                Console.WriteLine("Calls to Node.js that returns a function...");
                while (true)
                {
                    var r = callToNodeThatReturnsFunction(data).Result;
                }
            }).Start();

            // Calls to Node.js and passes a chunk of data, Node.js throws
            new Thread(() =>
            {
                Console.WriteLine("Calls to Node.js and passes a chunk of data, Node.js throws...");
                while (true)
                {
                    try
                    {
                        var k = callToNodeThatThrows(data).Result;
                        throw new Exception("Unexpected success");
                    }
                    catch (Exception) { }
                }
            }).Start();

            // Calls to Node.js and passes a chunk of data, Node.js returns error
            new Thread(() =>
            {
                Console.WriteLine("Calls to Node.js and passes a chunk of data, Node.js returns error...");
                while (true)
                {
                    try
                    {
                        var k = callToNodeThatReturnsError(data).Result;
                        throw new Exception("Unexpected success");
                    }
                    catch (Exception) { }
                }
            }).Start();

            // Calls to Node.js that calls back to .NET in a loop passing a chunk of data
            new Thread(() =>
            {
                Console.WriteLine("Calls to Node.js that calls back to .NET in a loop passing a chunk of data...");
                var r = callToNodeCallBack((Func<object,Task<object>>)(async (input) => { 
                    return input; 
                })).Result;
            }).Start();

            // Calls to Node.js that calls back to .NET in a loop, where .NET throws an exception
            new Thread(() =>
            {
                Console.WriteLine("Calls to Node.js that calls back to .NET in a loop, where .NET throws an exception...");
                var r = callToNodeCallBackWithException((Func<object, Task<object>>)(async (input) => { 
                    throw new Exception("Error"); 
                })).Result;
            }).Start();
        }
    }
}
