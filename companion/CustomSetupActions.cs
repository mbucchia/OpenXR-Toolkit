using System;
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
            // Check for layers from the previous versions of this project.
            Microsoft.Win32.RegistryKey key;
            key = Microsoft.Win32.Registry.LocalMachine.CreateSubKey("SOFTWARE\\Khronos\\OpenXR\\1\\ApiLayers\\Implicit");
            var existingValues = key.GetValueNames();
            foreach (var value in existingValues)
            {
                if (value.EndsWith("\\XR_APILAYER_NOVENDOR_nis_scaler.json"))
                {
                    MessageBox.Show("An older version of this software was detected (OpenXR-NIS-Scaler). Please uninstall it through 'Add or remove programs'.", "Warning", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
            }

            base.OnAfterInstall(savedState);
        }
    }
}
