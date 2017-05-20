using System;
using Microsoft.VisualStudio.TestTools.UnitTesting;
using EdgeJs;
using System.Threading.Tasks;
using System.Dynamic;

namespace double_test
{
    [TestClass]
    public class DoubleEdge
    {
        [TestMethod]
        public void FailsWithMissingParameter()
        {
            try
            {
                Edge.Func(null);
                Assert.Fail("Unexpected success.");
            }
            catch (Exception e)
            {
                Assert.IsTrue(GetInnermostException(e).Message.IndexOf("Node.js code") != -1, "Unexpected exception.");
            }
        }

        [TestMethod]
        public void FailsWithMalformedJavaScript()
        {
            try
            {
                Edge.Func("foobar");
                Assert.Fail("Unexpected success.");
            }
            catch (Exception e)
            {
                Assert.IsTrue(GetInnermostException(e).Message.IndexOf("foobar is not defined") != -1, "Unexpected exception.");
            }
        }

        [TestMethod]
        public void SucceedsForHelloWorld()
        {
            var result = Edge.Func(@"
                return function (data, cb) {
                    cb(null, 'Node.js welcomes ' + data);
                }
            ")(".NET").Result;

            Assert.AreEqual(result, "Node.js welcomes .NET");
        }

        [TestMethod]
        public void SucceedsCheckingNodeVersion()
        {
            var result = Edge.Func(@"
                return function (data, cb) {
                    cb(null, process.version);
                }
            ")(".NET").Result;

            Assert.AreEqual(result, "v7.10.0");
        }

        [TestMethod]
        public void SuccessfulyMarshalsDataFromNetToNode()
        {
            var result = Edge.Func(@"
                var assert = require('assert');

                return function (data, cb) {
			        assert.equal(typeof data, 'object');
			        assert.ok(data.a === 1);
			        assert.ok(data.b === 3.1415);
			        assert.ok(data.c === 'foo');
			        assert.ok(data.d === true);
			        assert.ok(data.e === false);
			        assert.equal(typeof data.f, 'object');
			        assert.ok(Buffer.isBuffer(data.f));
			        assert.equal(data.f.length, 10);
			        assert.ok(Array.isArray(data.g));
			        assert.equal(data.g.length, 2);
			        assert.ok(data.g[0] === 1);
			        assert.ok(data.g[1] === 'foo');
			        assert.equal(typeof data.h, 'object');
			        assert.ok(data.h.a === 'foo');
			        assert.ok(data.h.b === 12);
			        assert.equal(typeof data.i, 'function');
			        assert.equal(typeof data.j, 'object');
			        assert.ok(data.j.valueOf() === Date.UTC(2013, 07, 30));
                    cb(null, 'ok');
                }
            ")(new { 
                 a = 1,
                 b = 3.1415,
                 c = "foo",
                 d = true,
                 e = false,
                 f = new byte[10],
                 g = new object[] { 1, "foo" },
                 h = new { a = "foo", b = 12 },
                 i = (Func<object,Task<object>>)(async (i) => { return i; }),
                 j = new DateTime(2013, 08, 30)
             }).Result;

            Assert.AreEqual(result, "ok");
        }

        [TestMethod]
        public void SuccessfulyMarshalsDataFromNodeToNet()
        {
            dynamic result = (dynamic)Edge.Func(@"
                return function (data, cb) {
                    cb(null, {
			            a: 1,
			            b: 3.1415,
			            c: 'foo',
			            d: true,
			            e: false,
			            f: new Buffer(10),
			            g: [ 1, 'foo' ],
			            h: { a: 'foo', b: 12 },
			            i: function (payload, callback) { },
			            j: new Date(Date.UTC(2013, 07, 30))
		            });
                }
            ")(null).Result;

            Assert.AreEqual(result.a, 1);
            Assert.AreEqual(result.b, 3.1415);
            Assert.AreEqual(result.c, "foo");
            Assert.AreEqual(result.d, true);
            Assert.AreEqual(result.e, false);
            Assert.AreEqual(result.f.GetType(), typeof(byte[]));
            Assert.AreEqual(((byte[])result.f).Length, 10);
            Assert.AreEqual(result.g.GetType(), typeof(object[]));
            Assert.AreEqual(((object[])result.g)[0], 1);
            Assert.AreEqual(((object[])result.g)[1], "foo");
            Assert.AreEqual(result.h.GetType(), typeof(ExpandoObject));
            Assert.AreEqual(((dynamic)result.h).a, "foo");
            Assert.AreEqual(((dynamic)result.h).b, 12);
            Assert.AreEqual(result.i.GetType(), typeof(Func<object,Task<object>>));
            Assert.AreEqual(result.j, new DateTime(2013, 08, 30));
        }

        [TestMethod]
        public void SuccessfulyMarshalsNodeException()
        {
            try
            {
                var result = Edge.Func(@"
                    return function (data, cb) {
                        throw new Error('Sample error');
                    }
                ")(null).Result;
                Assert.Fail("Unexpected success.");
            }
            catch (Exception e)
            {
                Assert.IsTrue(GetInnermostException(e).Message.IndexOf("Sample error") != -1, "Unexpected exception.");
            }
        }

        [TestMethod]
        public void SuccessfulyMarshalsNodeError()
        {
            try
            {
                var result = Edge.Func(@"
                    return function (data, cb) {
                        cb(new Error('Sample error'));
                    }
                ")(null).Result;
                Assert.Fail("Unexpected success.");
            }
            catch (Exception e)
            {
                Assert.IsTrue(GetInnermostException(e).Message.IndexOf("Sample error") != -1, "Unexpected exception.");
            }
        }

        [TestMethod]
        public void SuccessfulyMarshalsEmptyBuffer()
        {
            var result = Edge.Func(@"
                var assert = require('assert');

                return function (data, cb) {
                    assert.ok(Buffer.isBuffer(data));
                    assert.equal(data.length, 0);
                    cb(null, new Buffer(0));
                }
            ")(new byte[] {}).Result;

            Assert.AreEqual(result.GetType(), typeof(byte[]));
            Assert.AreEqual(((byte[])result).Length, 0);
        }

        [TestMethod]
        public void SuccessfulyRoundtripsUnicodeCharacters()
        {
            var data = "ñòóôõöøùúûüýÿ";
            var result = Edge.Func(@"
                return function (data, cb) {
                    cb(null, data);
                }
            ")(data).Result;

            Assert.AreEqual(result, data);
        }

        [TestMethod]
        public void SuccessfulyRoundtripsEmptyString()
        {
            var data = "";
            var result = Edge.Func(@"
                return function (data, cb) {
                    cb(null, data);
                }
            ")(data).Result;

            Assert.AreEqual(result, data);
        }

        [TestMethod]
        public void SuccessfulyCallsSyncNetFunctionFromNodeAsynchronously()
        {
            var result = Edge.Func(@"
                return function (data, cb) {
                    data(null, cb);
                }
            ")((Func<object, Task<object>>)(async (input) =>
             {
                 return "ok";
             })).Result;

            Assert.AreEqual(result, "ok");
        }

        [TestMethod]
        public void SuccessfulyCallsSyncNetFunctionFromNodeSynchronously()
        {
            var result = Edge.Func(@"
                return function (data, cb) {
                    cb(null, data(null, true));
                }
            ")((Func<object, Task<object>>)(async (input) =>
             {
                 return "ok";
             })).Result;

            Assert.AreEqual(result, "ok");
        }

        [TestMethod]
        public void FailsCallingAsyncNetFunctionFromNodeSynchronously()
        {
            try
            {
                var result = Edge.Func(@"
                    return function (data, cb) {
                        cb(null, data(null, true));
                    }
                ")((Func<object, Task<object>>)(async (input) =>
                {
                    await Task.Delay(1000);
                    return "ok";
                })).Result;
                Assert.Fail("Unexpected success.");
            }
            catch (Exception e)
            {
                Assert.IsTrue(GetInnermostException(e).Message.IndexOf("The JavaScript function was called synchronously but") != -1, "Unexpected exception.");
            }
        }

        [TestMethod]
        public void SuccessfulyCallsAsyncNetFunctionFromNode()
        {
            var result = Edge.Func(@"
                return function (data, cb) {
                    data(null, cb);
                }
            ")((Func<object,Task<object>>)(async (input) =>
             {
                 await Task.Delay(1);
                 return "ok";
             })).Result;

            Assert.AreEqual(result, "ok");
        }

        [TestMethod]
        public void CallExportedNodeClosureOverJavaScriptState()
        {
            var func = Edge.Func(@"
                var k = 0;
                return function (data, cb) {
                    cb(null, ++k);
                }
            ");

            Assert.AreEqual(func(null).Result, 1);
            Assert.AreEqual(func(null).Result, 2);
        }

        Exception GetInnermostException(Exception e)
        {
            var result = e;
            while (e != null)
            {
                if (e.InnerException != null)
                {
                    result = e.InnerException;
                }

                e = e.InnerException;
            }

            return result;
        }

    }
}
