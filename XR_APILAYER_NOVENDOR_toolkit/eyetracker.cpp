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

    class EyeTrackerBase : public IEyeTracker {
      public:
        EyeTrackerBase(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager)
            : m_openXR(openXR), m_configManager(configManager) {
        }

        ~EyeTrackerBase() override {
            endSession();
        }

        void beginSession(XrSession session) override {
            m_session = session;

            // Create a reference space.
            {
                XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
                referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
                referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
                CHECK_XRCMD(m_openXR.xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_viewSpace));
            }
        }

        void endSession() override {
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

        void beginFrame(XrTime frameTime) override {
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

        virtual bool getEyeGaze(XrVector3f& projectedPoint) const = 0;

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
                if (!getEyeGaze(projectedPoint)) {
                    return false;
                }

                m_eyeGazeState.gazeRay = projectedPoint;

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

                if (m_valid) {
                    m_eyeGazeState.leftPoint.x = m_gaze[0].x;
                    m_eyeGazeState.leftPoint.y = m_gaze[0].y;
                    m_eyeGazeState.rightPoint.x = m_gaze[1].x;
                    m_eyeGazeState.rightPoint.y = m_gaze[1].y;
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

      protected:
        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        float m_projectionDistance{2.f};

        XrSession m_session{XR_NULL_HANDLE};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        XrTime m_frameTime{0};

        XrActionSet m_eyeTrackerActionSet{XR_NULL_HANDLE};

        mutable XrVector2f m_gaze[ViewCount];
        mutable bool m_valid{false};
        mutable EyeGazeState m_eyeGazeState{};
    };

    class OpenXrEyeTracker : public EyeTrackerBase {
      public:
        OpenXrEyeTracker(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager)
            : EyeTrackerBase(openXR, configManager) {
        }

        ~OpenXrEyeTracker() override {
        }

        void beginSession(XrSession session) override {
            EyeTrackerBase::beginSession(session);

            m_debugWithController = m_configManager->getValue(SettingEyeDebugWithController);

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

        void endSession() override {
            if (m_eyeSpace != XR_NULL_HANDLE) {
                m_openXR.xrDestroySpace(m_eyeSpace);
                m_eyeSpace = XR_NULL_HANDLE;
            }
            if (m_eyeGazeAction != XR_NULL_HANDLE) {
                m_openXR.xrDestroyAction(m_eyeGazeAction);
                m_eyeGazeAction = XR_NULL_HANDLE;
            }

            EyeTrackerBase::endSession();
        }

        bool getEyeGaze(XrVector3f& projectedPoint) const override {
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
                    : DirectX::XMVector3Transform(DirectX::XMVectorSet(0, 0, 2, 1) /* 2m forward */, gaze);

            projectedPoint.x = gazeProjectedPoint.m128_f32[0];
            projectedPoint.y = gazeProjectedPoint.m128_f32[1];
            projectedPoint.z = gazeProjectedPoint.m128_f32[2];

            return true;
        }

        bool isProjectionDistanceSupported() const {
            return false;
        }

      private:
        bool m_debugWithController{false};
        XrAction m_eyeGazeAction{XR_NULL_HANDLE};
        XrSpace m_eyeSpace{XR_NULL_HANDLE};
    };

    class OmniceptEyeTracker : public EyeTrackerBase {
      public:
        OmniceptEyeTracker(OpenXrApi& openXR,
                           std::shared_ptr<IConfigManager> configManager,
                           std::unique_ptr<Client> omniceptClient)
            : EyeTrackerBase(openXR, configManager), m_omniceptClient(std::move(omniceptClient)) {
            std::shared_ptr<Abi::SubscriptionList> subList = Abi::SubscriptionList::GetSubscriptionListToNone();

            Abi::Subscription eyeTrackingSub = Abi::Subscription::generateSubscriptionForDomainType<Abi::EyeTracking>();
            subList->getSubscriptions().push_back(eyeTrackingSub);

            m_omniceptClient->setSubscriptions(*subList);
        }

        ~OmniceptEyeTracker() override {
            endSession();
        }

        void beginSession(XrSession session) override {
            EyeTrackerBase::beginSession(session);

            m_omniceptClient->startClient();
        }

        void endSession() override {
            if (m_omniceptClient) {
                // TODO: This is occasionally causing a crash... Disabled for now.
                // m_omniceptClient->pauseClient();
            }

            EyeTrackerBase::endSession();
        }

        bool getEyeGaze(XrVector3f& projectedPoint) const override {
            Client::LastValueCached<Abi::EyeTracking> lvc = m_omniceptClient->getLastData<Abi::EyeTracking>();
            if (!lvc.valid || lvc.data.combinedGazeConfidence < 0.5f) {
                return false;
            }

            projectedPoint.x = -lvc.data.combinedGaze.x;
            projectedPoint.y = lvc.data.combinedGaze.y;
            projectedPoint.z = -lvc.data.combinedGaze.z;

            return true;
        }

        bool isProjectionDistanceSupported() const {
            return false;
        }

      private:
        const std::unique_ptr<Client> m_omniceptClient;
    };

    class PimaxEyeTracker : public EyeTrackerBase {
      public:
        PimaxEyeTracker(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager)
            : EyeTrackerBase(openXR, configManager) {
            aSeeVR_register_callback(aSeeVRCallbackType::state, stateCallback, this);
            aSeeVR_register_callback(aSeeVRCallbackType::eye_data, eyeDataCallback, this);
            aSeeVR_register_callback(aSeeVRCallbackType::coefficient, getCoefficientCallback, this);
        }

        ~PimaxEyeTracker() override {
            endSession();
        }

        void beginSession(XrSession session) override {
            EyeTrackerBase::beginSession(session);

            const auto status = aSeeVR_get_coefficient();
            if (status != ASEEVR_RETURN_CODE::success) {
                Log("aSeeVR_get_coefficient failed with: %d\n", status);
            }
        }

        void endSession() override {
            aSeeVR_stop();

            EyeTrackerBase::endSession();
        }

        bool getEyeGaze(XrVector3f& projectedPoint) const override {
            if (!m_isDeviceReady) {
                return false;
            }

            // TODO: Use timestamp to implement a timeout.

            // We assume the point is projected onto a screen at Z=-1
            projectedPoint.x = m_recommendedGaze.x - 0.5f;
            projectedPoint.y = m_recommendedGaze.y - 0.5f;
            projectedPoint.z = -m_projectionDistance;

            return true;
        }

        bool isProjectionDistanceSupported() const {
            return true;
        }

      private:
        void setCoefficients(const aSeeVRCoefficient& coefficients) {
            m_coefficients = coefficients;
            const auto status = aSeeVR_start(&m_coefficients);
            if (status == ASEEVR_RETURN_CODE::success) {
                m_isDeviceReady = true;
            } else {
                Log("aSeeVR_start failed with: %d\n", status);
            }
        }

        void setEyeData(int64_t timestamp, float recommendedGazeX, float recommendedGazeY) {
            m_recommendedGaze = {recommendedGazeX, recommendedGazeY};
            m_lastTimestamp = timestamp;
        }

        aSeeVRCoefficient m_coefficients;
        bool m_isDeviceReady{false};
        XrVector2f m_recommendedGaze{0, 0};
        int64_t m_lastTimestamp{0};

        static void _7INVENSUN_CALL stateCallback(const aSeeVRState* state, void* context) {
            switch (state->code) {
            case aSeeVRStateCode::api_start:
                Log("aSeeVR_start completed with: %d\n", state->error);
                break;

            case aSeeVRStateCode::api_stop:
                Log("aSeeVR_stop completed with: %d\n", state->error);
                break;

            default:
                break;
            }
        }

        static void _7INVENSUN_CALL eyeDataCallback(const aSeeVREyeData* eyeData, void* context) {
            if (!eye_data) {
                return;
            }

            PimaxEyeTracker* tracker = reinterpret_cast<PimaxEyeTracker*>(context);

            int64_t timestamp = 0;
            aSeeVR_get_int64(eyeData, aSeeVREye::undefine_eye, aSeeVREyeDataItemType::timestamp, &timestamp);

            aSeeVRPoint2D point2D = {0};
            aSeeVR_get_point2d(eyeData, aSeeVREye::undefine_eye, aSeeVREyeDataItemType::gaze, &point2D);

            tracker->setEyeData(timestamp, point2D.x, point2D.y);
        }

        static void _7INVENSUN_CALL getCoefficientCallback(const aSeeVRCoefficient* data, void* context) {
            PimaxEyeTracker* tracker = reinterpret_cast<PimaxEyeTracker*>(context);
            tracker->setCoefficients(*data);
        }
    };

} // namespace

namespace toolkit::input {
    std::shared_ptr<IEyeTracker> CreateEyeTracker(toolkit::OpenXrApi& openXR,
                                                  std::shared_ptr<toolkit::config::IConfigManager> configManager) {
        return std::make_shared<OpenXrEyeTracker>(openXR, configManager);
    }

    std::shared_ptr<IEyeTracker>
    CreateOmniceptEyeTracker(toolkit::OpenXrApi& openXR,
                             std::shared_ptr<toolkit::config::IConfigManager> configManager,
                             std::unique_ptr<Client> omniceptClient) {
        return std::make_shared<OmniceptEyeTracker>(openXR, configManager, std::move(omniceptClient));
    }

    std::shared_ptr<IEyeTracker> CreatePimaxEyeTracker(toolkit::OpenXrApi& openXR,
                                                       std::shared_ptr<toolkit::config::IConfigManager> configManager) {
        return std::make_shared<PimaxEyeTracker>(openXR, configManager);
    }

} // namespace toolkit::input
