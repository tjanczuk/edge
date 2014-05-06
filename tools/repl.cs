using System;
using System.IO;

class Program
{
    public static void Main(string[] args)
    {
        if (args.Length != 3) {
            throw new InvalidOperationException("Usage: repl.exe <file> <from_str> <to_str>");
        }

        Console.WriteLine("Replacing " + args[1] + " with " + args[2] + " in " + args[0] + "...");
        var s = File.ReadAllText(args[0]);
        File.WriteAllText(args[0], s.Replace(args[1], args[2]));
        Console.WriteLine("Done.");
    }
}