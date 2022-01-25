
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
            this.openLog = new System.Windows.Forms.Button();
            this.openScreenshots = new System.Windows.Forms.Button();
            this.leftKey = new System.Windows.Forms.ComboBox();
            this.nextKey = new System.Windows.Forms.ComboBox();
            this.rightKey = new System.Windows.Forms.ComboBox();
            this.label5 = new System.Windows.Forms.Label();
            this.label6 = new System.Windows.Forms.Label();
            this.ctrlModifierCheckbox = new System.Windows.Forms.CheckBox();
            this.altModifierCheckbox = new System.Windows.Forms.CheckBox();
            this.label7 = new System.Windows.Forms.Label();
            this.label8 = new System.Windows.Forms.Label();
            this.label9 = new System.Windows.Forms.Label();
            this.SuspendLayout();
            // 
            // layerActive
            // 
            this.layerActive.AutoSize = true;
            this.layerActive.Location = new System.Drawing.Point(34, 28);
            this.layerActive.Name = "layerActive";
            this.layerActive.Size = new System.Drawing.Size(188, 20);
            this.layerActive.TabIndex = 10;
            this.layerActive.Text = "Layer status is not known";
            // 
            // reportIssuesLink
            // 
            this.reportIssuesLink.AutoSize = true;
            this.reportIssuesLink.Location = new System.Drawing.Point(10, 645);
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
            this.screenshotCheckbox.Location = new System.Drawing.Point(38, 338);
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
            this.checkUpdatesLink.Location = new System.Drawing.Point(411, 645);
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
            this.disableCheckbox.Location = new System.Drawing.Point(38, 75);
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
            this.label1.Location = new System.Drawing.Point(62, 114);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(445, 20);
            this.label1.TabIndex = 17;
            this.label1.Text = "Completely disable the software without needing to uninstall it.";
            // 
            // experimentalCheckbox
            // 
            this.experimentalCheckbox.AutoSize = true;
            this.experimentalCheckbox.Location = new System.Drawing.Point(38, 258);
            this.experimentalCheckbox.Name = "experimentalCheckbox";
            this.experimentalCheckbox.Size = new System.Drawing.Size(239, 24);
            this.experimentalCheckbox.TabIndex = 18;
            this.experimentalCheckbox.Text = "Enable experimental settings";
            this.experimentalCheckbox.UseVisualStyleBackColor = true;
            this.experimentalCheckbox.CheckedChanged += new System.EventHandler(this.experimentalCheckbox_CheckedChanged);
            // 
            // label2
            // 
            this.label2.Location = new System.Drawing.Point(62, 195);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(522, 40);
            this.label2.TabIndex = 19;
            this.label2.Text = "Recover an application by ignoring all its settings upon next startup. When in sa" +
    "fe mode, press Ctrl+F1+F2+F3 to delete all settings.";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(62, 295);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(470, 20);
            this.label3.TabIndex = 20;
            this.label3.Text = "Expose experimental features that may be unfinished or unstable.";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(62, 377);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(537, 20);
            this.label4.TabIndex = 21;
            this.label4.Text = "Screenshots are stored in %LocalAppData%\\OpenXR-Toolkit\\screenshots.";
            // 
            // openLog
            // 
            this.openLog.Location = new System.Drawing.Point(27, 574);
            this.openLog.Name = "openLog";
            this.openLog.Size = new System.Drawing.Size(225, 48);
            this.openLog.TabIndex = 22;
            this.openLog.Text = "Open log file";
            this.openLog.UseVisualStyleBackColor = true;
            this.openLog.Click += new System.EventHandler(this.openLog_Click);
            // 
            // openScreenshots
            // 
            this.openScreenshots.Location = new System.Drawing.Point(280, 574);
            this.openScreenshots.Name = "openScreenshots";
            this.openScreenshots.Size = new System.Drawing.Size(225, 48);
            this.openScreenshots.TabIndex = 23;
            this.openScreenshots.Text = "Open screenshots folder";
            this.openScreenshots.UseVisualStyleBackColor = true;
            this.openScreenshots.Click += new System.EventHandler(this.openScreenshots_Click);
            // 
            // leftKey
            // 
            this.leftKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.leftKey.FormattingEnabled = true;
            this.leftKey.Location = new System.Drawing.Point(231, 458);
            this.leftKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.leftKey.Name = "leftKey";
            this.leftKey.Size = new System.Drawing.Size(110, 28);
            this.leftKey.TabIndex = 24;
            this.leftKey.SelectedIndexChanged += new System.EventHandler(this.leftKey_SelectedIndexChanged);
            // 
            // nextKey
            // 
            this.nextKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.nextKey.FormattingEnabled = true;
            this.nextKey.Location = new System.Drawing.Point(351, 458);
            this.nextKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.nextKey.Name = "nextKey";
            this.nextKey.Size = new System.Drawing.Size(110, 28);
            this.nextKey.TabIndex = 25;
            this.nextKey.SelectedIndexChanged += new System.EventHandler(this.nextKey_SelectedIndexChanged);
            // 
            // rightKey
            // 
            this.rightKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.rightKey.FormattingEnabled = true;
            this.rightKey.Location = new System.Drawing.Point(471, 458);
            this.rightKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.rightKey.Name = "rightKey";
            this.rightKey.Size = new System.Drawing.Size(110, 28);
            this.rightKey.TabIndex = 26;
            this.rightKey.SelectedIndexChanged += new System.EventHandler(this.rightKey_SelectedIndexChanged);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(33, 463);
            this.label5.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(186, 20);
            this.label5.TabIndex = 27;
            this.label5.Text = "On-screen menu hotkeys";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(140, 506);
            this.label6.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(73, 20);
            this.label6.TabIndex = 28;
            this.label6.Text = "Modifiers";
            // 
            // ctrlModifierCheckbox
            // 
            this.ctrlModifierCheckbox.AutoSize = true;
            this.ctrlModifierCheckbox.Location = new System.Drawing.Point(234, 506);
            this.ctrlModifierCheckbox.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.ctrlModifierCheckbox.Name = "ctrlModifierCheckbox";
            this.ctrlModifierCheckbox.Size = new System.Drawing.Size(59, 24);
            this.ctrlModifierCheckbox.TabIndex = 29;
            this.ctrlModifierCheckbox.Text = "Ctrl";
            this.ctrlModifierCheckbox.UseVisualStyleBackColor = true;
            this.ctrlModifierCheckbox.CheckedChanged += new System.EventHandler(this.ctrlModifierCheckbox_CheckedChanged);
            // 
            // altModifierCheckbox
            // 
            this.altModifierCheckbox.AutoSize = true;
            this.altModifierCheckbox.Location = new System.Drawing.Point(310, 506);
            this.altModifierCheckbox.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.altModifierCheckbox.Name = "altModifierCheckbox";
            this.altModifierCheckbox.Size = new System.Drawing.Size(54, 24);
            this.altModifierCheckbox.TabIndex = 30;
            this.altModifierCheckbox.Text = "Alt";
            this.altModifierCheckbox.UseVisualStyleBackColor = true;
            this.altModifierCheckbox.CheckedChanged += new System.EventHandler(this.altModifierCheckbox_CheckedChanged);
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Location = new System.Drawing.Point(268, 434);
            this.label7.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(31, 20);
            this.label7.TabIndex = 32;
            this.label7.Text = "left";
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Location = new System.Drawing.Point(381, 434);
            this.label8.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(47, 20);
            this.label8.TabIndex = 33;
            this.label8.Text = "down";
            // 
            // label9
            // 
            this.label9.AutoSize = true;
            this.label9.Location = new System.Drawing.Point(508, 434);
            this.label9.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label9.Name = "label9";
            this.label9.Size = new System.Drawing.Size(40, 20);
            this.label9.TabIndex = 34;
            this.label9.Text = "right";
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(616, 674);
            this.Controls.Add(this.label9);
            this.Controls.Add(this.label8);
            this.Controls.Add(this.label7);
            this.Controls.Add(this.altModifierCheckbox);
            this.Controls.Add(this.ctrlModifierCheckbox);
            this.Controls.Add(this.label6);
            this.Controls.Add(this.label5);
            this.Controls.Add(this.rightKey);
            this.Controls.Add(this.nextKey);
            this.Controls.Add(this.leftKey);
            this.Controls.Add(this.openScreenshots);
            this.Controls.Add(this.openLog);
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
        private System.Windows.Forms.Button openLog;
        private System.Windows.Forms.Button openScreenshots;
        private System.Windows.Forms.ComboBox leftKey;
        private System.Windows.Forms.ComboBox rightKey;
        private System.Windows.Forms.Label label5;
        private System.Windows.Forms.Label label6;
        private System.Windows.Forms.CheckBox ctrlModifierCheckbox;
        private System.Windows.Forms.CheckBox altModifierCheckbox;
        private System.Windows.Forms.ComboBox nextKey;
        private System.Windows.Forms.Label label7;
        private System.Windows.Forms.Label label8;
        private System.Windows.Forms.Label label9;
    }
}

