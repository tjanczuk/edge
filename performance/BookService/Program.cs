using System;
using System.Net.Http;
using System.Net.Http.Headers;
using System.ServiceModel;
using System.Web.Http;
using System.Web.Http.SelfHost;

namespace BookService
{
    public class Book
    {
        public string title = "Run .NET and node.js in-process with edge.js";
        public object author = new { first = "Tomasz", last = "Janczuk" };
        public int year = 2013;
        public double price = 24.99;
        public bool available = true;
        public string description = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus posuere tincidunt felis, et mattis mauris ultrices quis. Cras molestie, quam varius tincidunt tincidunt, mi magna imperdiet lacus, quis elementum ante nibh quis orci. In posuere erat sed tellus lacinia luctus. Praesent sodales tellus mauris, et egestas justo. In blandit, metus non congue adipiscing, est orci luctus odio, non sagittis erat orci ac sapien. Proin ut est id enim mattis volutpat. Vivamus ultrices dapibus feugiat. In dictum tincidunt eros, non pretium nisi rhoncus in. Duis a lacus et elit feugiat ullamcorper. Mauris tempor turpis nulla. Nullam nec facilisis elit.";
        public byte[] picture = new byte[16000];
        public object[] tags = new object[] { ".NET", "node.js", "CLR", "V8", "interop" };
    }

    public class BookController : ApiController
    {
        static readonly Uri baseAddress = new Uri("http://localhost:31415/");

        public Book Get()
        {
            return new Book();
        }

        static void Main(string[] args)
        {
            HttpSelfHostServer server = null;
            try
            {
                HttpSelfHostConfiguration config = new HttpSelfHostConfiguration(baseAddress);
                config.HostNameComparisonMode = HostNameComparisonMode.Exact;

                // Register default route
                config.Routes.MapHttpRoute(
                    name: "DefaultApi",
                    routeTemplate: "api/{controller}"
                );

                server = new HttpSelfHostServer(config);
                server.OpenAsync().Wait();
                Console.WriteLine("Listening on " + baseAddress);
                Console.WriteLine("Hit ENTER to exit...");
                Console.ReadLine();
            }
            catch (Exception e)
            {
                Console.WriteLine("Could not start server: {0}", e.GetBaseException().Message);
                Console.WriteLine("Hit ENTER to exit...");
                Console.ReadLine();
            }
            finally
            {
                if (server != null)
                {
                    server.CloseAsync().Wait();
                }
            }
        }
    }
}