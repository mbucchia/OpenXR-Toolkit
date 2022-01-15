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
using System.Net.Sockets;
using System.Numerics;
using System.IO;

namespace mappingtool
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            ResetDefaults();
        }

        private void ResetDefaults()
        {
            m_initializing = true;

            // Set all the defaults.
            // NOTE: Have to maintain parity with config::Reset() in dllmain.cpp.

            leftXOffset.Value = 0;
            leftXOffset_Scroll(null, null);
            leftYOffset.Value = 0;
            leftYOffset_Scroll(null, null);
            leftZOffset.Value = 0;
            leftZOffset_Scroll(null, null);
            leftXRotation.Value = 0;
            leftXRotation_Scroll(null, null);
            leftYRotation.Value = 0;
            leftYRotation_Scroll(null, null);
            leftZRotation.Value = 0;
            leftZRotation_Scroll(null, null);
            leftDisable.Checked = false;
            rightXOffset.Value = 0;
            rightXOffset_Scroll(null, null);
            rightYOffset.Value = 0;
            rightYOffset_Scroll(null, null);
            rightZOffset.Value = 0;
            rightZOffset_Scroll(null, null);
            rightXRotation.Value = 0;
            rightXRotation_Scroll(null, null);
            rightYRotation.Value = 0;
            rightYRotation_Scroll(null, null);
            rightZRotation.Value = 0;
            rightZRotation_Scroll(null, null);
            rightDisable.Checked = false;
            gripJoint.SelectedIndex = 0; // Palm
            aimJoint.SelectedIndex = 8; // Index intermediate

            leftPinchAction.SelectedIndex = 2; // /input/trigger/value
            leftThumbPressAction.SelectedIndex = 0;
            leftIndexBendAction.SelectedIndex = 0;
            leftFingerGunAction.SelectedIndex = 0;
            leftSqueezeAction.SelectedIndex = 3; // /input/squeeze/value
            leftWristTapAction.SelectedIndex = 1; // /input/menu/click
            leftPalmTapAction.SelectedIndex = 0;
            leftIndexTipTapAction.SelectedIndex = 8; // /input/b/click
            rightPinchAction.SelectedIndex = 2; // /input/trigger/value
            rightThumbPressAction.SelectedIndex = 0;
            rightIndexBendAction.SelectedIndex = 0;
            rightFingerGunAction.SelectedIndex = 0;
            rightSqueezeAction.SelectedIndex = 3; // /input/squeeze/value
            rightWristTapAction.SelectedIndex = 0;
            rightPalmTapAction.SelectedIndex = 0;
            leftCustom1Action.SelectedIndex = 0;
            rightCustom1Action.SelectedIndex = 0;
            interactionProfile.SelectedIndex = 1; // HP Reverb

            pinchNear.Value = 0;
            pinchNear_Scroll(null, null);
            pinchFar.Value = 50;
            pinchFar_Scroll(null, null);
            thumbPressNear.Value = 0;
            thumbPressNear_Scroll(null, null);
            thumbPressFar.Value = 50;
            thumbPressFar_Scroll(null, null);
            indexBendNear.Value = 45;
            indexBendNear_Scroll(null, null);
            indexBendFar.Value = 70;
            indexBendFar_Scroll(null, null);
            fingerGunNear.Value = 10;
            fingerGunNear_Scroll(null, null);
            fingerGunFar.Value = 30;
            fingerGunFar_Scroll(null, null);
            squeezeNear.Value = 35;
            squeezeNear_Scroll(null, null);
            squeezeFar.Value = 70;
            squeezeFar_Scroll(null, null);
            wristTapNear.Value = 40;
            wristTapNear_Scroll(null, null);
            wristTapFar.Value = 60;
            wristTapFar_Scroll(null, null);
            palmTapNear.Value = 20;
            palmTapNear_Scroll(null, null);
            palmTapFar.Value = 60;
            palmTapFar_Scroll(null, null);
            indexTipTapNear.Value = 0;
            indexTipTapNear_Scroll(null, null);
            indexTipTapFar.Value = 70;
            indexTipTapFar_Scroll(null, null);
            clickThreshold.Value = 75;
            clickThreshold_Scroll(null, null);
            custom1Joint1.SelectedIndex = 0;
            custom1Joint2.SelectedIndex = 0;
            custom1Near.Value = 0;
            custom1Near_Scroll(null, null);
            custom1Far.Value = 100;
            custom1Far_Scroll(null, null);

            m_initializing = false;
        }

        #region Offsets tab.
        private void UpdateLeftOffset()
        {
            SendUpdate("left.transform.vec", (leftXOffset.Value / 1000.0f).ToString() + " " + (leftYOffset.Value / 1000.0f).ToString() + " " + (leftZOffset.Value / 1000.0f).ToString());
        }

        private void UpdateRightOffset()
        {
            SendUpdate("right.transform.vec", (rightXOffset.Value / 1000.0f).ToString() + " " + (rightYOffset.Value / 1000.0f).ToString() + " " + (rightZOffset.Value / 1000.0f).ToString());
        }

        private void UpdateLeftRotation()
        {
            // We store the Euler angles as well (the API layer will ignore them and use the quaternion instead.
            if (saveToFile != null)
            {
                SendUpdate("left.transform.euler", leftXRotation.Value + " " + leftYRotation.Value + " " + leftZRotation.Value);
            }

            Quaternion q = Quaternion.CreateFromYawPitchRoll(
                (float)(leftYRotation.Value * Math.PI) / 180.0f,
                (float)(leftXRotation.Value * Math.PI) / 180.0f,
                (float)(leftZRotation.Value * Math.PI) / 180.0f);
            SendUpdate("left.transform.quat", q.X.ToString() + " " + q.Y.ToString() + " " + q.Z.ToString() + " " + q.W.ToString());
        }

        private void UpdateRightRotation()
        {
            // We store the Euler angles as well (the API layer will ignore them and use the quaternion instead.
            if (saveToFile != null)
            {
                SendUpdate("right.transform.euler", rightXRotation.Value + " " + rightYRotation.Value + " " + rightZRotation.Value);
            }

            Quaternion q = Quaternion.CreateFromYawPitchRoll(
                (float)(rightYRotation.Value * Math.PI) / 180.0f,
                (float)(rightXRotation.Value * Math.PI) / 180.0f,
                (float)(rightZRotation.Value * Math.PI) / 180.0f);
            SendUpdate("right.transform.quat", q.X.ToString() + " " + q.Y.ToString() + " " + q.Z.ToString() + " " + q.W.ToString());
        }

        private void leftXOffset_Scroll(object sender, EventArgs e)
        {
            leftXOffsetText.Text = leftXOffset.Value.ToString();
            UpdateLeftOffset();
        }

        private void leftYOffset_Scroll(object sender, EventArgs e)
        {
            leftYOffsetText.Text = leftYOffset.Value.ToString();
            UpdateLeftOffset();
        }

        private void leftZOffset_Scroll(object sender, EventArgs e)
        {
            leftZOffsetText.Text = leftZOffset.Value.ToString();
            UpdateLeftOffset();
        }

        private void leftXRotation_Scroll(object sender, EventArgs e)
        {
            leftXRotationText.Text = leftXRotation.Value.ToString();
            UpdateLeftRotation();
        }

        private void leftYRotation_Scroll(object sender, EventArgs e)
        {
            leftYRotationText.Text = leftYRotation.Value.ToString();
            UpdateLeftRotation();
        }

        private void leftZRotation_Scroll(object sender, EventArgs e)
        {
            leftZRotationText.Text = leftZRotation.Value.ToString();
            UpdateLeftRotation();
        }

        private void leftDisable_CheckedChanged(object sender, EventArgs e)
        {
            SendUpdate("left.enabled", leftDisable.Checked ? "false" : "true");
        }

        private void rightXOffset_Scroll(object sender, EventArgs e)
        {
            rightXOffsetText.Text = rightXOffset.Value.ToString();
            UpdateRightOffset();
        }

        private void rightYOffset_Scroll(object sender, EventArgs e)
        {
            rightYOffsetText.Text = rightYOffset.Value.ToString();
            UpdateRightOffset();
        }

        private void rightZOffset_Scroll(object sender, EventArgs e)
        {
            rightZOffsetText.Text = rightZOffset.Value.ToString();
            UpdateRightOffset();
        }

        private void rightXRotation_Scroll(object sender, EventArgs e)
        {
            rightXRotationText.Text = rightXRotation.Value.ToString();
            UpdateRightRotation();
        }

        private void rightYRotation_Scroll(object sender, EventArgs e)
        {
            rightYRotationText.Text = rightYRotation.Value.ToString();
            UpdateRightRotation();
        }

        private void rightZRotation_Scroll(object sender, EventArgs e)
        {
            rightZRotationText.Text = rightZRotation.Value.ToString();
            UpdateRightRotation();
        }

        private void rightDisable_CheckedChanged(object sender, EventArgs e)
        {
            SendUpdate("right.enabled", rightDisable.Checked ? "false" : "true");
        }

        private void gripJoint_SelectedIndexChanged(object sender, EventArgs e)
        {
            SendUpdate("grip_joint", gripJoint.SelectedIndex.ToString());
        }

        private void aimJoint_SelectedIndexChanged(object sender, EventArgs e)
        {
            SendUpdate("aim_joint", aimJoint.SelectedIndex.ToString());
        }

        private void linkLabel1_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            string jointsDescriptionUrl = "https://raw.githubusercontent.com/KhronosGroup/OpenXR-Docs/master/specification/sources/images/ext_hand_tracking_joint_convention.png";

            linkLabel1.LinkVisited = true;
            System.Diagnostics.Process.Start(jointsDescriptionUrl);
        }

        private void linkLabel2_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
        {
            string gripAxesDescriptionUrl = "https://raw.githubusercontent.com/KhronosGroup/OpenXR-Docs/master/specification/sources/images/grip_axes_diagram.png";

            linkLabel2.LinkVisited = true;
            System.Diagnostics.Process.Start(gripAxesDescriptionUrl);
        }
        #endregion

        #region Bindings tab.
        private void UpdateAction(string property, string rawValue)
        {
            // Strip the note string when there is one.
            SendUpdate(property, rawValue.Split(' ')[0]);
        }

        private void leftPinchAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.pinch", leftPinchAction.Text);
        }

        private void leftThumbPressAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.thumb_press", leftThumbPressAction.Text);
        }

        private void leftIndexBendAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.index_bend", leftIndexBendAction.Text);
        }
        private void leftFingerGunAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.finger_gun", leftFingerGunAction.Text);
        }

        private void rightPinchAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.pinch", rightPinchAction.Text);
        }

        private void rightThumbPressAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.thumb_press", rightThumbPressAction.Text);
        }

        private void rightIndexBendAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.index_bend", rightIndexBendAction.Text);
        }

        private void rightFingerGunAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.finger_gun", rightFingerGunAction.Text);
        }

        private void leftSqueezeAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.squeeze", leftSqueezeAction.Text);
        }

        private void rightSqueezeAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.squeeze", rightSqueezeAction.Text);
        }

        private void leftWristTapAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.wrist_tap", leftWristTapAction.Text);
        }

        private void leftPalmTapAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.palm_tap", leftPalmTapAction.Text);
        }

        private void leftIndexTipTapAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.index_tip_tap", leftIndexTipTapAction.Text);
        }

        private void rightWristTapAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.wrist_tap", rightWristTapAction.Text);
        }

        private void rightPalmTapAction_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.palm_tap", rightPalmTapAction.Text);
        }

        private void leftCustom1Action_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("left.custom1", leftCustom1Action.Text);
        }

        private void rightCustom1Action_SelectedIndexChanged(object sender, EventArgs e)
        {
            UpdateAction("right.custom1", rightCustom1Action.Text);
        }

        private void interactionProfile_SelectedIndexChanged(object sender, EventArgs e)
        {
            // Strip the note string when there is one.
            UpdateAction("interaction_profile", interactionProfile.Text.Split(' ')[0]);
        }
        #endregion

        #region Gestures tab.

        // TODO: Deduplicate the code below.

        private void pinchNear_Scroll(object sender, EventArgs e)
        {
            pinchNearText.Text = pinchNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (pinchNear.Value >= pinchFar.Value)
            {
                pinchFar.Value = pinchNear.Value + 1;
                pinchFar_Scroll(null, null);
            }
            SendUpdate("pinch.near", (pinchNear.Value / 1000.0f).ToString());
        }

        private void pinchFar_Scroll(object sender, EventArgs e)
        {
            pinchFarText.Text = pinchFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (pinchFar.Value <= pinchNear.Value)
            {
                pinchNear.Value = pinchFar.Value - 1;
                pinchNear_Scroll(null, null);
            }
            SendUpdate("pinch.far", (pinchFar.Value / 1000.0f).ToString());
        }

        private void thumbPressNear_Scroll(object sender, EventArgs e)
        {
            thumbPressNearText.Text = thumbPressNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (thumbPressNear.Value >= thumbPressFar.Value)
            {
                thumbPressFar.Value = thumbPressNear.Value + 1;
                thumbPressFar_Scroll(null, null);
            }
            SendUpdate("thumb_press.near", (thumbPressNear.Value / 1000.0f).ToString());
        }

        private void thumbPressFar_Scroll(object sender, EventArgs e)
        {
            thumbPressFarText.Text = thumbPressFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (thumbPressFar.Value <= thumbPressNear.Value)
            {
                thumbPressNear.Value = thumbPressFar.Value - 1;
                thumbPressNear_Scroll(null, null);
            }
            SendUpdate("thumb_press.far", (thumbPressFar.Value / 1000.0f).ToString());
        }

        private void indexBendNear_Scroll(object sender, EventArgs e)
        {
            indexBendNearText.Text = indexBendNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (indexBendNear.Value >= indexBendFar.Value)
            {
                indexBendFar.Value = indexBendNear.Value + 1;
                indexBendFar_Scroll(null, null);
            }
            SendUpdate("index_bend.near", (indexBendNear.Value / 1000.0f).ToString());
        }

        private void indexBendFar_Scroll(object sender, EventArgs e)
        {
            indexBendFarText.Text = indexBendFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (indexBendFar.Value <= indexBendNear.Value)
            {
                indexBendNear.Value = indexBendFar.Value - 1;
                indexBendNear_Scroll(null, null);
            }
            SendUpdate("index_bend.far", (indexBendFar.Value / 1000.0f).ToString());
        }

        private void fingerGunNear_Scroll(object sender, EventArgs e)
        {
            fingerGunNearText.Text = fingerGunNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (fingerGunNear.Value >= fingerGunFar.Value)
            {
                fingerGunFar.Value = fingerGunNear.Value + 1;
                fingerGunFar_Scroll(null, null);
            }
            SendUpdate("finger_gun.near", (fingerGunNear.Value / 1000.0f).ToString());
        }

        private void fingerGunFar_Scroll(object sender, EventArgs e)
        {
            fingerGunFarText.Text = fingerGunFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (fingerGunFar.Value <= fingerGunNear.Value)
            {
                fingerGunNear.Value = fingerGunFar.Value - 1;
                fingerGunNear_Scroll(null, null);
            }
            SendUpdate("finger_gun.far", (fingerGunFar.Value / 1000.0f).ToString());
        }

        private void squeezeNear_Scroll(object sender, EventArgs e)
        {
            squeezeNearText.Text = squeezeNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (squeezeNear.Value >= squeezeFar.Value)
            {
                squeezeFar.Value = squeezeNear.Value + 1;
                squeezeFar_Scroll(null, null);
            }
            SendUpdate("squeeze.near", (squeezeNear.Value / 1000.0f).ToString());
        }

        private void squeezeFar_Scroll(object sender, EventArgs e)
        {
            squeezeFarText.Text = squeezeFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (squeezeFar.Value <= squeezeNear.Value)
            {
                squeezeNear.Value = squeezeFar.Value - 1;
                squeezeNear_Scroll(null, null);
            }
            SendUpdate("squeeze.far", (squeezeFar.Value / 1000.0f).ToString());
        }

        private void wristTapNear_Scroll(object sender, EventArgs e)
        {
            wristTapNearText.Text = wristTapNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (wristTapNear.Value >= wristTapFar.Value)
            {
                wristTapFar.Value = wristTapNear.Value + 1;
                wristTapFar_Scroll(null, null);
            }
            SendUpdate("wrist_tap.near", (wristTapNear.Value / 1000.0f).ToString());
        }

        private void wristTapFar_Scroll(object sender, EventArgs e)
        {
            wristTapFarText.Text = wristTapFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (wristTapFar.Value <= wristTapNear.Value)
            {
                wristTapNear.Value = wristTapFar.Value - 1;
                wristTapNear_Scroll(null, null);
            }
            SendUpdate("wrist_tap.far", (wristTapFar.Value / 1000.0f).ToString());
        }

        private void palmTapNear_Scroll(object sender, EventArgs e)
        {
            palmTapNearText.Text = palmTapNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (palmTapNear.Value >= palmTapFar.Value)
            {
                palmTapFar.Value = palmTapNear.Value + 1;
                palmTapFar_Scroll(null, null);
            }
            SendUpdate("palm_tap.near", (palmTapNear.Value / 1000.0f).ToString());
        }

        private void palmTapFar_Scroll(object sender, EventArgs e)
        {
            palmTapFarText.Text = palmTapFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (palmTapFar.Value <= palmTapNear.Value)
            {
                palmTapNear.Value = palmTapFar.Value - 1;
                palmTapNear_Scroll(null, null);
            }
            SendUpdate("palm_tap.far", (palmTapFar.Value / 1000.0f).ToString());
        }

        private void indexTipTapNear_Scroll(object sender, EventArgs e)
        {
            indexTipTapNearText.Text = indexTipTapNear.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (indexTipTapNear.Value >= indexTipTapFar.Value)
            {
                indexTipTapFar.Value = indexTipTapNear.Value + 1;
                indexTipTapFar_Scroll(null, null);
            }
            SendUpdate("index_tip_tap.near", (indexTipTapNear.Value / 1000.0f).ToString());
        }

        private void indexTipTapFar_Scroll(object sender, EventArgs e)
        {
            indexTipTapFarText.Text = indexTipTapFar.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (indexTipTapFar.Value <= indexTipTapNear.Value)
            {
                indexTipTapNear.Value = indexTipTapFar.Value - 1;
                indexTipTapNear_Scroll(null, null);
            }
            SendUpdate("index_tip_tap.far", (indexTipTapFar.Value / 1000.0f).ToString());
        }

        private void clickThreshold_Scroll(object sender, EventArgs e)
        {
            clickThresholdText.Text = clickThreshold.Value.ToString();
            SendUpdate("click_threshold", (clickThreshold.Value / 100.0f).ToString());
        }

        private void custom1Joint1_SelectedIndexChanged(object sender, EventArgs e)
        {
            SendUpdate("custom1_joint1", (custom1Joint1.SelectedIndex - 1).ToString());
        }

        private void custom1Joint2_SelectedIndexChanged(object sender, EventArgs e)
        {
            SendUpdate("custom1_joint2", (custom1Joint2.SelectedIndex - 1).ToString());
        }

        private void custom1Near_Scroll(object sender, EventArgs e)
        {
            custom1NearText.Text = custom1Near.Value.ToString();
            // We must always keep the far value greater than the near value.
            if (custom1Near.Value >= custom1Far.Value)
            {
                custom1Far.Value = custom1Near.Value + 1;
                custom1Far_Scroll(null, null);
            }
            SendUpdate("custom1.near", (custom1Near.Value / 1000.0f).ToString());
        }

        private void custom1Far_Scroll(object sender, EventArgs e)
        {
            custom1FarText.Text = custom1Far.Value.ToString();
            // We must always keep the near value smaller than the near value.
            if (custom1Far.Value <= custom1Near.Value)
            {
                custom1Near.Value = custom1Far.Value - 1;
                custom1Near_Scroll(null, null);
            }
            SendUpdate("custom1.far", (custom1Far.Value / 1000.0f).ToString());
        }
        #endregion


        private bool m_initializing = true;
        private StreamWriter saveToFile = null;

        private void SendUpdate(string property, string value)
        {
            if (m_initializing)
            {
                return;
            }

            string line = property + "=" + value;

            if (saveToFile == null)
            {
                UdpClient udpClient = new UdpClient();
                udpClient.Connect("localhost", 10001);
                Byte[] sendBytes = Encoding.ASCII.GetBytes(line);
                udpClient.Send(sendBytes, sendBytes.Length);
                status.Text = line;
            }
            else
            {
                saveToFile.WriteLine(line);
            }
        }

        private void FlushConfiguration()
        {
            leftXOffset_Scroll(null, null);
            leftXRotation_Scroll(null, null);
            rightXOffset_Scroll(null, null);
            rightXRotation_Scroll(null, null);
            leftDisable_CheckedChanged(null, null);
            rightDisable_CheckedChanged(null, null);
            gripJoint_SelectedIndexChanged(null, null);
            aimJoint_SelectedIndexChanged(null, null);

            leftPinchAction_SelectedIndexChanged(null, null);
            leftThumbPressAction_SelectedIndexChanged(null, null);
            leftIndexBendAction_SelectedIndexChanged(null, null);
            leftFingerGunAction_SelectedIndexChanged(null, null);
            leftSqueezeAction_SelectedIndexChanged(null, null);
            leftWristTapAction_SelectedIndexChanged(null, null);
            leftPalmTapAction_SelectedIndexChanged(null, null);
            leftIndexTipTapAction_SelectedIndexChanged(null, null);
            leftCustom1Action_SelectedIndexChanged(null, null);
            rightPinchAction_SelectedIndexChanged(null, null);
            rightThumbPressAction_SelectedIndexChanged(null, null);
            rightIndexBendAction_SelectedIndexChanged(null, null);
            rightFingerGunAction_SelectedIndexChanged(null, null);
            rightSqueezeAction_SelectedIndexChanged(null, null);
            rightWristTapAction_SelectedIndexChanged(null, null);
            rightPalmTapAction_SelectedIndexChanged(null, null);
            rightCustom1Action_SelectedIndexChanged(null, null);
            interactionProfile_SelectedIndexChanged(null, null);

            pinchNear_Scroll(null, null);
            pinchFar_Scroll(null, null);
            thumbPressNear_Scroll(null, null);
            thumbPressFar_Scroll(null, null);
            indexBendNear_Scroll(null, null);
            indexBendFar_Scroll(null, null);
            fingerGunNear_Scroll(null, null);
            fingerGunFar_Scroll(null, null);
            squeezeNear_Scroll(null, null);
            squeezeFar_Scroll(null, null);
            wristTapNear_Scroll(null, null);
            wristTapFar_Scroll(null, null);
            palmTapNear_Scroll(null, null);
            palmTapFar_Scroll(null, null);
            indexTipTapNear_Scroll(null, null);
            indexTipTapFar_Scroll(null, null);
            clickThreshold_Scroll(null, null);
            custom1Joint1_SelectedIndexChanged(null, null);
            custom1Joint2_SelectedIndexChanged(null, null);
            custom1Near_Scroll(null, null);
            custom1Far_Scroll(null, null);
        }

        private void ParseVec(TrackBar X, TrackBar Y, TrackBar Z, string action)
        {
            var values = action.Split(' ');
            X.Value = (int)Math.Round(Double.Parse(values[0]) * 1000);
            Y.Value = (int)Math.Round(Double.Parse(values[1]) * 1000);
            Z.Value = (int)Math.Round(Double.Parse(values[2]) * 1000);
        }
        private void ParseEuler(TrackBar X, TrackBar Y, TrackBar Z, string action)
        {
            var values = action.Split(' ');
            X.Value = Int32.Parse(values[0]);
            Y.Value = Int32.Parse(values[1]);
            Z.Value = Int32.Parse(values[2]);
        }

        private void SelectActionByName(ComboBox box, string action)
        {
            for (int i = 0; i < box.Items.Count; i++)
            {
                if (box.Items[i].ToString().Split(' ')[0] == action)
                {
                    box.SelectedIndex = i;
                    return;
                }
            }

            MessageBox.Show(this, "Action does not exist: " + action, "Error", MessageBoxButtons.OK, MessageBoxIcon.Error);
            box.SelectedItem = 0;
        }

        private void loadMenuItem_Click(object sender, EventArgs e)
        {
            var openFileDialog = new OpenFileDialog
            {
                RestoreDirectory = true,
                Title = "Open a configuration file",
                Filter = "Configuration files (*.cfg)|*.cfg|All files (*.*)|*.*",
                FilterIndex = 0,
                CheckFileExists = true,
                CheckPathExists = true
            };

            if (openFileDialog.ShowDialog() == DialogResult.OK)
            {
                var configFile = new StreamReader(openFileDialog.FileName);

                try
                {
                    while (true)
                    {
                        var line = configFile.ReadLine();
                        if (line == null)
                        {
                            break;
                        }

                        var split = line.Split('=');
                        if (split.Length == 1)
                        {
                            continue;
                        }
                        var name = split[0];
                        var value = split[1];

                        switch (name)
                        {
                            case "left.transform.vec":
                                ParseVec(leftXOffset, leftYOffset, leftZOffset, value);
                                leftXOffset_Scroll(null, null);
                                leftYOffset_Scroll(null, null);
                                leftZOffset_Scroll(null, null);
                                break;
                            case "right.transform.vec":
                                ParseVec(rightXOffset, rightYOffset, rightZOffset, value);
                                rightXOffset_Scroll(null, null);
                                rightYOffset_Scroll(null, null);
                                rightZOffset_Scroll(null, null);
                                break;
                            // Since quaternion have multiple representations in Euler angles, we store the Euler angles (for UI use only). The API layer will use the quaternion instead.
                            case "left.transform.euler":
                                ParseEuler(leftXRotation, leftYRotation, leftZRotation, value);
                                leftXRotation_Scroll(null, null);
                                leftYRotation_Scroll(null, null);
                                leftZRotation_Scroll(null, null);
                                break;
                            case "right.transform.euler":
                                ParseEuler(rightXRotation, rightYRotation, rightZRotation, value);
                                rightXRotation_Scroll(null, null);
                                rightYRotation_Scroll(null, null);
                                rightZRotation_Scroll(null, null);
                                break;
                            case "left.enabled":
                                leftDisable.Checked = !(value == "1" || value == "true");
                                break;
                            case "right.enabled":
                                rightDisable.Checked = !(value == "1" || value == "true");
                                break;
                            case "aim_joint":
                                aimJoint.SelectedIndex = Int32.Parse(value);
                                break;
                            case "grip_joint":
                                gripJoint.SelectedIndex = Int32.Parse(value);
                                break;
                            case "left.pinch":
                                SelectActionByName(leftPinchAction, value);
                                break;
                            case "left.thumb_press":
                                SelectActionByName(leftThumbPressAction, value);
                                break;
                            case "left.index_bend":
                                SelectActionByName(leftIndexBendAction, value);
                                break;
                            case "left.finger_gun":
                                SelectActionByName(leftFingerGunAction, value);
                                break;
                            case "left.squeeze":
                                SelectActionByName(leftSqueezeAction, value);
                                break;
                            case "left.wrist_tap":
                                SelectActionByName(leftWristTapAction, value);
                                break;
                            case "left.palm_tap":
                                SelectActionByName(leftPalmTapAction, value);
                                break;
                            case "left.index_tip_tap":
                                SelectActionByName(leftIndexTipTapAction, value);
                                break;
                            case "left.custom1":
                                SelectActionByName(leftCustom1Action, value);
                                break;
                            case "right.pinch":
                                SelectActionByName(rightPinchAction, value);
                                break;
                            case "right.thumb_press":
                                SelectActionByName(rightThumbPressAction, value);
                                break;
                            case "right.index_bend":
                                SelectActionByName(rightIndexBendAction, value);
                                break;
                            case "right.finger_gun":
                                SelectActionByName(rightFingerGunAction, value);
                                break;
                            case "right.squeeze":
                                SelectActionByName(rightSqueezeAction, value);
                                break;
                            case "right.wrist_tap":
                                SelectActionByName(rightWristTapAction, value);
                                break;
                            case "right.palm_tap":
                                SelectActionByName(rightPalmTapAction, value);
                                break;
                            case "right.custom1":
                                SelectActionByName(rightCustom1Action, value);
                                break;
                            case "interaction_profile":
                                SelectActionByName(interactionProfile, value);
                                break;
                            case "pinch.near":
                                pinchNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "pinch.far":
                                pinchFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "thumb_press.near":
                                thumbPressNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "thumb_press.far":
                                thumbPressFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "index_bend.near":
                                indexBendNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "index_bend.far":
                                indexBendFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "finger_gun.near":
                                fingerGunNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "finger_gun.far":
                                fingerGunFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "squeeze.near":
                                squeezeNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "squeeze.far":
                                squeezeFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "wrist_tap.near":
                                wristTapNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "wrist_tap.far":
                                wristTapFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "palm_tap.near":
                                palmTapNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "palm_tap.far":
                                palmTapFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "index_tip_tap.near":
                                indexTipTapNear.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "index_tip_tap.far":
                                indexTipTapFar.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "click_threshold":
                                clickThreshold.Value = (int)Math.Round(Double.Parse(value) * 100);
                                break;
                            case "custom1_joint1":
                                custom1Joint1.SelectedIndex = 1 + Int32.Parse(value);
                                break;
                            case "custom1_joint2":
                                custom1Joint2.SelectedIndex = 1 + Int32.Parse(value);
                                break;
                            case "custom1.near":
                                custom1Near.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                            case "custom1.far":
                                custom1Far.Value = (int)Math.Round(Double.Parse(value) * 1000);
                                break;
                        }

                        FlushConfiguration();
                    }

                    status.Text = "Loaded from " + openFileDialog.FileName;
                }
                finally
                {
                    configFile.Close();
                }
            }
        }

        private void saveMenuItem_Click(object sender, EventArgs e)
        {
            var saveFileDialog = new SaveFileDialog
            {
                RestoreDirectory = true,
                Title = "Save a configuration file",
                Filter = "Configuration files (*.cfg)|*.cfg",
                FilterIndex = 0,
            };

            if (saveFileDialog.ShowDialog() == DialogResult.OK)
            {
                // This is kinda hacky but will do the job. We modify SendUpdate() to write to file instead of sending to the application.
                saveToFile = new StreamWriter(saveFileDialog.FileName);

                try
                {
                    FlushConfiguration();
                    status.Text = "Saved to " + saveFileDialog.FileName;
                }
                finally
                {
                    saveToFile.Close();
                    saveToFile = null;
                }
            }
        }

        private void flushMenuItem_Click(object sender, EventArgs e)
        {
            FlushConfiguration();
            status.Text = "Pushed all settings";
        }

        private void restoreMenuItem_Click(object sender, EventArgs e)
        {
            ResetDefaults();
            FlushConfiguration();
            status.Text = "Restored defaults";
        }
    }
}
