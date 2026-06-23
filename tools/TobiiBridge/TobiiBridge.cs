using System;
using System.Globalization;
using System.Linq;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using Tobii.EyeX.Client;
using Tobii.Interaction.Client;
using Tobii.Interaction.Framework;

internal static class TobiiBridge
{
    private const int Port = 4242;
    private static readonly UdpClient Udp = new UdpClient();
    private static readonly IPEndPoint Destination = new IPEndPoint(IPAddress.Loopback, Port);
    private static readonly ManualResetEvent ExitEvent = new ManualResetEvent(false);

    private static Context context;
    private static object eyeXEnvironment;
    private static int eventHandlerTicket;

    private static int Main()
    {
        Console.CancelKeyPress += OnCancelKeyPress;

        try
        {
            var environmentType = typeof(Context).Assembly.GetType("Tobii.EyeX.Client.Environment");
            eyeXEnvironment = environmentType.GetMethod("Initialize", Type.EmptyTypes).Invoke(null, null);
            context = new Context(false);
            context.Name = "Multiband Compressor Eye Control";
            context.ConnectionStateChanged += OnConnectionStateChanged;
            eventHandlerTicket = context.RegisterEventHandler(OnInteractionEvent);
            context.EnableConnection();

            Interactor interactor;
            var snapshot = context.CreateWithGlobalInteractor(
                "MultibandCompressor.GazePointData",
                out interactor);

            var gazeParams = new GazePointDataParams
            {
                GazePointDataMode = GazePointDataMode.LightlyFiltered
            };

            interactor.CreateGazePointDataBehavior(ref gazeParams);
            snapshot.CommitAsync(null);

            Console.WriteLine("Tobii EyeX bridge started.");
            Console.WriteLine("Sending gaze X/Y to UDP 127.0.0.1:" + Port);
            Console.WriteLine("Press Ctrl+C to stop.");

            ExitEvent.WaitOne();

            return 0;
        }
        catch (Exception exception)
        {
            Console.Error.WriteLine(exception);
            return 1;
        }
        finally
        {
            if (context != null)
            {
                context.DisableConnection();
                try
                {
                    context.Shutdown(1000, false);
                }
                catch
                {
                    // The runtime may report remaining global interactors during shutdown.
                }

                context.Dispose();
            }

            if (eyeXEnvironment != null)
                eyeXEnvironment.GetType().GetMethod("Dispose").Invoke(eyeXEnvironment, null);

            Udp.Close();
        }
    }

    private static void OnConnectionStateChanged(object sender, ConnectionStateChangedEventArgs eventArgs)
    {
        Console.WriteLine("Tobii connection state: " + eventArgs.State);
    }

    private static void OnInteractionEvent(InteractionEvent interactionEvent)
    {
        foreach (var behavior in interactionEvent.Behaviors)
        {
            GazePointDataEventParams gazePoint;

            if (!behavior.TryGetGazePointDataEventParams(out gazePoint))
                continue;

            var message = string.Format(
                CultureInfo.InvariantCulture,
                "GAZE,{0:F6},{1:F6},1",
                gazePoint.X,
                gazePoint.Y);

            var bytes = Encoding.ASCII.GetBytes(message);
            Udp.Send(bytes, bytes.Length, Destination);
        }
    }

    private static void OnCancelKeyPress(object sender, ConsoleCancelEventArgs eventArgs)
    {
        eventArgs.Cancel = true;
        ExitEvent.Set();
    }
}
