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

namespace toolkit {

    // The path where the DLL loads config files and stores logs.
    extern std::string dllHome;

} // namespace toolkit

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::input;
    using namespace toolkit::log;

    using namespace xr::math;

    constexpr XrVector3f Bright1{255 / 255.f, 219 / 255.f, 172 / 255.f};

    // Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
    constexpr XrVector3f LBB{-0.5f, -0.5f, -0.5f};
    constexpr XrVector3f LBF{-0.5f, -0.5f, 0.5f};
    constexpr XrVector3f LTB{-0.5f, 0.5f, -0.5f};
    constexpr XrVector3f LTF{-0.5f, 0.5f, 0.5f};
    constexpr XrVector3f RBB{0.5f, -0.5f, -0.5f};
    constexpr XrVector3f RBF{0.5f, -0.5f, 0.5f};
    constexpr XrVector3f RTB{0.5f, 0.5f, -0.5f};
    constexpr XrVector3f RTF{0.5f, 0.5f, 0.5f};

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR)                                                                       \
    {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V5, COLOR}, {V6, COLOR},

    constexpr SimpleMeshVertex c_cubeVertices[] = {
        CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, Bright1) // -X
        CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Bright1) // +X
        CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, Bright1) // -Y
        CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Bright1) // +Y
        CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, Bright1) // -Z
        CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Bright1) // +Z
    };

    constexpr unsigned short c_cubeIndices[] = {
        0,  1,  2,  3,  4,  5,  // -X
        6,  7,  8,  9,  10, 11, // +X
        12, 13, 14, 15, 16, 17, // -Y
        18, 19, 20, 21, 22, 23, // +Y
        24, 25, 26, 27, 28, 29, // -Z
        30, 31, 32, 33, 34, 35, // +Z
    };

    template <typename T, size_t N>
    void copyFromArray(std::vector<T>& targetVector, const T (&sourceArray)[N]) {
        targetVector.assign(sourceArray, sourceArray + N);
    }

    static constexpr uint32_t HandCount = 2;

    static constexpr XrTime GracePeriod = 2000000; // 2ms

    enum class Hand : uint32_t { Left, Right };
    enum class PoseType { Grip, Aim };

    struct ActionSpace {
        Hand hand;
        PoseType poseType;
        XrPosef poseInActionSpace;
    };

    struct Config {
        Config();

        void ParseConfigurationStatement(const std::string& line, unsigned int lineNumber = 1);
        bool LoadConfiguration(const std::string& configName);
        void Dump();

        std::string interactionProfile;
        bool leftHandEnabled;
        bool rightHandEnabled;

        // The skin tone to use for rendering the hand, 0=bright to 2=dark.
        int skinTone;

        // The opacity (alpha channel) for the hand mesh.
        float opacity;

        // The index of the joint (see enum XrHandJointEXT) to use for the aim pose.
        int aimJointIndex;

        // The index of the joint (see enum XrHandJointEXT) to use for the grip pose.
        int gripJointIndex;

        // The threshold (between 0 and 1) when converting a float action into a boolean action and the action is
        // true.
        float clickThreshold;

        // The transformation to apply to the aim and grip poses.
        XrPosef transform[HandCount];

        // The index of the 1st joint (see enum XrHandJointEXT) to use for 1st custom gesture.
        int custom1Joint1Index;

        // The index of the 2nd joint (see enum XrHandJointEXT) to use for 1st custom gesture.
        int custom1Joint2Index;

        // The target XrAction path for a given gesture, and the near/far threshold to map the float action too
        // (near maps to 1, far maps to 0).
#define DEFINE_ACTION(configName)                                                                                      \
    std::string configName##Action[HandCount];                                                                         \
    float configName##Near;                                                                                            \
    float configName##Far;

        DEFINE_ACTION(pinch);
        DEFINE_ACTION(thumbPress);
        DEFINE_ACTION(indexBend);
        DEFINE_ACTION(fingerGun);
        DEFINE_ACTION(squeeze);
        DEFINE_ACTION(palmTap);
        DEFINE_ACTION(wristTap);
        DEFINE_ACTION(indexTipTap);
        DEFINE_ACTION(custom1);

#undef DEFINE_ACTION
    };

    class HandTracker : public IHandTracker {
      public:
        HandTracker(OpenXrApi& openXR,
                    XrSession session,
                    std::shared_ptr<IConfigManager> configManager,
                    std::shared_ptr<IDevice> graphicsDevice)
            : m_openXR(openXR), m_configManager(configManager), m_graphicsDevice(graphicsDevice) {
            // Open a socket for live configuration.
            {
                WSADATA wsaData;
                WSAStartup(MAKEWORD(2, 2), &wsaData);
                m_configSocket = socket(AF_INET, SOCK_DGRAM, 0);
                if (m_configSocket != INVALID_SOCKET) {
                    int one = 1;
                    setsockopt(m_configSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&one, sizeof(one));
                    u_long mode = 1; // 1 to enable non-blocking socket
                    ioctlsocket(m_configSocket, FIONBIO, &mode);
                }

                struct sockaddr_in saddr;
                saddr.sin_family = AF_INET;
                saddr.sin_addr.s_addr = INADDR_ANY;
                saddr.sin_port = htons(10001);
                if (m_configSocket == INVALID_SOCKET ||
                    bind(m_configSocket, (const struct sockaddr*)&saddr, sizeof(saddr))) {
                    Log("Failed to create or bind configuration socket\n");
                }
            }

            // Load file configuration.
            m_config.LoadConfiguration(openXR.GetApplicationName());
            m_config.Dump();

            CHECK_HRCMD(openXR.xrStringToPath(
                openXR.GetXrInstance(), m_config.interactionProfile.c_str(), &m_interactionProfile));

            CHECK_XRCMD(openXR.xrGetInstanceProcAddr(openXR.GetXrInstance(),
                                                     "xrCreateHandTrackerEXT",
                                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrCreateHandTrackerEXT)));
            CHECK_XRCMD(openXR.xrGetInstanceProcAddr(openXR.GetXrInstance(),
                                                     "xrDestroyHandTrackerEXT",
                                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrDestroyHandTrackerEXT)));
            CHECK_XRCMD(openXR.xrGetInstanceProcAddr(openXR.GetXrInstance(),
                                                     "xrLocateHandJointsEXT",
                                                     reinterpret_cast<PFN_xrVoidFunction*>(&xrLocateHandJointsEXT)));

            XrHandTrackerCreateInfoEXT leftTrackerCreateInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
            leftTrackerCreateInfo.hand = XR_HAND_LEFT_EXT;
            leftTrackerCreateInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
            XrHandTrackerCreateInfoEXT rightTrackerCreateInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
            rightTrackerCreateInfo.hand = XR_HAND_RIGHT_EXT;
            rightTrackerCreateInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;

            CHECK_XRCMD(xrCreateHandTrackerEXT(session, &leftTrackerCreateInfo, &m_handTracker[0]));
            CHECK_XRCMD(xrCreateHandTrackerEXT(session, &rightTrackerCreateInfo, &m_handTracker[1]));

            XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
            referenceSpaceCreateInfo.poseInReferenceSpace = Pose::Identity();
            CHECK_XRCMD(m_openXR.xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &m_referenceSpace));

            std::vector<SimpleMeshVertex> vertices;
            copyFromArray(vertices, c_cubeVertices);
            std::vector<uint16_t> indices;
            copyFromArray(indices, c_cubeIndices);
            m_jointMesh = m_graphicsDevice->createSimpleMesh(vertices, indices, "Joint Mesh");
        }

        ~HandTracker() override {
            m_openXR.xrDestroySpace(m_referenceSpace);
            xrDestroyHandTrackerEXT(m_handTracker[0]);
            xrDestroyHandTrackerEXT(m_handTracker[1]);
            closesocket(m_configSocket);
        }

        XrPath getInteractionProfile() const {
            return m_interactionProfile;
        }

        void registerAction(XrAction action, XrActionSet actionSet) override {
        }

        void unregisterAction(XrAction action) override {
        }

        void registerActionSpace(XrSpace space, const std::string path, const XrPosef& poseInActionSpace) override {
            ActionSpace actionSpace;

            if (path.find("/user/hand/left") == 0) {
                actionSpace.hand = Hand::Left;
            } else if (path.find("/user/hand/right") == 0) {
                actionSpace.hand = Hand::Right;
            } else {
                assert(false);
            }

            if (path.find("/input/aim/pose") != std::string::npos) {
                actionSpace.poseType = PoseType::Aim;
            } else if (path.find("/input/grip/pose") != std::string::npos) {
                actionSpace.poseType = PoseType::Grip;
            } else {
                assert(false);
            }

            actionSpace.poseInActionSpace = poseInActionSpace;

            m_actionSpaces.insert_or_assign(space, actionSpace);
        }

        void unregisterActionSpace(XrSpace space) override {
            m_actionSpaces.erase(space);
        }

        void registerBindings(const XrInteractionProfileSuggestedBinding& bindings) override {
        }

        const std::string getFullPath(XrAction action, XrPath subactionPath) override {
            return {};
        }

        void sync(XrTime frameTime) override {
            // Arbitrarily choose this place to handle configuration input.
            if (m_configSocket != INVALID_SOCKET) {
                struct sockaddr_in saddr;
                while (true) {
                    char buffer[100] = {};
                    int slen = sizeof(saddr);
                    const int len =
                        recvfrom(m_configSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&saddr, &slen);
                    if (len <= 0) {
                        break;
                    }

                    std::string line(buffer);
                    m_config.ParseConfigurationStatement(line);
                }
            }

            // Delete outdated entries from the cache.
            for (auto& spaceCache : m_cachedHandJointPoses) {
                auto& cache = spaceCache.second;
                for (uint32_t side = 0; side < HandCount; side++) {
                    while (cache[side].size() > 0 && cache[side].front().first + GracePeriod < frameTime) {
                        cache[side].pop_front();
                    }
                }
            }

            m_thisFrameTime = frameTime;
        }

        bool locate(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation& location) const override {
            const auto actionSpaceIt = m_actionSpaces.find(space);
            if (actionSpaceIt == m_actionSpaces.cend()) {
                return false;
            }

            const auto& actionSpace = actionSpaceIt->second;

            if ((actionSpace.hand == Hand::Left && !m_config.leftHandEnabled) ||
                (actionSpace.hand == Hand::Right && !m_config.rightHandEnabled)) {
                return false;
            }

            const auto& jointPoses = getCachedHandJointPoses(actionSpace.hand, time, baseSpace);

            const uint32_t side = actionSpace.hand == Hand::Left ? 0 : 1;
            const uint32_t joint =
                actionSpace.poseType == PoseType::Grip ? m_config.gripJointIndex : m_config.aimJointIndex;

            // Translate the hand poses for the requested joint to a controller pose.
            location.locationFlags = jointPoses[joint].locationFlags;
            location.pose = Pose::Multiply(actionSpace.poseInActionSpace,
                                           Pose::Multiply(m_config.transform[side], jointPoses[joint].pose));

            return true;
        }

        void render(const XrPosef& pose,
                    XrSpace baseSpace,
                    std::shared_ptr<graphics::ITexture> renderTarget) const override {
            // TODO: Support skin tone.
            // TODO: Support opacity.
            for (uint32_t hand = 0; hand < HandCount; hand++) {
                const auto& jointPoses =
                    getCachedHandJointPoses(hand ? Hand::Right : Hand::Left, m_thisFrameTime, baseSpace);

                for (uint32_t joint = 0; joint < XR_HAND_JOINT_COUNT_EXT; joint++) {
                    if (!xr::math::Pose::IsPoseValid(jointPoses[joint].locationFlags)) {
                        continue;
                    }

                    XrVector3f scaling{jointPoses[joint].radius,
                                       min(0.0025f, jointPoses[joint].radius),
                                       max(0.015f, jointPoses[joint].radius)};
                    m_graphicsDevice->draw(m_jointMesh, jointPoses[joint].pose, scaling);
                }
            }
        }

        bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateBoolean& state) const override {
            return false;
        }

        bool getActionState(const XrActionStateGetInfo& getInfo, XrActionStateFloat& state) const override {
            return false;
        }

      private:
        const XrHandJointLocationEXT* getCachedHandJointPoses(Hand hand,
                                                              XrTime time,
                                                              std::optional<XrSpace> baseSpace) const {
            const uint32_t side = hand == Hand::Left ? 0 : 1;

            auto& cache = m_cachedHandJointPoses[baseSpace.value_or(m_referenceSpace)][side];

            // Search for a entry in the cache.
            int32_t closestIndex = -1;
            auto insertIt = cache.begin();
            XrTime closestTimeDelta = INT64_MAX;
            for (uint32_t i = 0; i < cache.size(); i++) {
                const auto t = cache[i].first;
                const XrTime delta = std::abs(t - time);
                if (t < time) {
                    insertIt++;
                }

                if (delta < closestTimeDelta) {
                    closestTimeDelta = delta;
                    closestIndex = i;
                } else {
                    break;
                }
            }

            if (closestIndex != -1 && closestTimeDelta < GracePeriod) {
                return cache[closestIndex].second;
            }

            // Create a new entry.
            {
                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = baseSpace.value_or(m_referenceSpace);
                locateInfo.time = time;

                CacheEntry entry;
                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
                locations.jointLocations = entry.second;
                entry.first = time;

                CHECK_HRCMD(xrLocateHandJointsEXT(m_handTracker[side], &locateInfo, &locations));
                return cache.emplace(insertIt, entry)->second;
            }
        }

        OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_graphicsDevice;

        XrSpace m_referenceSpace{XR_NULL_HANDLE};
        using CacheEntry = std::pair<XrTime, XrHandJointLocationEXT[XR_HAND_JOINT_COUNT_EXT]>;
        mutable std::map<XrSpace, std::deque<CacheEntry>[2]> m_cachedHandJointPoses;

        Config m_config;
        SOCKET m_configSocket{INVALID_SOCKET};
        XrPath m_interactionProfile{XR_NULL_PATH};

        XrHandTrackerEXT m_handTracker[2]{XR_NULL_HANDLE, XR_NULL_HANDLE};
        XrTime m_thisFrameTime{0};
        std::shared_ptr<ISimpleMesh> m_jointMesh;

        std::map<XrSpace, ActionSpace> m_actionSpaces;

        // TODO: These should be auto-generated and accessible via OpenXrApi.
        PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT{nullptr};
        PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT{nullptr};
        PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT{nullptr};
    };

    Config::Config() {
        interactionProfile = "/interaction_profiles/hp/mixed_reality_controller";
        leftHandEnabled = true;
        rightHandEnabled = true;
        skinTone = 1; // Medium
        opacity = 1.0f;
        aimJointIndex = XR_HAND_JOINT_INDEX_INTERMEDIATE_EXT;
        gripJointIndex = XR_HAND_JOINT_PALM_EXT;
        clickThreshold = 0.75f;
        transform[0] = transform[1] = Pose::Identity();
        pinchAction[0] = pinchAction[1] = "/input/trigger/value";
        pinchNear = 0.0f;
        pinchFar = 0.05f;
        thumbPressAction[0] = thumbPressAction[1] = "";
        thumbPressNear = 0.0f;
        thumbPressFar = 0.05f;
        indexBendAction[0] = indexBendAction[1] = "";
        indexBendNear = 0.045f;
        indexBendFar = 0.07f;
        fingerGunAction[0] = fingerGunAction[1] = "";
        fingerGunNear = 0.01f;
        fingerGunFar = 0.03f;
        squeezeAction[0] = squeezeAction[1] = "/input/squeeze/value";
        squeezeNear = 0.035f;
        squeezeFar = 0.07f;
        palmTapAction[0] = palmTapAction[1] = "";
        palmTapNear = 0.02f;
        palmTapFar = 0.06f;
        wristTapAction[0] = "/input/menu/click";
        wristTapAction[1] = "";
        wristTapNear = 0.04f;
        wristTapFar = 0.05f;
        // This gesture only makes sense for one hand, but we leave it symmetrical for simplicity.
        indexTipTapAction[0] = "/input/b/click";
        indexTipTapAction[1] = "";
        indexTipTapNear = 0.0f;
        indexTipTapFar = 0.07f;
        // Custom gesture is unconfigured.
        custom1Action[0] = custom1Action[1] = "";
        custom1Joint1Index = -1;
        custom1Joint2Index = -1;
        custom1Near = 0.0f;
        custom1Far = 0.1f;
    }

    void Config::ParseConfigurationStatement(const std::string& line, unsigned int lineNumber) {
        try {
            const auto offset = line.find('=');
            if (offset != std::string::npos) {
                const std::string name = line.substr(0, offset);
                const std::string value = line.substr(offset + 1);
                std::string subName;
                int side = -1;

                if (line.substr(0, 5) == "left.") {
                    side = 0;
                    subName = name.substr(5);
                } else if (line.substr(0, 6) == "right.") {
                    side = 1;
                    subName = name.substr(6);
                }

                if (name == "interaction_profile") {
                    interactionProfile = value;
                } else if (name == "skin_tone") {
                    skinTone = std::stoi(value);
                } else if (name == "opacity") {
                    opacity = std::stof(value);
                } else if (name == "aim_joint") {
                    aimJointIndex = std::stoi(value);
                } else if (name == "grip_joint") {
                    gripJointIndex = std::stoi(value);
                } else if (name == "custom1_joint1") {
                    custom1Joint1Index = std::stoi(value);
                } else if (name == "custom1_joint2") {
                    custom1Joint2Index = std::stoi(value);
                } else if (name == "click_threshold") {
                    clickThreshold = std::stof(value);
                } else if (side >= 0 && subName == "enabled") {
                    const bool boolValue = value == "1" || value == "true";
                    if (side == 0) {
                        leftHandEnabled = boolValue;
                    } else {
                        rightHandEnabled = boolValue;
                    }
                } else if (side >= 0 && subName == "transform.vec") {
                    std::stringstream ss(value);
                    std::string component;
                    std::getline(ss, component, ' ');
                    transform[side].position.x = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].position.y = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].position.z = std::stof(component);
                } else if (side >= 0 && subName == "transform.quat") {
                    std::stringstream ss(value);
                    std::string component;
                    std::getline(ss, component, ' ');
                    transform[side].orientation.x = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].orientation.y = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].orientation.z = std::stof(component);
                    std::getline(ss, component, ' ');
                    transform[side].orientation.w = std::stof(component);
                } else if (side >= 0 && subName == "transform.euler") {
                    // For UI use only.
                }
