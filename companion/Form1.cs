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
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;
using Silk.NET.Core;
using Silk.NET.Core.Native;
using Silk.NET.OpenXR;
using System.Reflection;
using System.Diagnostics;
using System.Windows.Input;
using System.Runtime.InteropServices;

namespace companion
{
    public partial class Form1 : Form
    {
        [DllImport("XR_APILAYER_NOVENDOR_toolkit.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.StdCall)]
        public static extern IntPtr getVersionString();

        // Must match config.cpp.
        public const string RegPrefix = "SOFTWARE\\OpenXR_Toolkit";

        private List<Tuple<string, int>> VirtualKeys;

        private bool loading = true;
        private bool tracing = false;

        private int keyMenuGen = 1;

        public Form1()
        {
            InitializeComponent();

            InitializeKeyList(leftKey);
            InitializeKeyList(nextKey);
            InitializeKeyList(previousKey);
            InitializeKeyList(rightKey);
            InitializeKeyList(screenshotKey);

            InitXr();
            timer1_Tick(null, null);

            SuspendLayout();
            Microsoft.Win32.RegistryKey key = null;
            try
            {
                key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(RegPrefix);

                // Must match the defaults in the layer!
                keyMenuGen = (int)key.GetValue("key_menu_gen", 1);
                safemodeCheckbox.Checked = (int)key.GetValue("safe_mode", 0) == 1 ? true : false;
                screenshotCheckbox.Checked = (int)key.GetValue("enable_screenshot", 0) == 1 ? true : false;
                screenshotFormat.SelectedIndex = (int)key.GetValue("screenshot_fileformat", 1);
                screenshotFormat.Enabled = screenshotCheckbox.Enabled && screenshotCheckbox.Checked;
                screenshotEye.SelectedIndex = (int)key.GetValue("screenshot_eye", 0);
                menuVisibility.SelectedIndex = (int)key.GetValue("menu_eye", 0);
                ctrlModifierCheckbox.Checked = (int)key.GetValue("ctrl_modifier", 1) == 1 ? true : false;
                altModifierCheckbox.Checked = (int)key.GetValue("alt_modifier", 0) == 1 ? true : false;
                SelectKey(leftKey, (int)key.GetValue("key_left", KeyInterop.VirtualKeyFromKey(Key.F1)));
                SelectKey(nextKey, (int)key.GetValue("key_menu", KeyInterop.VirtualKeyFromKey(Key.F2)));
                SelectKey(previousKey, (int)key.GetValue("key_up", 0));
                SelectKey(rightKey, (int)key.GetValue("key_right", KeyInterop.VirtualKeyFromKey(Key.F3)));
                SelectKey(screenshotKey, (int)key.GetValue("key_screenshot", KeyInterop.VirtualKeyFromKey(Key.F12)));
                key.Close();

                // Bypass apps.
                key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(RegPrefix);
                foreach (var subKey in key.GetSubKeyNames())
                {
                    var app = key.OpenSubKey(subKey);
                    var modulePath = (string)app.GetValue("module", null);
                    var displayName = subKey;
                    if (modulePath != null)
                    {
                        displayName += " (" + Path.GetFileName(modulePath) + ")";
                    }

                    appList.Items.Add(displayName);
                    appList.SetItemChecked(appList.Items.Count - 1, (int)app.GetValue("bypass", 0) == 0);
                }
            }
            catch (Exception)
            {
                MessageBox.Show(this, "Failed to write to registry. Please make sure the app is running elevated.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
            }
            ResumeLayout();

            loading = false;
        }

        private void InitializeKeyList(ComboBox box)
        {
            if (VirtualKeys == null)
            {
                VirtualKeys = new();
                Key[] allowed = new[] {
                    Key.Escape, Key.F1, Key.F2, Key.F3, Key.F4, Key.F5, Key.F6, Key.F7, Key.F8, Key.F9, Key.F10, Key.F11, Key.F12, Key.PrintScreen, Key.Scroll, Key.Pause,
                    Key.OemTilde, Key.D1, Key.D2, Key.D3, Key.D4, Key.D5, Key.D6, Key.D7, Key.D8, Key.D9, Key.D0, Key.OemMinus, Key.OemPlus, Key.Back, Key.Insert, Key.Home, Key.PageUp,
                    Key.Tab, Key.Q, Key.W, Key.E, Key.R, Key.T, Key.Y, Key.U, Key.I, Key.O, Key.P, Key.OemOpenBrackets, Key.OemCloseBrackets, Key.OemPipe, Key.Delete, Key.End, Key.PageDown,
                    Key.A, Key.S, Key.D, Key.F, Key.G, Key.H, Key.J, Key.K, Key.L, Key.OemSemicolon, Key.OemQuotes, Key.Enter,
                    Key.Z, Key.X, Key.C, Key.V, Key.B, Key.N, Key.M, Key.OemComma, Key.OemPeriod, Key.Separator,
                    Key.Space, Key.Left, Key.Up, Key.Down, Key.Right,
                    Key.NumPad0, Key.NumPad1, Key.NumPad2, Key.NumPad3, Key.NumPad4, Key.NumPad5, Key.NumPad6, Key.NumPad7, Key.NumPad8, Key.NumPad9,
                    Key.Divide, Key.Multiply, Key.Subtract, Key.Add
                };

                foreach (var key in allowed)
                {
                    var text = key switch
                    {
                        Key.Add => "NumPad+",
                        Key.Back => "Backspace",
                        Key.D0 => "0",
                        Key.D1 => "1",
                        Key.D2 => "2",
                        Key.D3 => "3",
                        Key.D4 => "4",
                        Key.D5 => "5",
                        Key.D6 => "6",
                        Key.D7 => "7",
                        Key.D8 => "8",
                        Key.D9 => "9",
                        Key.Divide => "NumPad/",
                        Key.Multiply => "NumPad*",
                        Key.OemBackslash => "\\",
                        Key.OemCloseBrackets => "]",
                        Key.OemComma => ",",
                        Key.OemMinus => "-",
                        Key.OemOpenBrackets => "[",
                        Key.OemPeriod => ".",
                        Key.OemPipe => "|",
                        Key.OemPlus => "+",
                        Key.OemQuestion => "?",
                        Key.OemQuotes => "\"",
                        Key.OemSemicolon => ";",
                        Key.OemTilde => "~",
                        Key.Scroll => "ScrLk",
                        Key.Separator => "/",
                        Key.Snapshot => "PrntScrn",
                        Key.Subtract => "NumPad-",
                        _ => key.ToString()
                    };
                    VirtualKeys.Add(new(text, KeyInterop.VirtualKeyFromKey(key)));
                }
            }

            box.Items.Add("");
            foreach (var entry in VirtualKeys)
            {
                box.Items.Add(entry.Item1);
            }
        }

        private void SelectKey(ComboBox box, int virtualKey)
        {
            if (virtualKey == 0)
            {
                box.SelectedIndex = 0;
            }

            string keyText = "";
            foreach (var key in VirtualKeys)
            {
                if (key.Item2 == virtualKey)
                {
                    keyText = key.Item1;
                    break;
                }
            }

            foreach (var item in box.Items)
            {
                if ((string)item == keyText)
                {
                    box.SelectedItem = item;
                    break;
                }
            }
        }

        private void SetActiveString()
        {
            if (versionString != null)
            {
                layerActive.Text = versionString + " is active";
            }
            else
            {
                layerActive.Text = "OpenXR Toolkit layer is active";
            }

            if (appString != "")
            {
                layerActive.Text += " (" + appString + ")";
            }
        }

        string versionString = null;

        private unsafe void InitXr()
        {
            try
            {
                IntPtr pStr = getVersionString();
                versionString = Marshal.PtrToStringAnsi(pStr);
            }
            catch (Exception)
            {
                MessageBox.Show(this, "Failed to query version", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }

            AppDomain dom = AppDomain.CreateDomain("temporaryXr");
            try
            {
                // Load the OpenXR package into a temporary app domain. This is so make sure that the registry is read everytime when looking for implicit API layer.
                AssemblyName assemblyName = new AssemblyName();
                assemblyName.CodeBase = typeof(XR).Assembly.Location;
                Assembly assembly = dom.Load(assemblyName);
                Type localXR = assembly.GetType("Silk.NET.OpenXR.XR");

                XR xr = (XR)localXR.GetMethod("GetApi").Invoke(null, null);

                // Make sure our layer is installed.
                uint layerCount = 0;
                xr.EnumerateApiLayerProperties(ref layerCount, null);
                var layers = new ApiLayerProperties[layerCount];
                for (int i = 0; i < layers.Length; i++)
                {
                    layers[i].Type = StructureType.TypeApiLayerProperties;
                }
                var layersSpan = new Span<ApiLayerProperties>(layers);
                if (xr.EnumerateApiLayerProperties(ref layerCount, layersSpan) == Result.Success)
                {
                    bool found = false;
                    string layersList = "";
                    for (int i = 0; i < layers.Length; i++)
                    {
                        fixed (void* nptr = layers[i].LayerName)
                        {
                            string layerName = SilkMarshal.PtrToString(new System.IntPtr(nptr));
                            layersList += layerName + "\n";
                            if (layerName == "XR_APILAYER_NOVENDOR_toolkit")
                            {
                                found = true;
                            }
                        }
                    }

                    tooltip.SetToolTip(layerActive, layersList);

                    bool wasLoading = loading;
                    if (!found)
                    {
                        layerActive.Text = "OpenXR Toolkit layer is NOT active";
                        layerActive.ForeColor = Color.Red;
                        loading = true;
                        disableCheckbox.Checked = true;
                        loading = wasLoading;
                    }
                    else
                    {
                        SetActiveString();
                        layerActive.ForeColor = Color.Green;
                        loading = true;
                        disableCheckbox.Checked = false;
                        loading = wasLoading;
                    }
                    safemodeCheckbox.Enabled = screenshotCheckbox.Enabled = screenshotFormat.Enabled = screenshotEye.Enabled =
                        menuVisibility.Enabled = leftKey.Enabled = nextKey.Enabled = previousKey.Enabled = rightKey.Enabled = screenshotKey.Enabled =
                        ctrlModifierCheckbox.Enabled = altModifierCheckbox.Enabled = !disableCheckbox.Checked;
                    screenshotFormat.Enabled = screenshotCheckbox.Enabled && screenshotCheckbox.Checked;
                }
                else
                {
                    MessageBox.Show(this, "Failed to query API layers", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }
            }
            catch (Exception)
            {
                MessageBox.Show(this, "Failed to initialize OpenXR", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                AppDomain.Unload(dom);
            }

            // Try to reclaim memory.
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
        }

        private void WriteSetting(string name, int value)
        {
            Microsoft.Win32.RegistryKey key = null;
            try
            {
                key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(RegPrefix);
                key.SetValue(name, value, Microsoft.Win32.RegistryValueKind.DWord);
            }
            catch (Exception)
            {
                MessageBox.Show(this, "Failed to write to registry. Please make sure the app is running elevated.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
            }
        }

        private void reportIssuesLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            string githubIssues = "https://github.com/mbucchia/OpenXR-Toolkit/issues?q=is%3Aissue+is%3Aopen+label%3Abug";

            reportIssuesLink.LinkVisited = true;
            System.Diagnostics.Process.Start(githubIssues);
        }

        private void checkUpdatesLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            string homepage = "https://mbucchia.github.io/OpenXR-Toolkit";

            checkUpdatesLink.LinkVisited = true;
            System.Diagnostics.Process.Start(homepage);
        }

        private void disableCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }

            var assembly = Assembly.GetAssembly(GetType());
            var installPath = Path.GetDirectoryName(assembly.Location);
            var jsonName = "XR_APILAYER_NOVENDOR_toolkit.json";
            var jsonPath = installPath + "\\" + jsonName;

            Microsoft.Win32.RegistryKey key = null;
            Microsoft.Win32.RegistryKey wmrKey = null;
            try
            {
                key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey("SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit");

                if (disableCheckbox.Checked)
                {
                    key.SetValue(jsonPath, 1);

                    // Always cleanup the global WMR options we might have set.
                    try
                    {
                        wmrKey = Microsoft.Win32.Registry.CurrentUser.CreateSubKey("SOFTWARE\\Microsoft\\OpenXR");
                        wmrKey.DeleteValue("MotionVectorEnabled");
                        wmrKey.DeleteValue("MinimumFrameInterval");
                        wmrKey.DeleteValue("MaximumFrameInterval");
                    }
                    catch (Exception)
                    {
                    }
                }
                else
                {
                    key.SetValue(jsonPath, 0);
                }
            }
            catch (Exception)
            {
                MessageBox.Show(this, "Failed to write to registry. Please make sure the app is running elevated.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
                if (wmrKey != null)
                {
                    wmrKey.Close();
                }
            }

            InitXr();
        }

        private void safemodeCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }

            WriteSetting("safe_mode", safemodeCheckbox.Checked ? 1 : 0);
        }

        private void sceenshotCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("enable_screenshot", screenshotCheckbox.Checked ? 1 : 0);
            screenshotFormat.Enabled = screenshotCheckbox.Checked;
        }

        private void menuVisibility_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("menu_eye", menuVisibility.SelectedIndex);
        }

