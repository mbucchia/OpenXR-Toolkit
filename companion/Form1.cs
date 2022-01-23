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

namespace companion
{
    public partial class Form1 : Form
    {
        // Must match config.cpp.
        private const string RegPrefix = "SOFTWARE\\OpenXR_Toolkit";

        private List<Tuple<string, int>> VirtualKeys;

        public Form1()
        {
            InitializeComponent();

            InitializeKeyList(leftKey);
            InitializeKeyList(nextKey);
            InitializeKeyList(rightKey);

            InitXr();

            SuspendLayout();
            Microsoft.Win32.RegistryKey key = null;
            try
            {
                key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(RegPrefix);
                // Must match the defaults in the layer!
                safemodeCheckbox.Checked = (int)key.GetValue("safe_mode", 0) == 1 ? true : false;
                experimentalCheckbox.Checked = (int)key.GetValue("enable_experimental", 0) == 1 ? true : false;
                screenshotCheckbox.Checked = (int)key.GetValue("enable_screenshot", 0) == 1 ? true : false;
                ctrlModifierCheckbox.Checked = (int)key.GetValue("ctrl_modifier", 1) == 1 ? true : false;
                altModifierCheckbox.Checked = (int)key.GetValue("alt_modifier", 0) == 1 ? true : false;
                SelectKey(leftKey, (int)key.GetValue("key_left", KeyInterop.VirtualKeyFromKey(Key.F1)));
                SelectKey(nextKey, (int)key.GetValue("key_menu", KeyInterop.VirtualKeyFromKey(Key.F2)));
                SelectKey(rightKey, (int)key.GetValue("key_right", KeyInterop.VirtualKeyFromKey(Key.F3)));
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

        private bool loading = true;

        private void InitializeKeyList(ComboBox box)
        {
            if (VirtualKeys == null)
            {
                VirtualKeys = new();
                Key[] allowed = new[] {
                    Key.A, Key.Add,
                    Key.B, Key.Back,
                    Key.C,
                    Key.D, Key.D0, Key.D1, Key.D2, Key.D3, Key.D4, Key.D5, Key.D6, Key.D7, Key.D8, Key.D9, Key.Delete, Key.Divide, Key.Down,
                    Key.E, Key.End, Key.Enter, Key.Escape,
                    Key.F, Key.F1, Key.F2, Key.F3, Key.F4, Key.F5, Key.F6, Key.F7, Key.F8, Key.F9, Key.F10, Key.F11,
                    Key.G,
                    Key.H, Key.Home,
                    Key.I, Key.Insert,
                    Key.J,
                    Key.K,
                    Key.L, Key.Left,
                    Key.M, Key.Multiply,
                    Key.N, Key.NumPad0, Key.NumPad1, Key.NumPad2, Key.NumPad3, Key.NumPad4, Key.NumPad5, Key.NumPad6, Key.NumPad7, Key.NumPad8, Key.NumPad9,
                    Key.O,
                    Key.P, Key.PageDown, Key.PageUp, Key.Pause, Key.PrintScreen,
                    Key.Q,
                    Key.R, Key.Right,
                    Key.S, Key.Scroll, Key.Separator, Key.Space, Key.Subtract,
                    Key.T, Key.Tab,
                    Key.U, Key.Up,
                    Key.V,
                    Key.W,
                    Key.Y,
                    Key.Z };

                foreach (var key in allowed)
                {
                    VirtualKeys.Add(new(key.ToString(), KeyInterop.VirtualKeyFromKey(key)));
                }
            }

            foreach (var entry in VirtualKeys)
            {
                box.Items.Add(entry.Item1);
            }
        }

        private void SelectKey(ComboBox box, int virtualKey)
        {
            string keyText = "";
            foreach (var key in VirtualKeys)
            {
                if (key.Item2 == virtualKey)
                {
                    keyText = key.Item1;
                    break;
                }
            }

            foreach(var item in box.Items)
            {
                if ((string)item == keyText)
                {
                    box.SelectedItem = item;
                    break;
                }
            }
        }

        private unsafe void InitXr()
        {
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

                    if (!found)
                    {
                        layerActive.Text = "OpenXR Toolkit layer is NOT active";
                        layerActive.ForeColor = Color.Red;
                        loading = true;
                        disableCheckbox.Checked = true;
                        loading = false;
                    }
                    else
                    {
                        layerActive.Text = "OpenXR Toolkit layer is active";
                        layerActive.ForeColor = Color.Green;
                        loading = true;
                        disableCheckbox.Checked = false;
                        loading = false;
                    }
                    safemodeCheckbox.Enabled = experimentalCheckbox.Enabled = screenshotCheckbox.Enabled = leftKey.Enabled = nextKey.Enabled = rightKey.Enabled =
                        ctrlModifierCheckbox.Enabled = altModifierCheckbox.Enabled = !disableCheckbox.Checked;
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
            string githubIssues = "https://github.com/mbucchia/OpenXR-Toolkit/issues";

            reportIssuesLink.LinkVisited = true;
            System.Diagnostics.Process.Start(githubIssues);
        }

        private void checkUpdatesLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            string githubReleases = "https://mbucchia.github.io/OpenXR-Toolkit";

            checkUpdatesLink.LinkVisited = true;
            System.Diagnostics.Process.Start(githubReleases);
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
            try
            {
                key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey("SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit");

                if (disableCheckbox.Checked)
                {
                    key.SetValue(jsonPath, 1);
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

        private void experimentalCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("enable_experimental", experimentalCheckbox.Checked ? 1 : 0);
        }

        private void sceenshotCheckbox_CheckedChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            WriteSetting("enable_screenshot", screenshotCheckbox.Checked ? 1 : 0);
        }

        private void leftKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            foreach (var key in VirtualKeys)
            {
                if (key.Item1 == (string)leftKey.SelectedItem)
                {
                    WriteSetting("key_left", key.Item2);
                    break;
                }
            }
        }

        private void nextKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            foreach (var key in VirtualKeys)
            {
                if (key.Item1 == (string)nextKey.SelectedItem)
                {
                    WriteSetting("key_menu", key.Item2);
                    break;
                }
            }
        }

        private void rightKey_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (loading)
            {
                return;
            }
            foreach (var key in VirtualKeys)
            {
                if (key.Item1 == (string)rightKey.SelectedItem)
                {
                    WriteSetting("key_right", key.Item2);
                    break;
                }
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
            Process.Start(processInfo);
        }

        private void openScreenshots_Click(object sender, EventArgs e)
        {
            var processInfo = new ProcessStartInfo();
            processInfo.Verb = "Open";
            processInfo.UseShellExecute = true;
            processInfo.FileName = Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData) + "\\OpenXR-Toolkit\\screenshots";
            Process.Start(processInfo);
        }
    }
}