#define PARSE_ACTION(configString, configName)                                                                         \
    else if (side >= 0 && subName == configString) {                                                                   \
        configName##Action[side] = value;                                                                              \
    }                                                                                                                  \
    else if (name == configString ".near") {                                                                           \
        configName##Near = std::stof(value);                                                                           \
    }                                                                                                                  \
    else if (name == configString ".far") {                                                                            \
        configName##Far = std::stof(value);                                                                            \
    }

                PARSE_ACTION("pinch", pinch)
                PARSE_ACTION("thumb_press", thumbPress)
                PARSE_ACTION("index_bend", indexBend)
                PARSE_ACTION("finger_gun", fingerGun)
                PARSE_ACTION("squeeze", squeeze)
                PARSE_ACTION("palm_tap", palmTap)
                PARSE_ACTION("wrist_tap", wristTap)
                PARSE_ACTION("index_tip_tap", indexTipTap)
                PARSE_ACTION("custom1", custom1)

#undef PARSE_ACTION
                else {
                    Log("L%u: Unrecognized option\n", lineNumber);
                }
            } else {
                Log("L%u: Improperly formatted option\n", lineNumber);
            }
        } catch (...) {
            Log("L%u: Parsing error\n", lineNumber);
        }
    }

    bool Config::LoadConfiguration(const std::string& configName) {
        // TODO: Look in %AppData% first.
        std::ifstream configFile(std::filesystem::path(dllHome) / std::filesystem::path(configName + ".cfg"));
        if (configFile.is_open()) {
            Log("Loading config for \"%s\"\n", configName.c_str());

            unsigned int lineNumber = 0;
            std::string line;
            while (std::getline(configFile, line)) {
                lineNumber++;
                ParseConfigurationStatement(line, lineNumber);
            }
            configFile.close();

            return true;
        }

        Log("Could not load config for \"%s\"\n", configName.c_str());

        return false;
    }

    void Config::Dump() {
        Log("Emulating interaction profile: %s\n", interactionProfile.c_str());
        Log("Using %s skin tone and %.3f opacity\n",
            skinTone == 0   ? "bright"
            : skinTone == 1 ? "medium"
                            : "dark",
            opacity);
        if (leftHandEnabled) {
            Log("Left transform: (%.3f, %.3f, %.3f) (%.3f, %.3f, %.3f, %.3f)\n",
                transform[0].position.x,
                transform[0].position.y,
                transform[0].position.z,
                transform[0].orientation.x,
                transform[0].orientation.y,
                transform[0].orientation.z,
                transform[0].orientation.w);
        }
        if (rightHandEnabled) {
            Log("Right transform: (%.3f, %.3f, %.3f) (%.3f, %.3f, %.3f, %.3f)\n",
                transform[1].position.x,
                transform[1].position.y,
                transform[1].position.z,
                transform[1].orientation.x,
                transform[1].orientation.y,
                transform[1].orientation.z,
                transform[1].orientation.w);
        }
        if (leftHandEnabled || rightHandEnabled) {
            Log("Grip pose uses joint: %d\n", gripJointIndex);
            Log("Aim pose uses joint: %d\n", aimJointIndex);
            Log("Click threshold: %.3f\n", clickThreshold);
        }
        if (custom1Joint1Index >= 0 && custom1Joint2Index >= 0) {
            Log("Custom gesture uses joints: %d %d\n", custom1Joint1Index, custom1Joint2Index);
        }
        for (int side = 0; side < HandCount; side++) {
            if ((side == 0 && !leftHandEnabled) || (side == 1 && !rightHandEnabled)) {
                continue;
            }

#define LOG_IF_SET(actionName, configName)                                                                             \
    if (!configName##Action[side].empty()) {                                                                           \
        Log("%s hand " #actionName " translates to: %s (near: %.3f, far: %.3f)\n",                                     \
            side ? "Right" : "Left",                                                                                   \
            configName##Action[side].c_str(),                                                                          \
            configName##Near,                                                                                          \
            configName##Far);                                                                                          \
    }

            LOG_IF_SET("pinch", pinch);
            LOG_IF_SET("thumb press", thumbPress);
            LOG_IF_SET("index bend", indexBend);
            LOG_IF_SET("finger gun", fingerGun);
            LOG_IF_SET("squeeze", squeeze);
            LOG_IF_SET("palm tap", palmTap);
            LOG_IF_SET("wrist tap", wristTap);
            LOG_IF_SET("index tip tap", indexTipTap);
            LOG_IF_SET("custom gesture", custom1);

#undef LOG_IF_SET
        }
    }

} // namespace

namespace toolkit::input {

    std::shared_ptr<input::IHandTracker>
    CreateHandTracker(toolkit::OpenXrApi& openXR,
                      XrSession session,
                      std::shared_ptr<toolkit::config::IConfigManager> configManager,
                      std::shared_ptr<toolkit::graphics::IDevice> graphicsDevice) {
        return std::make_shared<HandTracker>(openXR, session, configManager, graphicsDevice);
    }

} // namespace toolkit::input
