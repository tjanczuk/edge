using System;
using System.Collections.Generic;
using System.Dynamic;
using System.Text;
using System.Threading.Tasks;

#pragma warning disable 1998

namespace Edge.Performance
{
    public class Startup
    {
        public async Task<object> Invoke(dynamic input)
        {
            var book = new 
            {
                title = "Run .NET and node.js in-process with edge.js",
                author = new {
                    first = "Tomasz",
                    last = "Janczuk"
                },
                year = 2013,
                price = 24.99,
                available = true, 
                description = "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Vivamus posuere tincidunt felis, et mattis mauris ultrices quis. Cras molestie, quam varius tincidunt tincidunt, mi magna imperdiet lacus, quis elementum ante nibh quis orci. In posuere erat sed tellus lacinia luctus. Praesent sodales tellus mauris, et egestas justo. In blandit, metus non congue adipiscing, est orci luctus odio, non sagittis erat orci ac sapien. Proin ut est id enim mattis volutpat. Vivamus ultrices dapibus feugiat. In dictum tincidunt eros, non pretium nisi rhoncus in. Duis a lacus et elit feugiat ullamcorper. Mauris tempor turpis nulla. Nullam nec facilisis elit.",
                picture = new byte[16000],
                tags = new object[] { ".NET", "node.js", "CLR", "V8", "interop" }
            };
            
            return book;
        }
    }
}
