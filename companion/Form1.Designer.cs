
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
            this.pictureBox1 = new System.Windows.Forms.PictureBox();
            this.label10 = new System.Windows.Forms.Label();
            this.screenshotKey = new System.Windows.Forms.ComboBox();
            this.label11 = new System.Windows.Forms.Label();
            this.previousKey = new System.Windows.Forms.ComboBox();
            this.menuVisibility = new System.Windows.Forms.ComboBox();
            this.label12 = new System.Windows.Forms.Label();
            this.licences = new System.Windows.Forms.LinkLabel();
            this.screenshotFormat = new System.Windows.Forms.ComboBox();
            this.traceButton = new System.Windows.Forms.Button();
            this.label13 = new System.Windows.Forms.Label();
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
            this.SuspendLayout();
            // 
            // layerActive
            // 
            this.layerActive.Location = new System.Drawing.Point(23, 99);
            this.layerActive.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.layerActive.Name = "layerActive";
            this.layerActive.Size = new System.Drawing.Size(365, 13);
            this.layerActive.TabIndex = 0;
            this.layerActive.Text = "Layer status is not known";
            this.layerActive.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // reportIssuesLink
            // 
            this.reportIssuesLink.AutoSize = true;
            this.reportIssuesLink.Location = new System.Drawing.Point(7, 673);
            this.reportIssuesLink.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.reportIssuesLink.Name = "reportIssuesLink";
            this.reportIssuesLink.Size = new System.Drawing.Size(71, 13);
            this.reportIssuesLink.TabIndex = 29;
            this.reportIssuesLink.TabStop = true;
            this.reportIssuesLink.Text = "Report issues";
            this.reportIssuesLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.reportIssuesLink_LinkClicked);
            // 
            // screenshotCheckbox
            // 
            this.screenshotCheckbox.AutoSize = true;
            this.screenshotCheckbox.Location = new System.Drawing.Point(25, 327);
            this.screenshotCheckbox.Name = "screenshotCheckbox";
            this.screenshotCheckbox.Size = new System.Drawing.Size(114, 17);
            this.screenshotCheckbox.TabIndex = 7;
            this.screenshotCheckbox.Text = "Enable screenshot";
            this.screenshotCheckbox.UseVisualStyleBackColor = true;
            this.screenshotCheckbox.CheckedChanged += new System.EventHandler(this.sceenshotCheckbox_CheckedChanged);
            // 
            // checkUpdatesLink
            // 
            this.checkUpdatesLink.AutoSize = true;
            this.checkUpdatesLink.Location = new System.Drawing.Point(274, 673);
            this.checkUpdatesLink.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.checkUpdatesLink.Name = "checkUpdatesLink";
            this.checkUpdatesLink.Size = new System.Drawing.Size(131, 13);
            this.checkUpdatesLink.TabIndex = 31;
            this.checkUpdatesLink.TabStop = true;
            this.checkUpdatesLink.Text = "Check for a newer version";
            this.checkUpdatesLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.checkUpdatesLink_LinkClicked);
            // 
            // disableCheckbox
            // 
            this.disableCheckbox.AutoSize = true;
            this.disableCheckbox.Location = new System.Drawing.Point(25, 156);
            this.disableCheckbox.Margin = new System.Windows.Forms.Padding(2);
            this.disableCheckbox.Name = "disableCheckbox";
            this.disableCheckbox.Size = new System.Drawing.Size(158, 17);
            this.disableCheckbox.TabIndex = 1;
            this.disableCheckbox.Text = "Disable the OpenXR Toolkit";
            this.disableCheckbox.UseVisualStyleBackColor = true;
            this.disableCheckbox.CheckedChanged += new System.EventHandler(this.disableCheckbox_CheckedChanged);
            // 
            // safemodeCheckbox
            // 
            this.safemodeCheckbox.AutoSize = true;
            this.safemodeCheckbox.Location = new System.Drawing.Point(25, 209);
            this.safemodeCheckbox.Margin = new System.Windows.Forms.Padding(2);
            this.safemodeCheckbox.Name = "safemodeCheckbox";
            this.safemodeCheckbox.Size = new System.Drawing.Size(111, 17);
            this.safemodeCheckbox.TabIndex = 3;
            this.safemodeCheckbox.Text = "Enable safe mode";
            this.safemodeCheckbox.UseVisualStyleBackColor = true;
            this.safemodeCheckbox.CheckedChanged += new System.EventHandler(this.safemodeCheckbox_CheckedChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(41, 181);
            this.label1.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(297, 13);
            this.label1.TabIndex = 2;
            this.label1.Text = "Completely disable the software without needing to uninstall it.";
            // 
            // experimentalCheckbox
            // 
            this.experimentalCheckbox.AutoSize = true;
            this.experimentalCheckbox.Location = new System.Drawing.Point(25, 275);
            this.experimentalCheckbox.Margin = new System.Windows.Forms.Padding(2);
            this.experimentalCheckbox.Name = "experimentalCheckbox";
            this.experimentalCheckbox.Size = new System.Drawing.Size(160, 17);
            this.experimentalCheckbox.TabIndex = 5;
            this.experimentalCheckbox.Text = "Enable experimental settings";
            this.experimentalCheckbox.UseVisualStyleBackColor = true;
            this.experimentalCheckbox.CheckedChanged += new System.EventHandler(this.experimentalCheckbox_CheckedChanged);
            // 
            // label2
            // 
            this.label2.Location = new System.Drawing.Point(41, 234);
            this.label2.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(348, 26);
            this.label2.TabIndex = 4;
            this.label2.Text = "Recover an application by ignoring all its settings upon next startup. When in sa" +
    "fe mode, press Ctrl+F1+F2+F3 to delete all settings.";
            // 
            // label3
            // 
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(41, 299);
            this.label3.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(312, 13);
            this.label3.TabIndex = 6;
            this.label3.Text = "Expose experimental features that may be unfinished or unstable.";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(41, 352);
            this.label4.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(360, 13);
            this.label4.TabIndex = 9;
            this.label4.Text = "Screenshots are stored in %LocalAppData%\\OpenXR-Toolkit\\screenshots.";
            // 
            // openLog
            // 
            this.openLog.Location = new System.Drawing.Point(18, 588);
            this.openLog.Margin = new System.Windows.Forms.Padding(2);
            this.openLog.Name = "openLog";
            this.openLog.Size = new System.Drawing.Size(150, 31);
            this.openLog.TabIndex = 26;
            this.openLog.Text = "Open log file";
            this.openLog.UseVisualStyleBackColor = true;
            this.openLog.Click += new System.EventHandler(this.openLog_Click);
            // 
            // openScreenshots
            // 
            this.openScreenshots.Location = new System.Drawing.Point(187, 588);
            this.openScreenshots.Margin = new System.Windows.Forms.Padding(2);
            this.openScreenshots.Name = "openScreenshots";
            this.openScreenshots.Size = new System.Drawing.Size(150, 31);
            this.openScreenshots.TabIndex = 27;
            this.openScreenshots.Text = "Open screenshots folder";
            this.openScreenshots.UseVisualStyleBackColor = true;
            this.openScreenshots.Click += new System.EventHandler(this.openScreenshots_Click);
            // 
            // leftKey
            // 
            this.leftKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.leftKey.FormattingEnabled = true;
            this.leftKey.Location = new System.Drawing.Point(154, 477);
            this.leftKey.Name = "leftKey";
            this.leftKey.Size = new System.Drawing.Size(75, 21);
            this.leftKey.TabIndex = 14;
            this.leftKey.SelectedIndexChanged += new System.EventHandler(this.leftKey_SelectedIndexChanged);
            // 
            // nextKey
            // 
            this.nextKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.nextKey.FormattingEnabled = true;
            this.nextKey.Location = new System.Drawing.Point(234, 477);
            this.nextKey.Name = "nextKey";
            this.nextKey.Size = new System.Drawing.Size(75, 21);
            this.nextKey.TabIndex = 18;
            this.nextKey.SelectedIndexChanged += new System.EventHandler(this.nextKey_SelectedIndexChanged);
            // 
            // rightKey
            // 
            this.rightKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.rightKey.FormattingEnabled = true;
            this.rightKey.Location = new System.Drawing.Point(314, 477);
            this.rightKey.Name = "rightKey";
            this.rightKey.Size = new System.Drawing.Size(75, 21);
            this.rightKey.TabIndex = 20;
            this.rightKey.SelectedIndexChanged += new System.EventHandler(this.rightKey_SelectedIndexChanged);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(22, 480);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(125, 13);
            this.label5.TabIndex = 12;
            this.label5.Text = "On-screen menu hotkeys";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(93, 544);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(49, 13);
            this.label6.TabIndex = 23;
            this.label6.Text = "Modifiers";
            // 
            // ctrlModifierCheckbox
            // 
            this.ctrlModifierCheckbox.AutoSize = true;
            this.ctrlModifierCheckbox.Location = new System.Drawing.Point(156, 544);
            this.ctrlModifierCheckbox.Name = "ctrlModifierCheckbox";
            this.ctrlModifierCheckbox.Size = new System.Drawing.Size(41, 17);
            this.ctrlModifierCheckbox.TabIndex = 24;
            this.ctrlModifierCheckbox.Text = "Ctrl";
            this.ctrlModifierCheckbox.UseVisualStyleBackColor = true;
            this.ctrlModifierCheckbox.CheckedChanged += new System.EventHandler(this.ctrlModifierCheckbox_CheckedChanged);
            // 
            // altModifierCheckbox
            // 
            this.altModifierCheckbox.AutoSize = true;
            this.altModifierCheckbox.Location = new System.Drawing.Point(207, 544);
            this.altModifierCheckbox.Name = "altModifierCheckbox";
            this.altModifierCheckbox.Size = new System.Drawing.Size(38, 17);
            this.altModifierCheckbox.TabIndex = 25;
            this.altModifierCheckbox.Text = "Alt";
            this.altModifierCheckbox.UseVisualStyleBackColor = true;
            this.altModifierCheckbox.CheckedChanged += new System.EventHandler(this.altModifierCheckbox_CheckedChanged);
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Location = new System.Drawing.Point(179, 461);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(21, 13);
            this.label7.TabIndex = 13;
            this.label7.Text = "left";
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Location = new System.Drawing.Point(254, 461);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(33, 13);
            this.label8.TabIndex = 17;
            this.label8.Text = "down";
            // 
            // label9
            // 
            this.label9.AutoSize = true;
            this.label9.Location = new System.Drawing.Point(339, 461);
            this.label9.Name = "label9";
            this.label9.Size = new System.Drawing.Size(27, 13);
            this.label9.TabIndex = 19;
            this.label9.Text = "right";
            // 
            // pictureBox1
            // 
            this.pictureBox1.Image = global::companion.Properties.Resources.banner;
            this.pictureBox1.Location = new System.Drawing.Point(0, 0);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(413, 85);
            this.pictureBox1.TabIndex = 36;
            this.pictureBox1.TabStop = false;
            this.pictureBox1.Click += new System.EventHandler(this.pictureBox1_Click);
            // 
            // label10
            // 
            this.label10.AutoSize = true;
            this.label10.Location = new System.Drawing.Point(60, 507);
            this.label10.Name = "label10";
            this.label10.Size = new System.Drawing.Size(87, 13);
            this.label10.TabIndex = 21;
            this.label10.Text = "Take screenshot";
            // 
            // screenshotKey
            // 
            this.screenshotKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.screenshotKey.FormattingEnabled = true;
            this.screenshotKey.Location = new System.Drawing.Point(154, 504);
            this.screenshotKey.Name = "screenshotKey";
            this.screenshotKey.Size = new System.Drawing.Size(75, 21);
            this.screenshotKey.TabIndex = 22;
            this.screenshotKey.SelectedIndexChanged += new System.EventHandler(this.screenshotKey_SelectedIndexChanged);
            // 
            // label11
            // 
            this.label11.AutoSize = true;
            this.label11.Location = new System.Drawing.Point(259, 423);
            this.label11.Name = "label11";
            this.label11.Size = new System.Drawing.Size(19, 13);
            this.label11.TabIndex = 15;
            this.label11.Text = "up";
            // 
            // previousKey
            // 
            this.previousKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.previousKey.FormattingEnabled = true;
            this.previousKey.Location = new System.Drawing.Point(234, 439);
            this.previousKey.Name = "previousKey";
            this.previousKey.Size = new System.Drawing.Size(75, 21);
            this.previousKey.TabIndex = 16;
            this.previousKey.SelectedIndexChanged += new System.EventHandler(this.previousKey_SelectedIndexChanged);
            // 
            // menuVisibility
            // 
            this.menuVisibility.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.menuVisibility.FormattingEnabled = true;
            this.menuVisibility.Items.AddRange(new object[] {
            "Both eyes",
            "Left eye only",
            "Right eye only"});
            this.menuVisibility.Location = new System.Drawing.Point(154, 390);
            this.menuVisibility.Margin = new System.Windows.Forms.Padding(2);
            this.menuVisibility.Name = "menuVisibility";
            this.menuVisibility.Size = new System.Drawing.Size(119, 21);
            this.menuVisibility.TabIndex = 11;
            this.menuVisibility.SelectedIndexChanged += new System.EventHandler(this.menuVisibility_SelectedIndexChanged);
            // 
            // label12
            // 
            this.label12.AutoSize = true;
            this.label12.Location = new System.Drawing.Point(23, 392);
            this.label12.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.label12.Name = "label12";
            this.label12.Size = new System.Drawing.Size(124, 13);
            this.label12.TabIndex = 10;
            this.label12.Text = "In-headset menu visibility";
            // 
            // licences
            // 
            this.licences.AutoSize = true;
            this.licences.Location = new System.Drawing.Point(311, 651);
            this.licences.Margin = new System.Windows.Forms.Padding(2, 0, 2, 0);
            this.licences.Name = "licences";
            this.licences.Size = new System.Drawing.Size(94, 13);
            this.licences.TabIndex = 30;
            this.licences.TabStop = true;
            this.licences.Text = "3rd Party Licenses";
            this.licences.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.licences_LinkClicked);
            // 
            // screenshotFormat
            // 
            this.screenshotFormat.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.screenshotFormat.FormattingEnabled = true;
            this.screenshotFormat.Items.AddRange(new object[] {
            "DDS",
            "PNG",
            "JPG",
            "BMP"});
            this.screenshotFormat.Location = new System.Drawing.Point(143, 325);
            this.screenshotFormat.Name = "screenshotFormat";
            this.screenshotFormat.Size = new System.Drawing.Size(75, 21);
            this.screenshotFormat.TabIndex = 8;
            this.screenshotFormat.SelectedIndexChanged += new System.EventHandler(this.screenshotFormat_SelectedIndexChanged);
            // 
            // traceButton
            // 
            this.traceButton.Location = new System.Drawing.Point(18, 624);
            this.traceButton.Name = "traceButton";
            this.traceButton.Size = new System.Drawing.Size(150, 31);
            this.traceButton.TabIndex = 37;
            this.traceButton.Text = "Capture trace";
            this.traceButton.UseVisualStyleBackColor = true;
            this.traceButton.Click += new System.EventHandler(this.traceButton_Click);
            // 
            // label13
            // 
            this.label13.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label13.Location = new System.Drawing.Point(23, 120);
            this.label13.Name = "label13";
            this.label13.Size = new System.Drawing.Size(365, 13);
            this.label13.TabIndex = 0;
            this.label13.Text = "You do not need to keep this window open";
            this.label13.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(411, 691);
            this.Controls.Add(this.label13);
            this.Controls.Add(this.traceButton);
            this.Controls.Add(this.screenshotFormat);
            this.Controls.Add(this.licences);
            this.Controls.Add(this.label12);
            this.Controls.Add(this.menuVisibility);
            this.Controls.Add(this.label11);
            this.Controls.Add(this.previousKey);
            this.Controls.Add(this.label10);
            this.Controls.Add(this.screenshotKey);
            this.Controls.Add(this.pictureBox1);
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
            this.MaximizeBox = false;
            this.Name = "Form1";
            this.Text = "OpenXR Toolkit Companion app";
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).EndInit();
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
        private System.Windows.Forms.PictureBox pictureBox1;
        private System.Windows.Forms.Label label10;
        private System.Windows.Forms.ComboBox screenshotKey;
        private System.Windows.Forms.Label label11;
        private System.Windows.Forms.ComboBox previousKey;
        private System.Windows.Forms.ComboBox menuVisibility;
        private System.Windows.Forms.Label label12;
        private System.Windows.Forms.LinkLabel licences;
        private System.Windows.Forms.ComboBox screenshotFormat;
        private System.Windows.Forms.Button traceButton;
        private System.Windows.Forms.Label label13;
    }
}

