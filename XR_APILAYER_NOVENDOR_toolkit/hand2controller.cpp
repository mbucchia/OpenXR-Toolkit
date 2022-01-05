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
    using namespace toolkit::graphics;
    using namespace toolkit::input;
    using namespace toolkit::log;

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

    class HandTracker : public IHandTracker {
      public:
        HandTracker(OpenXrApi& openXR,
                    XrSession session,
                    std::shared_ptr<IConfigManager> configManager,
                    std::shared_ptr<IDevice> graphicsDevice)
            : m_openXR(openXR), m_configManager(configManager), m_graphicsDevice(graphicsDevice) {
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

            std::vector<SimpleMeshVertex> vertices;
            copyFromArray(vertices, c_cubeVertices);
            std::vector<uint16_t> indices;
            copyFromArray(indices, c_cubeIndices);
            m_jointMesh = m_graphicsDevice->createSimpleMesh(vertices, indices, "Joint Mesh");
        }

        ~HandTracker() override {
            xrDestroyHandTrackerEXT(m_handTracker[0]);
            xrDestroyHandTrackerEXT(m_handTracker[1]);
        }

        XrPath getInteractionProfile() const {
            return XR_NULL_PATH;
        }

        void registerAction(XrAction action, XrActionSet actionSet) override {
        }

        void unregisterAction(XrAction action) override {
        }

        void registerActionSpace(XrSpace space, const std::string path, const XrPosef& poseInActionSpace) override {
        }

        void unregisterActionSpace(XrSpace space) override {
        }

        void registerBindings(const XrInteractionProfileSuggestedBinding& bindings) override {
        }

        const std::string getFullPath(XrAction action, XrPath subactionPath) override {
            return {};
        }

        void sync(XrTime frameTime) override {
            m_thisFrameTime = frameTime;
        }

        bool locate(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation& location) const override {
            return false;
        }

        void render(const XrPosef& pose,
                    XrSpace baseSpace,
                    std::shared_ptr<graphics::ITexture> renderTarget) const override {
            for (uint32_t hand = 0; hand < 2; hand++) {
                XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
                locateInfo.baseSpace = baseSpace;
                locateInfo.time = m_thisFrameTime;

                XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT];
                XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
                locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
                locations.jointLocations = jointLocations;

                CHECK_XRCMD(xrLocateHandJointsEXT(m_handTracker[hand], &locateInfo, &locations));
                for (uint32_t joint = 0; joint < XR_HAND_JOINT_COUNT_EXT; joint++) {
                    if (!xr::math::Pose::IsPoseValid(locations.jointLocations[joint].locationFlags)) {
                        continue;
                    }

                    XrVector3f scaling{locations.jointLocations[joint].radius,
                                       min(0.0025f, locations.jointLocations[joint].radius),
                                       max(0.015f, locations.jointLocations[joint].radius)};
                    m_graphicsDevice->draw(m_jointMesh, locations.jointLocations[joint].pose, scaling);
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
        const OpenXrApi& m_openXR;
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_graphicsDevice;

        XrHandTrackerEXT m_handTracker[2]{XR_NULL_HANDLE, XR_NULL_HANDLE};
        XrTime m_thisFrameTime{0};
        std::shared_ptr<ISimpleMesh> m_jointMesh;

        // TODO: These should be auto-generated and accessible via OpenXrApi.
        PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT{nullptr};
        PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT{nullptr};
        PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT{nullptr};
    };

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
