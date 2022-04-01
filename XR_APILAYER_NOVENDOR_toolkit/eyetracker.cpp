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

    using namespace HP::Omnicept;

    using namespace xr::math;

    class EyeTracker : public IEyeTracker {
      public:
        EyeTracker(OpenXrApi& openXR,
                   std::shared_ptr<IConfigManager> configManager,
                   std::unique_ptr<Client> omniceptClient)
            : m_openXR(openXR), m_configManager(configManager), m_omniceptClient(std::move(omniceptClient)) {
            if (m_omniceptClient) {
                initializeOmniceptEyeTracking();
            }
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

            if (m_omniceptClient) {
                m_omniceptClient->startClient();
            } else {
                initializeOpenXrEyeTracking();
            }
        }

        void endSession() override {
            if (m_omniceptClient) {
                // TODO: This is occasionally causing a crash... Disabled for now.
                // m_omniceptClient->pauseClient();
            }

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

        void endFrame() override {
        }

        void update() {
            m_projectionDistance = m_configManager->getValue(SettingEyeProjectionDistance) / 100.f;
        }

        XrActionSet getActionSet() const override {
            return m_eyeTrackerActionSet;
        }

        bool getProjectedGaze(XrVector2f gaze[ViewCount]) const {
            assert(m_session != XR_NULL_HANDLE);

            if (!m_frameTime) {
                return false;
            }

            if (!m_valid) {
                // We need the FOVs so we can create a projection matrix.
                XrView eyeInViewSpace[2] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
                {
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

                XrVector3f projectedPoint{};
                const bool hasGaze =
                    m_omniceptClient ? getOmniceptEyeGaze(projectedPoint) : getOpenXrEyeGaze(projectedPoint);
                if (!hasGaze) {
                    return false;
                }

                // Project the pose onto the screen.

                // 1) We have a 3D point (forward) for the gaze. This point is relative to the view space.
                const auto gazeProjectedPoint =
                    DirectX::XMVectorSet(projectedPoint.x, projectedPoint.y, projectedPoint.z, 1.f);

                for (uint32_t eye = 0; eye < ViewCount; eye++) {
                    // 2) Compute the view space to camera transform for this eye.
                    const auto cameraProjection = ComposeProjectionMatrix(eyeInViewSpace[eye].fov, {0.001f, 100.f});
                    const auto cameraView = LoadXrPose(eyeInViewSpace[eye].pose);
                    const auto viewToCamera = DirectX::XMMatrixMultiply(cameraProjection, cameraView);

                    // 3) Transform the 3D point to camera space.
                    const auto gazeProjectedInCameraSpace =
                        DirectX::XMVector3Transform(gazeProjectedPoint, viewToCamera);

                    // 4) Project the 3D point in camera space to a 2D point in normalized device coordinates.
                    XrVector4f point;
                    StoreXrVector4(&point, gazeProjectedInCameraSpace);
                    if (std::abs(point.w) < FLT_EPSILON) {
                        break;
                    }
                    // output NDC (-1,+1)
                    m_gaze[eye].x = point.x / point.w;
                    m_gaze[eye].y = point.y / point.w;

                    // Mark as valid if we have both eyes.
                    m_valid = (eye == ViewCount - 1);
                }
            }

            for (uint32_t eye = 0; eye < ViewCount; eye++) {
                gaze[eye] = m_gaze[eye];
            }

            return true;
        }

        const EyeGazeState& getEyeGazeState() const override {
            return m_eyeGazeState;
        }

      private:
        void initializeOpenXrEyeTracking() {
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
                CHECK_XRCMD(m_openXR.xrCreateActionSpace(m_session, &actionSpaceCreateInfo, &m_eyeSpace));
            }
        }

        bool getOpenXrEyeGaze(XrVector3f projectedPoint) const {
            XrSpaceLocation location{XR_TYPE_SPACE_LOCATION, nullptr};

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
            {
                XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE, nullptr};
                XrActionStateGetInfo getActionStateInfo{XR_TYPE_ACTION_STATE_GET_INFO, nullptr};
                getActionStateInfo.action = m_eyeGazeAction;
                CHECK_XRCMD(m_openXR.xrGetActionStatePose(m_session, &getActionStateInfo, &actionStatePose));

                if (!actionStatePose.isActive) {
                    return false;
                }
            }

            CHECK_XRCMD(m_openXR.xrLocateSpace(m_eyeSpace, m_viewSpace, m_frameTime, &location));

            if (!Pose::IsPoseValid(location.locationFlags)) {
                return false;
            }

            if (m_debugWithController) {
                location.pose.position.x = location.pose.position.y = location.pose.position.z = 0.f;
            }

            const auto gaze = LoadXrPose(location.pose);
            const auto gazeProjectedPoint =
                m_debugWithController
                    ? LoadXrVector3(location.pose.position)
                    : DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, m_projectionDistance, 1), gaze);

            projectedPoint.x = gazeProjectedPoint.m128_f32[0];
            projectedPoint.y = gazeProjectedPoint.m128_f32[1];
            projectedPoint.z = gazeProjectedPoint.m128_f32[2];

            return true;
        }

        void initializeOmniceptEyeTracking() {
            std::shared_ptr<Abi::SubscriptionList> subList = Abi::SubscriptionList::GetSubscriptionListToNone();

            Abi::Subscription eyeTrackingSub = Abi::Subscription::generateSubscriptionForDomainType<Abi::EyeTracking>();
            subList->getSubscriptions().push_back(eyeTrackingSub);

            m_omniceptClient->setSubscriptions(*subList);
        }

        bool getOmniceptEyeGaze(XrVector3f& projectedPoint) const {
            Client::LastValueCached<Abi::EyeTracking> lvc = m_omniceptClient->getLastData<Abi::EyeTracking>();
            if (!lvc.valid || lvc.data.combinedGazeConfidence < 0.5f) {
                return false;
            }

            projectedPoint.x = -lvc.data.combinedGaze.x;
            projectedPoint.y = lvc.data.combinedGaze.y;
            projectedPoint.z = -lvc.data.combinedGaze.z;

            return true;
        }

        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::unique_ptr<Client> m_omniceptClient;
        bool m_debugWithController{false};
        float m_projectionDistance{2.f};

        XrSession m_session{XR_NULL_HANDLE};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        XrTime m_frameTime{0};

        XrActionSet m_eyeTrackerActionSet{XR_NULL_HANDLE};
        XrAction m_eyeGazeAction{XR_NULL_HANDLE};
        XrSpace m_eyeSpace{XR_NULL_HANDLE};

        mutable XrVector2f m_gaze[ViewCount];
        mutable bool m_valid{false};
        mutable EyeGazeState m_eyeGazeState{};
    };

} // namespace

namespace toolkit::input {
    std::shared_ptr<IEyeTracker> CreateEyeTracker(toolkit::OpenXrApi& openXR,
                                                  std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                  std::unique_ptr<Client> omniceptClient) {
        return std::make_shared<EyeTracker>(openXR, configManager, std::move(omniceptClient));
    }

} // namespace toolkit::input
