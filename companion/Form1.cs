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

namespace companion
{
    public partial class Form1 : Form
    {
        // Must match config.cpp.
        private const string RegPrefix = "SOFTWARE\\OpenXR_Toolkit";

        public Form1()
        {
            InitializeComponent();

            InitXr();

            Microsoft.Win32.RegistryKey key = null;
            try
            {
                key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey(RegPrefix);
                safemodeCheckbox.Checked = (int)key.GetValue("safe_mode", 0) == 1 ? true : false;
                experimentalCheckbox.Checked = (int)key.GetValue("enable_experimental", 0) == 1 ? true : false;
                screenshotCheckbox.Checked = (int)key.GetValue("enable_screenshot", 0) == 1 ? true : false;
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

            loading = false;
        }

        private bool loading = true;

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
                    safemodeCheckbox.Enabled = !disableCheckbox.Checked;
                    experimentalCheckbox.Enabled = !disableCheckbox.Checked;
                    screenshotCheckbox.Enabled = !disableCheckbox.Checked;
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
                    key.DeleteValue(jsonPath);
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
    }
}
