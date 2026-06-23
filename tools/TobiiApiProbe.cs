using System;
using System.Linq;
using System.Reflection;

internal static class TobiiApiProbe
{
    private static int Main(string[] args)
    {
        if (args.Length != 1)
        {
            Console.Error.WriteLine("Usage: TobiiApiProbe <Tobii.StreamEngine.Net.dll>");
            return 1;
        }

        try
        {
            var assembly = Assembly.LoadFrom(args[0]);

            foreach (var type in assembly.GetTypes()
                                         .Where(t => IsDetailedTarget(t.FullName))
                                         .OrderBy(t => t.FullName))
            {
                Console.WriteLine("TYPE " + type.FullName);

                foreach (var constructor in type.GetConstructors(BindingFlags.Public
                                                                 | BindingFlags.NonPublic
                                                                 | BindingFlags.Instance
                                                                 | BindingFlags.DeclaredOnly))
                {
                    Console.WriteLine("  CONSTRUCTOR " + constructor);
                }

                foreach (var method in type.GetMethods(BindingFlags.Public
                                                       | BindingFlags.NonPublic
                                                       | BindingFlags.Static
                                                       | BindingFlags.Instance
                                                       | BindingFlags.DeclaredOnly))
                {
                    Console.WriteLine("  METHOD " + method);
                }

                foreach (var field in type.GetFields(BindingFlags.Public
                                                     | BindingFlags.NonPublic
                                                     | BindingFlags.Static
                                                     | BindingFlags.Instance
                                                     | BindingFlags.DeclaredOnly))
                {
                    Console.WriteLine("  FIELD " + field);
                }

                foreach (var property in type.GetProperties(BindingFlags.Public
                                                            | BindingFlags.NonPublic
                                                            | BindingFlags.Static
                                                            | BindingFlags.Instance
                                                            | BindingFlags.DeclaredOnly))
                {
                    Console.WriteLine("  PROPERTY " + property);
                }

                foreach (var eventInfo in type.GetEvents(BindingFlags.Public
                                                         | BindingFlags.NonPublic
                                                         | BindingFlags.Static
                                                         | BindingFlags.Instance
                                                         | BindingFlags.DeclaredOnly))
                {
                    Console.WriteLine("  EVENT " + eventInfo);
                }
            }

            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 2;
        }
    }

    private static bool ContainsRelevantTerm(string value)
    {
        if (string.IsNullOrEmpty(value))
            return false;

        var text = value.ToLowerInvariant();
        return text.Contains("gaze")
            || text.Contains("stream")
            || text.Contains("device")
            || text.Contains("api");
    }

    private static bool IsDetailedTarget(string value)
    {
        return value == "Tobii.StreamEngine.EyeTracker"
            || value == "Tobii.StreamEngine.EyeTrackerFactory"
            || value == "Tobii.StreamEngine.IEyeTracker"
            || value == "Tobii.StreamEngine.tobii_gaze_point_t"
            || value == "Tobii.StreamEngine.tobii_gaze_point_callback_t"
            || value == "Tobii.StreamEngine.tobii_gaze_data_t"
            || value == "Tobii.StreamEngine.tobii_gaze_data_eye_t"
            || value == "Tobii.StreamEngine.tobii_gaze_data_callback_t"
            || value == "Tobii.StreamEngine.tobii_validity_t"
            || value == "Tobii.StreamEngine.TobiiVector2";
    }
}
