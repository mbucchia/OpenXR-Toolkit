
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
            this.label2 = new System.Windows.Forms.Label();
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
            this.appList = new System.Windows.Forms.CheckedListBox();
            this.label14 = new System.Windows.Forms.Label();
            this.screenshotEye = new System.Windows.Forms.ComboBox();
            this.timer1 = new System.Windows.Forms.Timer(this.components);
            ((System.ComponentModel.ISupportInitialize)(this.pictureBox1)).BeginInit();
            this.SuspendLayout();
            // 
            // layerActive
            // 
            this.layerActive.Location = new System.Drawing.Point(34, 152);
            this.layerActive.Name = "layerActive";
            this.layerActive.Size = new System.Drawing.Size(548, 20);
            this.layerActive.TabIndex = 0;
            this.layerActive.Text = "Layer status is not known";
            this.layerActive.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // reportIssuesLink
            // 
            this.reportIssuesLink.AutoSize = true;
            this.reportIssuesLink.Location = new System.Drawing.Point(10, 1073);
            this.reportIssuesLink.Name = "reportIssuesLink";
            this.reportIssuesLink.Size = new System.Drawing.Size(107, 20);
            this.reportIssuesLink.TabIndex = 31;
            this.reportIssuesLink.TabStop = true;
            this.reportIssuesLink.Text = "Report issues";
            this.reportIssuesLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.reportIssuesLink_LinkClicked);
            // 
            // screenshotCheckbox
            // 
            this.screenshotCheckbox.AutoSize = true;
            this.screenshotCheckbox.Location = new System.Drawing.Point(38, 422);
            this.screenshotCheckbox.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.screenshotCheckbox.Name = "screenshotCheckbox";
            this.screenshotCheckbox.Size = new System.Drawing.Size(168, 24);
            this.screenshotCheckbox.TabIndex = 7;
            this.screenshotCheckbox.Text = "Enable screenshot";
            this.screenshotCheckbox.UseVisualStyleBackColor = true;
            this.screenshotCheckbox.CheckedChanged += new System.EventHandler(this.sceenshotCheckbox_CheckedChanged);
            // 
            // checkUpdatesLink
            // 
            this.checkUpdatesLink.AutoSize = true;
            this.checkUpdatesLink.Location = new System.Drawing.Point(411, 1073);
            this.checkUpdatesLink.Name = "checkUpdatesLink";
            this.checkUpdatesLink.Size = new System.Drawing.Size(191, 20);
            this.checkUpdatesLink.TabIndex = 33;
            this.checkUpdatesLink.TabStop = true;
            this.checkUpdatesLink.Text = "Check for a newer version";
            this.checkUpdatesLink.LinkClicked += new System.Windows.Forms.LinkLabelLinkClickedEventHandler(this.checkUpdatesLink_LinkClicked);
            // 
            // disableCheckbox
            // 
            this.disableCheckbox.AutoSize = true;
            this.disableCheckbox.Location = new System.Drawing.Point(38, 240);
            this.disableCheckbox.Name = "disableCheckbox";
            this.disableCheckbox.Size = new System.Drawing.Size(231, 24);
            this.disableCheckbox.TabIndex = 1;
            this.disableCheckbox.Text = "Disable the OpenXR Toolkit";
            this.disableCheckbox.UseVisualStyleBackColor = true;
            this.disableCheckbox.CheckedChanged += new System.EventHandler(this.disableCheckbox_CheckedChanged);
            // 
            // safemodeCheckbox
            // 
            this.safemodeCheckbox.AutoSize = true;
            this.safemodeCheckbox.Location = new System.Drawing.Point(38, 322);
            this.safemodeCheckbox.Name = "safemodeCheckbox";
            this.safemodeCheckbox.Size = new System.Drawing.Size(164, 24);
            this.safemodeCheckbox.TabIndex = 3;
            this.safemodeCheckbox.Text = "Enable safe mode";
            this.safemodeCheckbox.UseVisualStyleBackColor = true;
            this.safemodeCheckbox.CheckedChanged += new System.EventHandler(this.safemodeCheckbox_CheckedChanged);
            // 
            // label1
            // 
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(62, 278);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(445, 20);
            this.label1.TabIndex = 2;
            this.label1.Text = "Completely disable the software without needing to uninstall it.";
            // 
            // label2
            // 
            this.label2.Location = new System.Drawing.Point(62, 360);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(522, 40);
            this.label2.TabIndex = 4;
            this.label2.Text = "Recover an application by ignoring all its settings upon next startup. When in sa" +
    "fe mode, press Ctrl+F1+F2+F3 to delete all settings.";
            // 
            // label4
            // 
            this.label4.AutoSize = true;
            this.label4.Location = new System.Drawing.Point(62, 461);
            this.label4.Name = "label4";
            this.label4.Size = new System.Drawing.Size(537, 20);
            this.label4.TabIndex = 9;
            this.label4.Text = "Screenshots are stored in %LocalAppData%\\OpenXR-Toolkit\\screenshots.";
            // 
            // openLog
            // 
            this.openLog.Location = new System.Drawing.Point(27, 943);
            this.openLog.Name = "openLog";
            this.openLog.Size = new System.Drawing.Size(225, 48);
            this.openLog.TabIndex = 28;
            this.openLog.Text = "Open log file";
            this.openLog.UseVisualStyleBackColor = true;
            this.openLog.Click += new System.EventHandler(this.openLog_Click);
            // 
            // openScreenshots
            // 
            this.openScreenshots.Location = new System.Drawing.Point(280, 943);
            this.openScreenshots.Name = "openScreenshots";
            this.openScreenshots.Size = new System.Drawing.Size(225, 48);
            this.openScreenshots.TabIndex = 29;
            this.openScreenshots.Text = "Open screenshots folder";
            this.openScreenshots.UseVisualStyleBackColor = true;
            this.openScreenshots.Click += new System.EventHandler(this.openScreenshots_Click);
            // 
            // leftKey
            // 
            this.leftKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.leftKey.FormattingEnabled = true;
            this.leftKey.Location = new System.Drawing.Point(231, 632);
            this.leftKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.leftKey.Name = "leftKey";
            this.leftKey.Size = new System.Drawing.Size(110, 28);
            this.leftKey.TabIndex = 14;
            this.leftKey.SelectedIndexChanged += new System.EventHandler(this.leftKey_SelectedIndexChanged);
            // 
            // nextKey
            // 
            this.nextKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.nextKey.FormattingEnabled = true;
            this.nextKey.Location = new System.Drawing.Point(351, 632);
            this.nextKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.nextKey.Name = "nextKey";
            this.nextKey.Size = new System.Drawing.Size(110, 28);
            this.nextKey.TabIndex = 18;
            this.nextKey.SelectedIndexChanged += new System.EventHandler(this.nextKey_SelectedIndexChanged);
            // 
            // rightKey
            // 
            this.rightKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.rightKey.FormattingEnabled = true;
            this.rightKey.Location = new System.Drawing.Point(471, 632);
            this.rightKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.rightKey.Name = "rightKey";
            this.rightKey.Size = new System.Drawing.Size(110, 28);
            this.rightKey.TabIndex = 20;
            this.rightKey.SelectedIndexChanged += new System.EventHandler(this.rightKey_SelectedIndexChanged);
            // 
            // label5
            // 
            this.label5.AutoSize = true;
            this.label5.Location = new System.Drawing.Point(33, 636);
            this.label5.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label5.Name = "label5";
            this.label5.Size = new System.Drawing.Size(186, 20);
            this.label5.TabIndex = 12;
            this.label5.Text = "On-screen menu hotkeys";
            // 
            // label6
            // 
            this.label6.AutoSize = true;
            this.label6.Location = new System.Drawing.Point(140, 720);
            this.label6.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label6.Name = "label6";
            this.label6.Size = new System.Drawing.Size(73, 20);
            this.label6.TabIndex = 23;
            this.label6.Text = "Modifiers";
            // 
            // ctrlModifierCheckbox
            // 
            this.ctrlModifierCheckbox.AutoSize = true;
            this.ctrlModifierCheckbox.Location = new System.Drawing.Point(234, 720);
            this.ctrlModifierCheckbox.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.ctrlModifierCheckbox.Name = "ctrlModifierCheckbox";
            this.ctrlModifierCheckbox.Size = new System.Drawing.Size(59, 24);
            this.ctrlModifierCheckbox.TabIndex = 24;
            this.ctrlModifierCheckbox.Text = "Ctrl";
            this.ctrlModifierCheckbox.UseVisualStyleBackColor = true;
            this.ctrlModifierCheckbox.CheckedChanged += new System.EventHandler(this.ctrlModifierCheckbox_CheckedChanged);
            // 
            // altModifierCheckbox
            // 
            this.altModifierCheckbox.AutoSize = true;
            this.altModifierCheckbox.Location = new System.Drawing.Point(310, 720);
            this.altModifierCheckbox.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.altModifierCheckbox.Name = "altModifierCheckbox";
            this.altModifierCheckbox.Size = new System.Drawing.Size(54, 24);
            this.altModifierCheckbox.TabIndex = 25;
            this.altModifierCheckbox.Text = "Alt";
            this.altModifierCheckbox.UseVisualStyleBackColor = true;
            this.altModifierCheckbox.CheckedChanged += new System.EventHandler(this.altModifierCheckbox_CheckedChanged);
            // 
            // label7
            // 
            this.label7.AutoSize = true;
            this.label7.Location = new System.Drawing.Point(268, 607);
            this.label7.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label7.Name = "label7";
            this.label7.Size = new System.Drawing.Size(31, 20);
            this.label7.TabIndex = 13;
            this.label7.Text = "left";
            // 
            // label8
            // 
            this.label8.AutoSize = true;
            this.label8.Location = new System.Drawing.Point(381, 607);
            this.label8.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label8.Name = "label8";
            this.label8.Size = new System.Drawing.Size(47, 20);
            this.label8.TabIndex = 17;
            this.label8.Text = "down";
            // 
            // label9
            // 
            this.label9.AutoSize = true;
            this.label9.Location = new System.Drawing.Point(508, 607);
            this.label9.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label9.Name = "label9";
            this.label9.Size = new System.Drawing.Size(40, 20);
            this.label9.TabIndex = 19;
            this.label9.Text = "right";
            // 
            // pictureBox1
            // 
            this.pictureBox1.Image = global::companion.Properties.Resources.banner;
            this.pictureBox1.Location = new System.Drawing.Point(0, 0);
            this.pictureBox1.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.pictureBox1.Name = "pictureBox1";
            this.pictureBox1.Size = new System.Drawing.Size(620, 131);
            this.pictureBox1.TabIndex = 36;
            this.pictureBox1.TabStop = false;
            this.pictureBox1.Click += new System.EventHandler(this.pictureBox1_Click);
            // 
            // label10
            // 
            this.label10.AutoSize = true;
            this.label10.Location = new System.Drawing.Point(90, 678);
            this.label10.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label10.Name = "label10";
            this.label10.Size = new System.Drawing.Size(127, 20);
            this.label10.TabIndex = 21;
            this.label10.Text = "Take screenshot";
            // 
            // screenshotKey
            // 
            this.screenshotKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.screenshotKey.FormattingEnabled = true;
            this.screenshotKey.Location = new System.Drawing.Point(231, 673);
            this.screenshotKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.screenshotKey.Name = "screenshotKey";
            this.screenshotKey.Size = new System.Drawing.Size(110, 28);
            this.screenshotKey.TabIndex = 22;
            this.screenshotKey.SelectedIndexChanged += new System.EventHandler(this.screenshotKey_SelectedIndexChanged);
            // 
            // label11
            // 
            this.label11.AutoSize = true;
            this.label11.Location = new System.Drawing.Point(388, 549);
            this.label11.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label11.Name = "label11";
            this.label11.Size = new System.Drawing.Size(27, 20);
            this.label11.TabIndex = 15;
            this.label11.Text = "up";
            // 
            // previousKey
            // 
            this.previousKey.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.previousKey.FormattingEnabled = true;
            this.previousKey.Location = new System.Drawing.Point(351, 573);
            this.previousKey.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.previousKey.Name = "previousKey";
            this.previousKey.Size = new System.Drawing.Size(110, 28);
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
            this.menuVisibility.Location = new System.Drawing.Point(231, 498);
            this.menuVisibility.Name = "menuVisibility";
            this.menuVisibility.Size = new System.Drawing.Size(176, 28);
            this.menuVisibility.TabIndex = 11;
            this.menuVisibility.SelectedIndexChanged += new System.EventHandler(this.menuVisibility_SelectedIndexChanged);
            // 
            // label12
            // 
            this.label12.AutoSize = true;
            this.label12.Location = new System.Drawing.Point(34, 501);
            this.label12.Name = "label12";
            this.label12.Size = new System.Drawing.Size(185, 20);
            this.label12.TabIndex = 10;
            this.label12.Text = "In-headset menu visibility";
            // 
            // licences
            // 
            this.licences.AutoSize = true;
            this.licences.Location = new System.Drawing.Point(466, 1040);
            this.licences.Name = "licences";
            this.licences.Size = new System.Drawing.Size(139, 20);
            this.licences.TabIndex = 32;
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
            this.screenshotFormat.Location = new System.Drawing.Point(214, 419);
            this.screenshotFormat.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.screenshotFormat.Name = "screenshotFormat";
            this.screenshotFormat.Size = new System.Drawing.Size(110, 28);
            this.screenshotFormat.TabIndex = 8;
            this.screenshotFormat.SelectedIndexChanged += new System.EventHandler(this.screenshotFormat_SelectedIndexChanged);
            // 
            // traceButton
            // 
            this.traceButton.Location = new System.Drawing.Point(27, 998);
            this.traceButton.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.traceButton.Name = "traceButton";
            this.traceButton.Size = new System.Drawing.Size(225, 48);
            this.traceButton.TabIndex = 30;
            this.traceButton.Text = "Capture trace";
            this.traceButton.UseVisualStyleBackColor = true;
            this.traceButton.Click += new System.EventHandler(this.traceButton_Click);
            // 
            // label13
            // 
            this.label13.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.label13.Location = new System.Drawing.Point(34, 185);
            this.label13.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label13.Name = "label13";
            this.label13.Size = new System.Drawing.Size(548, 20);
            this.label13.TabIndex = 0;
            this.label13.Text = "You do not need to keep this window open";
            this.label13.TextAlign = System.Drawing.ContentAlignment.TopCenter;
            // 
            // appList
            // 
            this.appList.FormattingEnabled = true;
            this.appList.Location = new System.Drawing.Point(27, 784);
            this.appList.Margin = new System.Windows.Forms.Padding(4, 5, 4, 5);
            this.appList.Name = "appList";
            this.appList.Size = new System.Drawing.Size(553, 142);
            this.appList.TabIndex = 27;
            this.appList.ItemCheck += new System.Windows.Forms.ItemCheckEventHandler(this.appList_ItemCheck);
            // 
            // label14
            // 
            this.label14.AutoSize = true;
            this.label14.Location = new System.Drawing.Point(38, 755);
            this.label14.Margin = new System.Windows.Forms.Padding(4, 0, 4, 0);
            this.label14.Name = "label14";
            this.label14.Size = new System.Drawing.Size(392, 20);
            this.label14.TabIndex = 26;
            this.label14.Text = "Enable OpenXR Toolkit selectively for each application";
            // 
            // screenshotEye
            // 
            this.screenshotEye.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
            this.screenshotEye.FormattingEnabled = true;
            this.screenshotEye.Items.AddRange(new object[] {
            "Both eyes",
            "Left eye only",
            "Right eye only"});
            this.screenshotEye.Location = new System.Drawing.Point(351, 418);
            this.screenshotEye.Name = "screenshotEye";
            this.screenshotEye.Size = new System.Drawing.Size(176, 28);
            this.screenshotEye.TabIndex = 9;
            this.screenshotEye.SelectedIndexChanged += new System.EventHandler(this.screenshotEye_SelectedIndexChanged);
            // 
            // timer1
            // 
            this.timer1.Enabled = true;
            this.timer1.Interval = 5000;
            this.timer1.Tick += new System.EventHandler(this.timer1_Tick);
            // 
            // Form1
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(9F, 20F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(616, 1098);
            this.Controls.Add(this.screenshotEye);
            this.Controls.Add(this.label14);
            this.Controls.Add(this.appList);
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
            this.Controls.Add(this.label2);
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
        private System.Windows.Forms.Label label2;
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
        private System.Windows.Forms.CheckedListBox appList;
        private System.Windows.Forms.Label label14;
        private System.Windows.Forms.ComboBox screenshotEye;
        private System.Windows.Forms.Timer timer1;
    }
}

