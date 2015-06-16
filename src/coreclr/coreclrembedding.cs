// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;
using System.Security;

[SecurityCritical]
public class CoreCLREmbedding
{
    [SecurityCritical]
    public static void LoadFunction(string assemblyPath, string typeName, string methodName)
    {
        Console.WriteLine("Loading " + typeName + "." + methodName + "() from " + assemblyPath);
    }
}
