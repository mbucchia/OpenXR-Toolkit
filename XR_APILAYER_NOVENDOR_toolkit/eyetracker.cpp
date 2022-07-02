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
        EyeTrackerBase(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager, EyeTrackerType trackerType)
            : m_openXR(openXR), m_configManager(configManager), m_trackerType(trackerType) {
        }

        ~EyeTrackerBase() override {
            endSession();
        }

        void beginSession(XrSession session) override {
            // Create a reference space.
            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO, nullptr};
            referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
            referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
            CHECK_XRCMD(m_openXR.xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_viewSpace));
            m_session = session;
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

            if (!m_frameTime || m_viewSpace == XR_NULL_HANDLE) {
                return false;
            }

            if (!m_valid) {
                // We need the FOVs so we can create a projection matrix.
                XrView eyeInViewSpace[ViewCount] = {{XR_TYPE_VIEW, nullptr}, {XR_TYPE_VIEW, nullptr}};
                {
                    XrViewLocateInfo locateInfo{XR_TYPE_VIEW_LOCATE_INFO, nullptr};
                    locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                    locateInfo.displayTime = m_frameTime;
                    locateInfo.space = m_viewSpace;
                    
                    auto state = XrViewState{XR_TYPE_VIEW_STATE, nullptr};
                    auto viewCountOutput = ViewCount;
                    CHECK_HRCMD(m_openXR.xrLocateViews(
                        m_session, &locateInfo, &state, viewCountOutput, &viewCountOutput, eyeInViewSpace));

                    if (!Pose::IsPoseValid(state)) {
                        return false;
                    }
                }

                if (!getEyeGaze(m_eyeGazeState.gazeRay)) {
                    return false;
                }

                // Project the pose onto the screen.

                // 1) We have a 3D point (forward) for the gaze. This point is relative to the view space.
                const auto gazeProjectedPoint = DirectX::XMVectorSet(
                    m_eyeGazeState.gazeRay.x, m_eyeGazeState.gazeRay.y, m_eyeGazeState.gazeRay.z, 1.f);

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
                    m_eyeGazeState.gazeNdc[0] = m_gaze[0];
                    m_eyeGazeState.gazeNdc[1] = m_gaze[1];
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

        bool isTrackingThroughRuntime() const override {
            return m_trackerType == EyeTrackerType::OpenXR;
        }

      protected:
        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        float m_projectionDistance{2.f};
        EyeTrackerType m_trackerType{EyeTrackerType::None};
        mutable bool m_valid{false};

        XrSession m_session{XR_NULL_HANDLE};
        XrSpace m_viewSpace{XR_NULL_HANDLE};
        XrTime m_frameTime{0};

        XrActionSet m_eyeTrackerActionSet{XR_NULL_HANDLE};

        mutable XrVector2f m_gaze[ViewCount];
        mutable EyeGazeState m_eyeGazeState{};
    };

    class OpenXrEyeTracker : public EyeTrackerBase {
      public:
        OpenXrEyeTracker(OpenXrApi& openXR, std::shared_ptr<IConfigManager> configManager)
            : EyeTrackerBase(openXR, configManager, EyeTrackerType::OpenXR) {
        }

        ~OpenXrEyeTracker() override {
        }

        void beginSession(XrSession session) override {
            EyeTrackerBase::beginSession(session);

            m_debugWithController = m_configManager->getValue(SettingEyeDebugWithController);

            // Create the resources for the eye tracker space.
            {
                // Create action set
                XrActionSetCreateInfo actionSetCreateInfo{XR_TYPE_ACTION_SET_CREATE_INFO, nullptr};
                strcpy_s(actionSetCreateInfo.actionSetName, "eye_tracker");
                strcpy_s(actionSetCreateInfo.localizedActionSetName, "Eye Tracker");
                actionSetCreateInfo.priority = 0;
                CHECK_XRCMD(
                    m_openXR.xrCreateActionSet(m_openXR.GetXrInstance(), &actionSetCreateInfo, &m_eyeTrackerActionSet));
            }
            {
                // Create user intent action
                XrActionCreateInfo actionCreateInfo{XR_TYPE_ACTION_CREATE_INFO, nullptr};
                strcpy_s(actionCreateInfo.actionName, "eye_tracker");
                strcpy_s(actionCreateInfo.localizedActionName, "Eye Tracker");
                actionCreateInfo.actionType = XR_ACTION_TYPE_POSE_INPUT;
                actionCreateInfo.countSubactionPaths = 0;
                CHECK_XRCMD(m_openXR.xrCreateAction(m_eyeTrackerActionSet, &actionCreateInfo, &m_gazeAction));
            }
            {
                // Create suggested bindings
                XrActionSuggestedBinding binding;
                binding.action = m_gazeAction;

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
                actionSpaceCreateInfo.action = m_gazeAction;
                actionSpaceCreateInfo.subactionPath = XR_NULL_PATH;
                actionSpaceCreateInfo.poseInActionSpace = Pose::Identity();
                CHECK_XRCMD(m_openXR.xrCreateActionSpace(m_session, &actionSpaceCreateInfo, &m_gazeActionSpace));
            }
        }

        void endSession() override {
            if (m_gazeActionSpace != XR_NULL_HANDLE) {
                m_openXR.xrDestroySpace(m_gazeActionSpace);
                m_gazeActionSpace = XR_NULL_HANDLE;
            }
            if (m_gazeAction != XR_NULL_HANDLE) {
                m_openXR.xrDestroyAction(m_gazeAction);
                m_gazeAction = XR_NULL_HANDLE;
            }

            EyeTrackerBase::endSession();
        }

        bool getEyeGaze(XrVector3f& projectedPoint) const override {
            if (m_gazeAction == XR_NULL_HANDLE || m_gazeActionSpace == XR_NULL_HANDLE)
                return false;

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
                getActionStateInfo.action = m_gazeAction;
                CHECK_XRCMD(m_openXR.xrGetActionStatePose(m_session, &getActionStateInfo, &actionStatePose));

                if (!actionStatePose.isActive) {
                    return false;
                }
            }

            XrSpaceLocation gazeLocation{XR_TYPE_SPACE_LOCATION, nullptr};
            CHECK_XRCMD(m_openXR.xrLocateSpace(m_gazeActionSpace, m_viewSpace, m_frameTime, &gazeLocation));

            if (!Pose::IsPoseTracked(gazeLocation)) {
                return false;
            }

            if (!m_debugWithController) {
                static constexpr DirectX::XMVECTORF32 kGazeForward = {{{0, 0, 2, 1}}}; /* 2m forward */
                StoreXrVector3(&projectedPoint,
                               DirectX::XMVector3Transform(kGazeForward, LoadXrPose(gazeLocation.pose)));
            } else {
                projectedPoint = gazeLocation.pose.position;
            }

            return true;
        }

        bool isProjectionDistanceSupported() const {
            return false;
        }

      private:
        bool m_debugWithController{false};
        XrAction m_gazeAction{XR_NULL_HANDLE};
        XrSpace m_gazeActionSpace{XR_NULL_HANDLE};
    };

    class OmniceptEyeTracker : public EyeTrackerBase {
      public:
        OmniceptEyeTracker(OpenXrApi& openXR,
                           std::shared_ptr<IConfigManager> configManager,
                           std::unique_ptr<Client> omniceptClient)
            : EyeTrackerBase(openXR, configManager, EyeTrackerType::Omnicept),
              m_omniceptClient(std::move(omniceptClient)) {
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
            : EyeTrackerBase(openXR, configManager, EyeTrackerType::Pimax) {
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
                                                  std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                  EyeTrackerType trackerType /* = EyeTrackerType::Any */) {
        // For eye tracking, we try to use the Omnicept runtime if it's available.
        if (trackerType == EyeTrackerType::Omnicept || trackerType == EyeTrackerType::Any) {
            if (utilities::IsServiceRunning("HP Omnicept")) {
                using namespace HP::Omnicept;
                try {
                    auto omniceptClientBuilder = Glia::StartBuildClient_Async(
                        "OpenXR-Toolkit",
                        std::move(std::make_unique<Abi::SessionLicense>("", "", Abi::LicensingModel::CORE, false)),
                        [&](const Client::State state) {
                            if (state == Client::State::RUNNING || state == Client::State::PAUSED) {
                                Log("Omnicept client connected\n");
                            } else if (state == Client::State::DISCONNECTED) {
                                Log("Omnicept client disconnected\n");
                            }
                        });

                    if (auto omniceptClient = omniceptClientBuilder->getBuildClientResultOrThrow()) {
                        Log("Detected HP Omnicept support\n");
                        return std::make_shared<OmniceptEyeTracker>(openXR, configManager, std::move(omniceptClient));
                    }

                } catch (const HP::Omnicept::Abi::HandshakeError& e) {
                    Log("Could not connect to Omnicept runtime HandshakeError: %s\n", e.what());
                } catch (const HP::Omnicept::Abi::TransportError& e) {
                    Log("Could not connect to Omnicept runtime TransportError: %s\n", e.what());
                } catch (const HP::Omnicept::Abi::ProtocolError& e) {
                    Log("Could not connect to Omnicept runtime ProtocolError: %s\n", e.what());
                } catch (std::exception& e) {
                    Log("Could not connect to Omnicept runtime: %s\n", e.what());
                }
            }
        }

        // ...and the Pimax eye tracker if available.

        if (trackerType == EyeTrackerType::Pimax || trackerType == EyeTrackerType::Any) {
            XrSystemGetInfo getInfo{XR_TYPE_SYSTEM_GET_INFO};
            getInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
            XrSystemId systemId;
            if (XR_SUCCEEDED(openXR.xrGetSystem(openXR.GetXrInstance(), &getInfo, &systemId))) {
                XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
                CHECK_XRCMD(openXR.xrGetSystemProperties(openXR.GetXrInstance(), systemId, &systemProperties));
                if (contains_string(std::string_view(systemProperties.systemName), "aapvr")) {
                    aSeeVRInitParam param;
                    param.ports[0] = 5777;
                    Log("--> aSeeVR_connect_server\n");
                    auto result = aSeeVR_connect_server(&param);
                    Log("<-- aSeeVR_connect_server\n");
                    if (result == ASEEVR_RETURN_CODE::success) {
                        Log("Detected Pimax Droolon support\n");
                        return std::make_shared<PimaxEyeTracker>(openXR, configManager);
                    }
                }
            }
        }

        // ...otherwise, we will try to fallback to OpenXR.
        //
        // NB: form factor will almost always going to be HMD, so maybe we can just move
        // all those checks up into xrCreateInstance() and check eyeTrackingSystemProperties.supportsEyeGazeInteraction,
        // but at this stage, xrGetSystem() was not called by the app, so we don't know what form factor the app will
        // request (and therefore we did not do all the get system properties etc ourselves).

        if (trackerType == EyeTrackerType::OpenXR || trackerType == EyeTrackerType::Any) {
            return std::make_shared<OpenXrEyeTracker>(openXR, configManager);
        }

        return nullptr;
    }

} // namespace toolkit::input
