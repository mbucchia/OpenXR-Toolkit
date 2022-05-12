// MIT License
//
// Copyright(c) 2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Security.Principal;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace companion
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main(string[] args)
        {
            if (args.Length == 0)
            {
#if !DEBUG
                var principal = new WindowsPrincipal(WindowsIdentity.GetCurrent());
                if (!principal.IsInRole(WindowsBuiltInRole.Administrator))
                {
                    var processInfo = new System.Diagnostics.ProcessStartInfo();
                    processInfo.Verb = "RunAs";
                    processInfo.FileName = Assembly.GetEntryAssembly().Location;
                    try
                    {
                        Process.Start(processInfo).WaitForExit();
                    }
                    catch (Win32Exception)
                    {
                        MessageBox.Show("This application must be run as Administrator.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                    Application.Exit();
                    return;
                }
#endif
                Application.EnableVisualStyles();
                Application.SetCompatibleTextRenderingDefault(false);
                Application.Run(new Form1());
            }
            else
            {
                AttachConsole(ATTACH_PARENT_PROCESS);

                try
                {
                    // Identify the app to write to.
                    string app = null;
                    int i = 0;
                    if (args[0] == "app")
                    {
                        if (args.Length < 2)
                        {
                            throw new Exception("Must specify an application name");
                        }
                        app = args[1];
                        i += 2;
                    }
                    else
                    {
                        key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(Form1.RegPrefix);
                        app = (string)key.GetValue("running", "");
                        key.Close();
                    }
                    if (app == "")
                    {
                        throw new Exception("No application specified or currently running");
                    }

                    key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(Form1.RegPrefix + "\\" + app);

                    // Dump current values.
                    if (i + 1 == args.Length && args[i] == "dump")
                    {
                        Console.Write(Assembly.GetEntryAssembly().Location);
                        Console.Write(" app " + app);
                        foreach (var p in parser)
                        {
                            if (p.DumpRegkey != null)
                            {
                                Console.Write(" -" + p.Arg + " " + p.DumpRegkey(p, (int)key.GetValue(p.Regkey, p.Default)));
                            }
                        }
                        Console.WriteLine();
                    }
                    else
                    {
                        // Parse the settings.
                        for (; i < args.Length; i++)
                        {
                            string arg = args[i];
                            string name = null;
                            int value = -1;

                            foreach (var p in parser)
                            {
                                if (p.Arg == arg.Substring(1))
                                {
                                    if (i + 1 >= args.Length)
                                    {
                                        throw new Exception("Must specify a value for argument " + arg);
                                    }

                                    name = p.Regkey;
                                    value = p.ParseArg(args[++i].Trim().ToLower(), p);
                                    break;
                                }
                            }

                            if (name == null)
                            {
                                throw new Exception("Invalid argument: " + arg);
                            }
                            key.SetValue(name, value);
                        }
                    }
                }
                catch (Exception e)
                {
                    if (key != null)
                    {
                        key.Close();
                    }
                    Console.WriteLine("Error: " + e.Message);
                    Usage();
                }
                Console.WriteLine("Done");
            }
        }

        private static Microsoft.Win32.RegistryKey key = null;

        private static ArgParser[] parser = new ArgParser[]
        {
            new ArgParser("sunglasses", "post_sunglasses", parseSunglassesMode, dumpSunglassesMode, 0, 0, 3),
            new ArgParser("post-process", "post_process", parseSettingValue, dumpSettingValue, 0, 0, 1),
            new ArgParser("contrast", "post_contrast", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("brightness", "post_brightness", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("exposure", "post_exposure", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("saturation", "post_saturation", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("vibrance", "post_vibrance", parseSettingValue, dumpSettingValue, 0, 0, 1000, 10),
            new ArgParser("highlights", "post_highlights", parseSettingValue, dumpSettingValue, 1000, 0, 1000, 10),
            new ArgParser("shadows", "post_shadows", parseSettingValue, dumpSettingValue, 0, 0, 1000, 10),
            new ArgParser("gain-r", "post_gain_r", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("gain-g", "post_gain_g", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("gain-b", "post_gain_b", parseSettingValue, dumpSettingValue, 500, 0, 1000, 10),
            new ArgParser("world-scale", "world_scale", parseSettingValue, dumpSettingValue, 1000, 1, 10000, 10),
            new ArgParser("zoom", "zoom", parseSettingValue, dumpSettingValue, 10, 10, 1500, 10),
            new ArgParser("reprojection-rate", "motion_reprojection_rate", parseMotionReprojectionRate, dumpMotionReprojectionRate, 0, 0, 3),
            new ArgParser("foveated-rendering", "vrs", parseToggle),
            new ArgParser("overlay", "overlay", parseToggle),
        };

        private static int parseSettingValue(string value, ArgParser arg)
        {
            // Special cases for bool.
            if (arg.Min == 0 && arg.Max == 1 && arg.Scale == 1)
            {
                if (value == "on" || value == "true")
                {
                    return 1;
                }
                if (value == "off" || value == "false")
                {
                    return 0;
                }
                if (value == "toggle")
                {
                    return (int)key.GetValue(arg.Regkey, arg.Default) ^ 1;
                }
            }

            // Detect absolute or relative.
            bool increaseBy = value[0] == '+';
            value = value.Substring(increaseBy ? 1 : 0);
            int v = arg.Scale == 1 ? int.Parse(value) : (int)(float.Parse(value) * arg.Scale + float.Epsilon);
            if (increaseBy)
            {
                v += (int)key.GetValue(arg.Regkey, arg.Default);
            }

            // Clamp.
            return Math.Min(Math.Max(v, arg.Min), arg.Max);
        }

        private static string dumpSettingValue(ArgParser arg, int value)
        {
            return (arg.Scale == 1 ? value : value / (float)arg.Scale).ToString();
        }

        private static int parseSunglassesMode(string value, ArgParser arg)
        {
            // Support "cycling" through.
            if (value[0] == '+')
            {
                return (int.Parse(value.Substring(1)) + (int)key.GetValue(arg.Regkey, arg.Default)) % (arg.Max + 1);
            }

            return value switch
            {
                "off" => 0,
                "light" => 1,
                "dark" => 2,
                "trunite" => 3,
                _ => throw new Exception("Unsupported value: " + value),
            };
        }

        private static string dumpSunglassesMode(ArgParser arg, int value)
        {
            return value switch
            {
                0 => "off",
                1 => "light",
                2 => "dark",
                3 => "trunite",
                _ => "invalid",
            };
        }

        private static int parseMotionReprojectionRate(string value, ArgParser arg)
        {
            // Support "cycling" through.
            if (value[0] == '+')
            {
                return (int.Parse(value.Substring(1)) + (int)key.GetValue(arg.Regkey, arg.Default)) % (arg.Max + 1);
            }

            return value switch
            {
                "unlocked" => 1,
                "1/2" => 2,
                "1/3" => 3,
                "1/4" => 4,
                _ => throw new Exception("Unsupported value: " + value),
            };
        }

        private static string dumpMotionReprojectionRate(ArgParser arg, int value)
        {
            return value switch
            {
                1 => "unlocked",
                2 => "1/2",
                3 => "1/3",
                4 => "1/4",
                _ => "invalid",
            };
        }

        private static int parseToggle(string value, ArgParser arg)
        {
            if (value != "toggle")
            {
                throw new Exception("Unsupported value: " + value);
            }

            // When toggling, we save/restore the correct value.
            int current = (int)key.GetValue(arg.Regkey, arg.Default);
            int restore = (int)key.GetValue(arg.Regkey + "_bak", current ^ 1);
            key.SetValue(arg.Regkey + "_bak", current);
            return restore;
        }

        private class ArgParser
        {
            public ArgParser(string arg, string regkey, Func<string, ArgParser, int> parseArg, Func<ArgParser, int, string> dumpRegkey = null, int def = 0, int min = 0, int max = 0, int scale = 1)
            {
                Arg = arg;
                Regkey = regkey;
                ParseArg = parseArg;
                DumpRegkey = dumpRegkey;
                Default = def;
                Min = min;
                Max = max;
                Scale = scale;
            }

            public string Arg { get; set; }
            public string Regkey { get; set; }
            public Func<string, ArgParser, int> ParseArg { get; set; }
            public Func<ArgParser, int, string> DumpRegkey { get; set; }
            public int Default { get; set; }
            public int Min { get; set; }
            public int Max { get; set; }
            public int Scale { get; set; }
        }

        private static void Usage()
        {
            Console.WriteLine("Usage: " + Assembly.GetEntryAssembly().Location);
            Console.WriteLine("    [app <name>]");
            foreach (var p in parser)
            {
                string values = "<value>";
                if (p.DumpRegkey == dumpSettingValue)
                {
                    values = "<[" + (p.Min / p.Scale) + "," + (p.Max / p.Scale) + "]>";
                }
                else if (p.DumpRegkey == null)
                {
                    values = "toggle";
                }
                else
                {
                    values = "<";
                    for (int i = p.Min; i <= p.Max; i++)
                    {
                        values += p.DumpRegkey(p, i);
                        if (i < p.Max)
                        {
                            values += "|";
                        }
                    }
                    values += ">";
                }
                Console.WriteLine("    [-" + p.Arg + " " + values + "]");
            }
            Console.WriteLine();
            Console.WriteLine("When no app is specified, the currently running app is used.");
            Console.WriteLine("Use syntax <+N> to add value N to integral and decimal values (N can be negative)");
            Console.WriteLine("Use syntax <+N> to cycle by step N through enumeration values (with automatic wraparound)");
            Console.WriteLine();
            Console.WriteLine("Examples:");
            Console.WriteLine(" companion.exe -brightness 50.5 -contrast 45.8");
            Console.WriteLine("   Set brightness and contrast values for the currently running app");
            Console.WriteLine(" companion.exe -sunglasses +1");
            Console.WriteLine("   Cycle through sunglasses mode for the currently running app");
            Console.WriteLine(" companion.exe -world-scale +-10");
            Console.WriteLine("   Decrease world scale by 10% for the currently running app");
            Console.WriteLine(" companion.exe -overlay toggle");
            Console.WriteLine("   Toggle the overlay on/off for the currently running app");
            Console.WriteLine(" companion.exe app FS2020 dump");
            Console.WriteLine("   Dump settings for app 'FS2020' (Flight Simulator 2020)");
            Environment.Exit(1);
        }

        // https://stackoverflow.com/questions/4362111/how-do-i-show-a-console-output-window-in-a-forms-application
        // Attach the parent console to us.
        [System.Runtime.InteropServices.DllImport("kernel32.dll")]
        private static extern bool AttachConsole(int dwProcessId);

        private const int ATTACH_PARENT_PROCESS = -1;
    }
}
