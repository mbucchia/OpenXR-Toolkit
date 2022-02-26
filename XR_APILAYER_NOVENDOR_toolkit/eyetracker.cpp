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

#include "pch.h"

#include "factories.h"
#include "interfaces.h"
#include "layer.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::input;
    using namespace toolkit::utilities;
    using namespace toolkit::log;

    using namespace xr::math;

    class EyeTracker : public IEyeTracker {
      public:
        EyeTracker(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager)
            : m_openXR(openXR), m_configManager(configManager) {
        }

        ~EyeTracker() override {
            endSession();
        }

        void beginSession(XrSession session) override {
            m_session = session;
            m_debugWithController = m_configManager->getValue(SettingEyeDebugWithController);

            // Create a reference space.
            {
                XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
                referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
                CHECK_XRCMD(m_openXR.xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_viewSpace));
            }

            // Create the resources for the eye tracker space.
            {
                XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
                strcpy_s(actionSetCreateInfo.actionSetName, "eye_tracker");
                strcpy_s(actionSetCreateInfo.localizedActionSetName, "Eye Tracker");
                actionSetCreateInfo.priority = 0;
                CHECK_XRCMD(
                    m_openXR.xrCreateActionSet(m_openXR.GetXrInstance(), &actionSetCreateInfo, &m_eyeTrackerActionSet));
            }
            {
                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
                strcpy_s(actionCreateInfo.actionName, "eye_tracker");
                strcpy_s(actionCreateInfo.localizedActionName, "Eye Tracker");
                actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                actionCreateInfo.countSubactionPaths = 0;
                CHECK_XRCMD(m_openXR.xrCreateAction(m_eyeTrackerActionSet, &actionCreateInfo, &m_eyeGazeAction));
            }
            {
                XrActionSuggestedBinding binding;
                binding.action = m_eyeGazeAction;

                XrInteractionProfileSuggestedBinding suggestedBindings{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
                                                                       nullptr};

                if (!m_debugWithController) {
                    CHECK_XRCMD(m_openXR.xrStringToPath(
                        m_openXR.GetXrInstance(), "/user/eyes_ext/input/gaze_ext/pose", &binding.binding));
                    CHECK_XRCMD(m_openXR.xrStringToPath(m_openXR.GetXrInstance(),
                                                        "/interaction_profiles/ext/eye_gaze_interaction",
                                                        &suggestedBindings.interactionProfile));
                } else {
                    // We use a Left HP motion controller to simulate the eye gaze.
                    CHECK_XRCMD(m_openXR.xrStringToPath(
                        m_openXR.GetXrInstance(), "/user/hand/left/input/grip/pose", &binding.binding));
                    CHECK_XRCMD(m_openXR.xrStringToPath(m_openXR.GetXrInstance(),
                                                        "/interaction_profiles/hp/mixed_reality_controller",
                                                        &suggestedBindings.interactionProfile));
                }
                suggestedBindings.suggestedBindings = &binding;
                suggestedBindings.countSuggestedBindings = 1;
                CHECK_XRCMD(m_openXR.xrSuggestInteractionProfileBindings(m_openXR.GetXrInstance(), &suggestedBindings));
            }
            {
                XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO, nullptr};
                actionSpaceCreateInfo.action = m_eyeGazeAction;
                actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
                actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
                CHECK_XRCMD(m_openXR.xrCreateActionSpace(session, &actionSpaceCreateInfo, &m_eyeSpace));
            }
        }

        void endFrame() override {
        }

        void endSession() override {
            if (m_eyeSpace != XR_NULL_HANDLE) {
                m_openXR.xrDestroySpace(m_eyeSpace);
                m_eyeSpace = XR_NULL_HANDLE;
            }
            if (m_eyeGazeAction != XR_NULL_HANDLE) {
                m_openXR.xrDestroyAction(m_eyeGazeAction);
                m_eyeGazeAction = XR_NULL_HANDLE;
            }
            if (m_eyeTrackerActionSet != XR_NULL_HANDLE) {
                m_openXR.xrDestroyActionSet(m_eyeTrackerActionSet);
                m_eyeTrackerActionSet = XR_NULL_HANDLE;
            }
            if (m_viewSpace != XR_NULL_HANDLE) {
                m_openXR.xrDestroySpace(m_viewSpace);
                m_viewSpace = XR_NULL_HANDLE;
            }

            m_session = XR_NULL_HANDLE;
        }

        void beginFrame(XrTime frameTime) {
            m_frameTime = frameTime;
            m_valid = false;
        }

        void update() {
            m_projectionDistance = m_configManager->getValue(SettingEyeProjectionDistance) / 100.f;
        }

        XrActionSet getActionSet() const override {
            return m_eyeTrackerActionSet;
        }

        // TODO: Consider switching to NDC.
        bool getProjectedGaze(float gazeX[ViewCount], float gazeY[ViewCount]) const {
            assert(m_session != XR_NULL_HANDLE);

            if (!m_frameTime) {
                return 0;
            }

            if (!m_valid) {
                // Query the latest eye gaze pose.
                {
                    XrActiveActionSet activeActionSets;
                    activeActionSets.actionSet = m_eyeTrackerActionSet;
                    activeActionSets.subactionPath = XR_NULL_PATH;

                    XrActionsSyncInfo syncInfo{XR_TYPE_ACTIONS_SYNC_INFO, nullptr};
                    syncInfo.activeActionSets = &activeActionSets;
                    syncInfo.countActiveActionSets = 1;
                    CHECK_XRCMD(m_openXR.xrSyncActions(m_session, &syncInfo));
                }
                XrView eyeInViewSpace[2] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
                {
                    // We need the FOVs so we can create a projection matrix.
                    XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO, nullptr};
                    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    locateInfo.space = m_viewSpace;
                    locateInfo.displayTime = m_frameTime;

                    XrViewState state{XR_TYPE_VIEW_STATE, nullptr};
                    uint32_t viewCountOutput;
                    CHECK_HRCMD(
                        m_openXR.xrLocateViews(m_session, &locateInfo, &state, 2, &viewCountOutput, eyeInViewSpace));

                    if (!Pose::IsPoseValid(state.viewStateFlags)) {
                        return false;
                    }
                }
                {
                    XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
                    XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
                    getActionStateInfo.action = m_eyeGazeAction;
                    CHECK_XRCMD(m_openXR.xrGetActionStatePose(m_session, &getActionStateInfo, &actionStatePose));

                    if (!actionStatePose.isActive) {
                        return false;
                    }
                }

                XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};
                CHECK_XRCMD(m_openXR.xrLocateSpace(m_eyeSpace, m_viewSpace, m_frameTime, &location));

                if (!Pose::IsPoseValid(location.locationFlags)) {
                    return false;
                }

                if (m_debugWithController) {
                    location.pose.position.x = location.pose.position.y = location.pose.position.z = 0.f;
                }

                // Project the pose onto the screen.

                // 1) Project the gaze to a 3D point forward. This point is relative to the view space.
                const auto gaze = LoadXrPose(location.pose);
                const auto gazeProjectedPoint =
                    DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, m_projectionDistance, 1), gaze);

                for (uint32_t eye = 0; eye < ViewCount; eye++) {
                    // 2) Compute the view space to camera transform for this eye.
                    const auto cameraProjection = ComposeProjectionMatrix(eyeInViewSpace[eye].fov, {0.001f, 100.f});
                    const auto cameraView = LoadXrPose(eyeInViewSpace[eye].pose);
                    const auto viewToCamera = DirectX::XMMatrixMultiply(cameraProjection, cameraView);

                    // 3) Transform the 3D point to camera space.
                    const auto gazeProjectedInCameraSpace =
                        DirectX::XMVector3Transform(gazeProjectedPoint, viewToCamera);

                    // 4) Project the 3D point in camera space to a 2D point in normalized screen coordinates.
                    XrVector4f point;
                    StoreXrVector4(&point, gazeProjectedInCameraSpace);
                    if (std::abs(point.w) < FLT_EPSILON) {
                        break;
                    }
                    m_gazeX[eye] = ((point.x / point.w) + 1) / 2;
                    m_gazeY[eye] = (1 - (point.y / point.w)) / 2;

                    // Mark as valid if we have both eyes.
                    m_valid = (eye == ViewCount - 1);
                }

                // Update Stats
                // TODO: Consider removing this.
                m_eyeGazeState.origin = location.pose.position;
                {
                    const auto q = location.pose.orientation;
                    const auto sinp = 2 * (q.w * q.y - q.z * q.x);
                    if (std::abs(sinp) >= 1.f) {
                        m_eyeGazeState.yaw = std::copysign((float)M_PI / 2.f, sinp);
                    } else {
                        m_eyeGazeState.yaw = std::asin(sinp);
                    }
                    m_eyeGazeState.yaw *= (float)(180.f / M_PI);
                }
                {
                    const auto q = location.pose.orientation;
                    const auto sinr_cosp = 2 * (q.w * q.x + q.y * q.z);
                    const auto cosr_cosp = 1 - 2 * (q.x * q.x + q.y * q.y);
                    m_eyeGazeState.pitch = std::atan2(sinr_cosp, cosr_cosp);
                    m_eyeGazeState.pitch *= (float)(180.f / M_PI);
                }
            }

            for (uint32_t eye = 0; eye < ViewCount; eye++) {
                gazeX[eye] = m_gazeX[eye];
                gazeY[eye] = m_gazeY[eye];
            }

            return true;
        }

        const EyeGazeState& getEyeGazeState() const override {
            return m_eyeGazeState;
        }

      private:
        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        bool m_debugWithController{false};
        float m_projectionDistance{2.f};

        XrSession m_session{XR_NULL_HANDLE};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        XrActionSet m_eyeTrackerActionSet{XR_NULL_HANDLE};
        XrAction m_eyeGazeAction{XR_NULL_HANDLE};
        XrSpace m_eyeSpace{XR_NULL_HANDLE};
        XrTime m_frameTime{0};

        mutable float m_gazeX[ViewCount];
        mutable float m_gazeY[ViewCount];
        mutable bool m_valid{false};

        mutable EyeGazeState m_eyeGazeState{};
    };

} // namespace

namespace toolkit::input {

    std::shared_ptr<IEyeTracker> CreateEyeTracker(toolkit::OpenXrApi& openXR,
                                                  std::shared_ptr<toolkit::config::IConfigManager> configManager) {
        return std::make_shared<EyeTracker>(openXR, configManager);
    }

} // namespace toolkit::input
