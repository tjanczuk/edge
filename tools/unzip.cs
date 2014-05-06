using System;
using System.IO.Compression;

class Program
{
    public static void Main(string[] args)
    {
        if (args.Length != 2) {
            throw new InvalidOperationException("Usage: unzip.exe <zipfile> <dirname>");
        }

        Console.WriteLine("Unziping " + args[0] + " to " + args[1] + "...");
        ZipFile.ExtractToDirectory(args[0], args[1]);
        Console.WriteLine("Done.");
    }
}