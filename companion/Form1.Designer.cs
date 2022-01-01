
namespace companion
{
    partial class Form1
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.components = new System.ComponentModel.Container();
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(Form1));
            this.layerActive = new System.Windows.Forms.Label();
            this.tooltip = new System.Windows.Forms.ToolTip(this.components);
            this.reportIssuesLink = new System.Windows.Forms.LinkLabel();
            this.screenshotCheckbox = new System.Windows.Forms.CheckBox();
            this.checkUpdatesLink = new System.Windows.Forms.LinkLabel();
            this.disableCheckbox = new System.Windows.Forms.CheckBox();
            this.safemodeCheckbox = new System.Windows.Forms.CheckBox();
            this.label1 = new System.Windows.Forms.Label();
            this.experimentalCheckbox = new System.Windows.Forms.CheckBox();
            this.label2 = new System.Windows.Forms.Label();
            this.label3 = new System.Windows.Forms.Label();
            this.label4 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // layerActive
            // 
            this.layerActive.AutoSize = true;
            this.layerActive.Location = new System.Drawing.Point(34, 27);
            this.layerActive.Name = "layerActive";
            this.layerActive.Size = new System.Drawing.Size(188, 20);
            this.layerActive.TabIndex = 10;
            this.layerActive.Text = "Layer status is not known";
            // 
            // reportIssuesLink
            // 
            this.reportIssuesLink.AutoSize = true;
            this.reportIssuesLink.Location = new System.Drawing.Point(12, 426);
            this.reportIssuesLink.Name = "reportIssuesLink";
            this.reportIssuesLink.Size = new System.Drawing.Size(107, 20);
            this.reportIssuesLink.TabIndex = 12;
            this.reportIssuesLink.TabStop = true;
            this.reportIssuesLink.Text = "Report issues";
            this.reportIssuesLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.reportIssuesLink_LinkClicked);
            // 
            // screenshotCheckbox
            // 
            this.screenshotCheckbox.AutoSize = true;
            this.screenshotCheckbox.Location = new System.Drawing.Point(38, 319);
            this.screenshotCheckbox.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.screenshotCheckbox.Name = "screenshotCheckbox";
            this.screenshotCheckbox.Size = new System.Drawing.Size(243, 24);
            this.screenshotCheckbox.TabIndex = 13;
            this.screenshotCheckbox.Text = "Enable screenshot (Ctrl+F12)";
            this.screenshotCheckbox.UseVisualStyleBackColor = true;
            this.screenshotCheckbox.CheckedChanged += new System.EventHandler(this.sceenshotCheckbox_CheckedChanged);
            // 
            // checkUpdatesLink
            // 
            this.checkUpdatesLink.AutoSize = true;
            this.checkUpdatesLink.Location = new System.Drawing.Point(412, 426);
            this.checkUpdatesLink.Name = "checkUpdatesLink";
            this.checkUpdatesLink.Size = new System.Drawing.Size(191, 20);
            this.checkUpdatesLink.TabIndex = 14;
            this.checkUpdatesLink.TabStop = true;
            this.checkUpdatesLink.Text = "Check for a newer version";
            this.checkUpdatesLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.checkUpdatesLink_LinkClicked);
            // 
            // disableCheckbox
            // 
            this.disableCheckbox.AutoSize = true;
            this.disableCheckbox.Location = new System.Drawing.Point(38, 76);
            this.disableCheckbox.Name = "disableCheckbox";
            this.disableCheckbox.Size = new System.Drawing.Size(231, 24);
            this.disableCheckbox.TabIndex = 15;
            this.disableCheckbox.Text = "Disable the OpenXR Toolkit";
            this.disableCheckbox.UseVisualStyleBackColor = true;
            this.disableCheckbox.CheckedChanged += new System.EventHandler(this.disableCheckbox_CheckedChanged);
            // 
            // safemodeCheckbox
            // 
            this.safemodeCheckbox.AutoSize = true;
            this.safemodeCheckbox.Location = new System.Drawing.Point(38, 157);
            this.safemodeCheckbox.Name = "safemodeCheckbox";
            this.safemodeCheckbox.Size = new System.Drawing.Size(164, 24);
            this.safemodeCheckbox.TabIndex = 16;
            this.safemodeCheckbox.Text = "Enable safe mode";
            this.safemodeCheckbox.UseVisualStyleBackColor = true;
            this.safemodeCheckbox.CheckedChanged += new System.EventHandler(this.safemodeCheckbox_CheckedChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(61, 114);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(445, 20);
            this.label1.TabIndex = 17;
            this.label1.Text = "Completely disable the software without needing to uninstall it.";
            // 
            // experimentalCheckbox
            // 
            this.experimentalCheckbox.AutoSize = true;
            this.experimentalCheckbox.Location = new System.Drawing.Point(38, 238);
            this.experimentalCheckbox.Name = "experimentalCheckbox";
            this.experimentalCheckbox.Size = new System.Drawing.Size(239, 24);
            this.experimentalCheckbox.TabIndex = 18;
            this.experimentalCheckbox.Text = "Enable experimental settings";
            this.experimentalCheckbox.UseVisualStyleBackColor = true;
            this.experimentalCheckbox.CheckedChanged += new System.EventHandler(this.experimentalCheckbox_CheckedChanged);
            // 
            // label2
            // 
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(61, 195);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(481, 20);
            this.label2.TabIndex = 19;
            this.label2.Text = "Recover an application by deleting all its settings upon next startup.";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(61, 276);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(470, 20);
            this.label3.TabIndex = 20;
            this.label3.Text = "Expose experimental features that may be unfinished or unstable.";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(61, 357);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(329, 20);
            this.label4.TabIndex = 21;
            this.label4.Text = "Screenshots are stored in %LocalAppData%.";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(616, 459);
            this.Controls.Add(this.label4);
            this.Controls.Add(this.label3);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.experimentalCheckbox);
            this.Controls.Add(this.label1);
            this.Controls.Add(this.safemodeCheckbox);
            this.Controls.Add(this.disableCheckbox);
            this.Controls.Add(this.checkUpdatesLink);
            this.Controls.Add(this.screenshotCheckbox);
            this.Controls.Add(this.reportIssuesLink);
            this.Controls.Add(this.layerActive);
            this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.FixedSingle;
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.MaximizeBox = false;
            this.Name = "Form1";
            this.Text = "OpenXR Toolkit Companion app";
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion
        private System.Windows.Forms.Label layerActive;
        private System.Windows.Forms.ToolTip tooltip;
        private System.Windows.Forms.LinkLabel reportIssuesLink;
        private System.Windows.Forms.CheckBox screenshotCheckbox;
        private System.Windows.Forms.LinkLabel checkUpdatesLink;
        private System.Windows.Forms.CheckBox disableCheckbox;
        private System.Windows.Forms.CheckBox safemodeCheckbox;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.CheckBox experimentalCheckbox;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.Label label4;
    }
}

