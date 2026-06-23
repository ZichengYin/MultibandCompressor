using System;
using System.Linq;
using System.Reflection;

internal static class AssemblyTypeList
{
    private static int Main(string[] args)
    {
        var assembly = Assembly.LoadFrom(args[0]);

        foreach (var type in assembly.GetTypes().OrderBy(t => t.FullName))
            Console.WriteLine(type.FullName);

        return 0;
    }
}