        private void leftKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            AssignKey(leftKey, "key_left", new ComboBox[] { previousKey, nextKey, rightKey, screenshotKey });
        }

        private void nextKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            AssignKey(nextKey, "key_menu", new ComboBox[] { leftKey, previousKey, rightKey, screenshotKey });
        }

        private void previousKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            AssignKey(previousKey, "key_up", new ComboBox[] { leftKey, nextKey, rightKey, screenshotKey });
        }

        private void rightKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            AssignKey(rightKey, "key_right", new ComboBox[] { leftKey, previousKey, nextKey, screenshotKey });
        }

        private void screenshotKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            AssignKey(screenshotKey, "key_screenshot", new ComboBox[] { leftKey, previousKey, nextKey, rightKey });
        }

        private void AssignKey(ComboBox key, string setting, ComboBox[] otherKeys)
        {
            if (loading)
            {
                return;
            }

            if (key.SelectedIndex > 0)
            {
                foreach (var other in otherKeys)
                {
                    if (key.SelectedItem == other.SelectedItem)
                    {
                        MessageBox.Show("Please make the key assignments unique.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }

                }

                foreach (var k in VirtualKeys)
                {
                    if (k.Item1 == (string)key.SelectedItem)
                    {
                        WriteSetting(setting, k.Item2);

                        // Force the splash to appear again after changing the menu key.
                        if (setting == "key_menu")
                        {
                            WriteSetting("key_menu_gen", ++keyMenuGen);
                        }

                        break;
                    }
                }
            }
            else
            {
                WriteSetting(setting, 0);
            }
        }



        private void ctrlModifierCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("ctrl_modifier", ctrlModifierCheckbox.Checked ? 1 : 0);
        }

        private void altModifierCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("alt_modifier", altModifierCheckbox.Checked ? 1 : 0);
        }

        private void openLog_Click(object sender, EventArgs e)
        {
            var processInfo = new ProcessStartInfo();
            processInfo.Verb = "Open";
            processInfo.UseShellExecute = true;
            processInfo.FileName = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\OpenXR-Toolkit\\logs\\XR_APILAYER_NOVENDOR_toolkit.log";
            try
            {
                Process.Start(processInfo);
            }
            catch (Win32Exception)
            {
                MessageBox.Show("Failed to open the log file. Please check attempt to locate '" + processInfo.FileName + "' manually.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        private void openScreenshots_Click(object sender, EventArgs e)
        {
            var processInfo = new ProcessStartInfo();
            processInfo.Verb = "Open";
            processInfo.UseShellExecute = true;
            processInfo.FileName = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\OpenXR-Toolkit\\screenshots";
            try
            {
                Process.Start(processInfo);
            }
            catch (Win32Exception)
            {
            }
        }

        private void pictureBox1_Click(object sender, EventArgs e)
        {
            string homepage = "https://mbucchia.github.io/OpenXR-Toolkit";

            checkUpdatesLink.LinkVisited = true;
            System.Diagnostics.Process.Start(homepage);
        }

        private void licences_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            var assembly = Assembly.GetAssembly(GetType());
            var installPath = Path.GetDirectoryName(assembly.Location);

            var processInfo = new ProcessStartInfo();
            processInfo.Verb = "Open";
            processInfo.FileName = "notepad";
            processInfo.Arguments = installPath + "\\THIRD_PARTY";
            try
            {
                Process.Start(processInfo);
                licences.LinkVisited = true;
            }
            catch (Win32Exception)
            {
            }
        }

        private void screenshotFormat_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("screenshot_fileformat", screenshotFormat.SelectedIndex);
        }

        private void screenshotEye_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("screenshot_eye", screenshotEye.SelectedIndex);
        }

        private void traceButton_Click(object sender, EventArgs e)
        {
            if (!tracing)
            {
                if (MessageBox.Show(this, "PRIVACY WARNING: The trace file generated by this tool may include the following personal information \"Name of the computer\", \"Windows account name\". By continuing, you consent to have this information collected and potentially exposed online (if you share the resulting file online)." +
                        "\n\nNOTE: Do not use this functionality unless instructed by the developers and you understand what you are doing.\n\nDo you wish to continue?", "Confirmation", MessageBoxButtons.YesNo, MessageBoxIcon.Warning) == DialogResult.No)
                {
                    return;
                }

                var captureForWMR = MessageBox.Show(this, "Do you want to capture additional Windows Mixed Reality traces?", "Windows Mixed Reality tracing", MessageBoxButtons.YesNo, MessageBoxIcon.Question) == DialogResult.Yes;

                // Cancel any pending traces.
                var processInfo = new ProcessStartInfo();
                processInfo.Verb = "Open";
                processInfo.FileName = "wpr";
                processInfo.Arguments = "-cancel";
                processInfo.CreateNoWindow = true;
                try
                {
                    Process.Start(processInfo);
                }
                catch (Win32Exception)
                {
                }

                var assembly = Assembly.GetAssembly(GetType());
                var installPath = Path.GetDirectoryName(assembly.Location);

                // Start a new trace.
                processInfo = new ProcessStartInfo();
                processInfo.Verb = "Open";
                processInfo.FileName = "wpr";
                processInfo.Arguments = "-start \"" + installPath + (captureForWMR ? "\\OXRTK_WMR.wprp" : "\\OXRTK.wprp") + "\" -filemode";
                processInfo.CreateNoWindow = true;
                processInfo.RedirectStandardOutput = true;
                processInfo.RedirectStandardError = true;
                processInfo.UseShellExecute = false;
                try
                {
                    var process = Process.Start(processInfo);
                    process.WaitForExit();

                    var output = process.StandardOutput.ReadToEnd();
                    var error = process.StandardError.ReadToEnd();
                    if (output != "" || error != "")
                    {
                        MessageBox.Show(this, "Standard output:\n" + output + "\nStandard error:\n" + error, "Outcome", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                catch (Win32Exception)
                {
                    MessageBox.Show(this, "Failed to start tracing. Please make sure the app is running elevated.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                traceButton.Text = "Stop capture";
                tracing = true;

                MessageBox.Show(this, "Do not close the Companion app until after you have stopped the capture.", "Confirmation", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
            else
            {
                var processInfo = new ProcessStartInfo();
                processInfo.Verb = "Open";
                processInfo.FileName = "wpr";
                processInfo.Arguments = "-stop \"" + Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\OpenXR-Toolkit\\logs\\OXRTK.etl\"";
                processInfo.CreateNoWindow = true;
                processInfo.RedirectStandardOutput = true;
                processInfo.RedirectStandardError = true;
                processInfo.UseShellExecute = false;
                try
                {
                    var process = Process.Start(processInfo);
                    process.WaitForExit();

                    var output = process.StandardOutput.ReadToEnd();
                    var error = process.StandardError.ReadToEnd();
                    if (output != "" || error != "")
                    {
                        MessageBox.Show(this, "Standard output:\n" + output + "\nStandard error:\n" + error, "Outcome", MessageBoxButtons.OK, MessageBoxIcon.Error);
                        return;
                    }
                }
                catch (Win32Exception)
                {
                    MessageBox.Show(this, "Failed to save trace. Please make sure the app is running elevated.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
                    return;
                }

                traceButton.Text = "Capture trace";
                tracing = false;

                // Open the output folder with our file.
                processInfo = new ProcessStartInfo();
                processInfo.Verb = "Open";
                processInfo.UseShellExecute = true;
                processInfo.FileName = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\OpenXR-Toolkit\\logs";
                try
                {
                    Process.Start(processInfo);
                }
                catch (Win32Exception)
                {
                }
            }
        }

        private void appList_ItemCheck(object sender, ItemCheckEventArgs e)
        {
            if (loading)
            {
                return;
            }
            var app = appList.Items[e.Index].ToString().Split('(')[0].Trim();
            Microsoft.Win32.RegistryKey key = null;
            try
            {
                key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(RegPrefix + "\\" + app);
                key.SetValue("bypass", e.NewValue == CheckState.Checked ? 0 : 1, Microsoft.Win32.RegistryValueKind.DWord);
            }
            catch (Exception)
            {
                MessageBox.Show(this, "Failed to write to registry.", "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
            }

        }

        string appString = null;

        private void timer1_Tick(object sender, EventArgs e)
        {
            Microsoft.Win32.RegistryKey key = null;
            try
            {
                key = Microsoft.Win32.Registry.CurrentUser.CreateSubKey(RegPrefix);
                appString = (string)key.GetValue("running", "");
                SetActiveString();
            }
            catch (Exception)
            {
            }
            finally
            {
                if (key != null)
                {
                    key.Close();
                }
            }
        }
    }
}
