using System;
using System.Linq;
using System.Reflection;

internal static class EyeXApiProbe
{
    private static int Main(string[] args)
    {
        foreach (var path in args)
        {
            var assembly = Assembly.LoadFrom(path);

            foreach (var type in assembly.GetTypes()
                                         .Where(t => IsRelevant(t.FullName))
                                         .OrderBy(t => t.FullName))
            {
                Console.WriteLine("TYPE " + type.FullName);

                foreach (var constructor in type.GetConstructors())
                    Console.WriteLine("  CONSTRUCTOR " + constructor);

                foreach (var method in type.GetMethods(BindingFlags.Public
                                                       | BindingFlags.Static
                                                       | BindingFlags.Instance
                                                       | BindingFlags.DeclaredOnly))
                    Console.WriteLine("  METHOD " + method);

                foreach (var property in type.GetProperties())
                    Console.WriteLine("  PROPERTY " + property);

                foreach (var field in type.GetFields(BindingFlags.Public
                                                     | BindingFlags.NonPublic
                                                     | BindingFlags.Static
                                                     | BindingFlags.Instance
                                                     | BindingFlags.DeclaredOnly))
                    Console.WriteLine("  FIELD " + field);

                foreach (var eventInfo in type.GetEvents())
                    Console.WriteLine("  EVENT " + eventInfo);
            }
        }

        return 0;
    }

    private static bool IsRelevant(string name)
    {
        if (string.IsNullOrEmpty(name))
            return false;

        var text = name.ToLowerInvariant();
        return text.Contains("eyexhost")
            || text.Contains("gazepoint")
            || text.Contains("datastream")
            || text.Contains("context")
            || text.Contains("engine")
            || text.Contains("snapshot")
            || text.Contains("interactor")
            || text.Contains("behavior")
            || text.Contains("interactionevent")
            || text.EndsWith(".environment");
    }
}
