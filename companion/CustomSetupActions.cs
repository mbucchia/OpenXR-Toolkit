﻿using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration;
using System.IO;
using System.Windows.Forms;

// Reference: https://www.c-sharpcorner.com/article/how-to-perform-custom-actions-and-upgrade-using-visual-studio-installer/
namespace SetupCustomActions
{
    [RunInstaller(true)]
    public partial class CustomActions : System.Configuration.Install.Installer
    {
        public CustomActions()
        {
        }

        protected override void OnAfterInstall(IDictionary savedState)
        {
            var installPath = Path.GetDirectoryName(base.Context.Parameters["AssemblyPath"]);
            var jsonName = "XR_APILAYER_NOVENDOR_toolkit.json";
            var jsonPath = installPath + "\\" + jsonName;

            // We want to add our layer at the very beginning, so that any other layer like the Ultraleap layer is following us.
            // We delete all entries, create our own, and recreate all entries.

            Microsoft.Win32.RegistryKey key;
            key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey("SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit");
            var existingValues = key.GetValueNames();
            var entriesValues = new Dictionary<string, object>();
            foreach (var value in existingValues)
            {
                entriesValues.Add(value, key.GetValue(value));
                key.DeleteValue(value);
            }

            bool detectedOldSoftware = false;
            key.SetValue(jsonPath, 0);
            foreach (var value in existingValues)
            {
                // Do not re-create keys for previous versions of our layer.
                if (value.EndsWith("\\XR_APILAYER_NOVENDOR_nis_scaler.json") ||
                    value.EndsWith("\\XR_APILAYER_NOVENDOR_hand_to_controller.json"))
                {
                    detectedOldSoftware = true;
                    continue;
                }

                // Do not re-create our own key. We did it before this loop.
                if (value.EndsWith("\\" + jsonName))
                {
                    continue;
                }

                key.SetValue(value, entriesValues[value]);
            }

            key.Close();

            if (detectedOldSoftware)
            {
                MessageBox.Show("An older version of this software was detected (OpenXR-NIS-Scaler or OpenXR-Hand-To-Controller). " +
                    "It was deactivated, however please uninstall it through 'Add or remove programs' to free up disk space.",
                    "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
            }

            base.OnAfterInstall(savedState);
        }
    }
}
