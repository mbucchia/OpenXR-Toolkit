// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
// Copyright(c) 2021-2022 Jean-Luc Dupiot - Reality XP
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
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::menu;
    using namespace toolkit::log;
    using namespace toolkit::utilities;

    using namespace xr::math;

    constexpr double KeyRepeat = 0.2;

    constexpr uint32_t ColorDefault = 0xffffffff;
    constexpr uint32_t ColorSelected = 0xff0099ff;
    constexpr uint32_t ColorWarning = 0xff0000ff;

    enum class MenuState { Splash, NotVisible, Visible };
    enum class MenuEntryType { Slider, Choice, Separator, RestoreDefaultsButton, ExitButton };

    struct MenuEntry {
        std::string title;
        MenuEntryType type;
#define BUTTON_OR_SEPARATOR "", 0, 0, [](int value) { return ""; }
        std::string configName;
        int minValue;
        int maxValue;
        std::function<std::string(int)> valueToString;

        bool visible{true};
    };

    struct MenuGroup {
        size_t start;
        size_t end;
    };

    // The logic of our menus.
    class MenuHandler : public IMenuHandler {
      public:
        MenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                    std::shared_ptr<IDevice> device,
                    uint32_t displayWidth,
                    uint32_t displayHeight,
                    bool isHandTrackingSupported,
                    bool isPredictionDampeningSupported)
            : m_configManager(configManager), m_device(device), m_displayWidth(displayWidth),
              m_displayHeight(displayHeight), m_isHandTrackingSupported(isHandTrackingSupported) {
            m_lastInput = std::chrono::steady_clock::now();

            // We display the hint for menu hotkeys for the first few runs.
            int firstRun = m_configManager->getValue("first_run");
            if (firstRun <= 10) {
                m_numSplashLeft = 10 - firstRun;
                m_state = MenuState::Splash;
                m_configManager->setValue("first_run", firstRun + 1);
            }

            // TODO: Add menu entries here.
            m_menuEntries.push_back({"Overlay",
                                     MenuEntryType::Choice,
                                     SettingOverlayType,
                                     0,
                                     (int)OverlayType::MaxValue - (m_configManager->isExperimentalMode() ? 1 : 2),
                                     [](int value) {
                                         std::string labels[] = {"Off", "FPS", "Detailed"};
                                         return labels[value];
                                     }});
            m_configManager->setEnumDefault(SettingOverlayType, OverlayType::None);
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.push_back({"Upscaling",
                                     MenuEntryType::Choice,
                                     SettingScalingType,
                                     0,
                                     (int)ScalingType::MaxValue - 1,
                                     [](int value) {
                                         std::string labels[] = {"Off", "NIS", "FSR"};
                                         return labels[value];
                                     }});
            m_upscalingGroup.start = m_menuEntries.size();
            m_originalScalingType = getCurrentScalingType();
            m_menuEntries.push_back({"Factor", MenuEntryType::Slider, SettingScaling, 100, 200, [&](int value) {
                                         // We don't even use value, the utility function below will query it.
                                         const auto& resolution =
                                             GetScaledDimensions(m_displayWidth, m_displayHeight, value, 2);
                                         return fmt::format("{}% ({}x{})", value, resolution.first, resolution.second);
                                     }});
            m_originalScalingValue = getCurrentScaling();
            m_menuEntries.push_back({"Sharpness", MenuEntryType::Slider, SettingSharpness, 0, 100, [](int value) {
                                         return fmt::format("{}%", value);
                                     }});
            m_upscalingGroup.end = m_menuEntries.size();
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});

            // The unit for ICD is tenth of millimeters.
            m_menuEntries.push_back({"ICD (World Scale)", MenuEntryType::Slider, SettingICD, 1, 10000, [](int value) {
                                         return fmt::format("{}mm", value / 10.0f);
                                     }});
            m_menuEntries.push_back({"FOV",
                                     MenuEntryType::Slider,
                                     SettingFOV,
                                     50,
                                     150,
                                     [](int value) { return fmt::format("{}%", value); },
                                     m_configManager->isExperimentalMode()});
            m_menuEntries.push_back({"Prediction dampening",
                                     MenuEntryType::Slider,
                                     SettingPredictionDampen,
                                     0,
                                     200,
                                     [&](int value) {
                                         if (value == 100) {
                                             return fmt::format("{}%", value);
                                         } else {
                                             return fmt::format(
                                                 "{}% (of {:.1f}ms)", value, m_stats.predictionTimeUs / 1000000.0f);
                                         }
                                     },
                                     isPredictionDampeningSupported});
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});

            m_menuEntries.push_back({"Hand Tracking",
                                     MenuEntryType::Choice,
                                     SettingHandTrackingEnabled,
                                     0,
                                     (int)HandTrackingEnabled::MaxValue - 1,
                                     [](int value) {
                                         std::string labels[] = {"Off", "Both", "Left", "Right"};
                                         return labels[value];
                                     },
                                     isHandTrackingSupported});
            m_originalHandTrackingEnabled = isHandTrackingEnabled();
            m_handTrackingGroup.start = m_menuEntries.size();
            m_menuEntries.push_back(
                {"Hand Visibility",
                 MenuEntryType::Slider,
                 SettingHandVisibilityAndSkinTone,
                 0,
                 4,
                 [](int value) {
                     std::string labels[] = {
                         "Hidden", "Visible - Bright", "Visible - Medium", "Visible - Dark", "Visible - Darker"};
                     return labels[value];
                 },
                 isHandTrackingSupported});
            m_handTrackingGroup.end = m_menuEntries.size();
            if (isHandTrackingSupported) {
                m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            }

            m_menuEntries.push_back({"Font size",
                                     MenuEntryType::Choice,
                                     SettingMenuFontSize,
                                     0,
                                     (int)MenuFontSize::MaxValue - 1,
                                     [](int value) {
                                         std::string labels[] = {"Small", "Medium", "Large"};
                                         return labels[value];
                                     }});
            m_configManager->setEnumDefault(SettingMenuFontSize, MenuFontSize::Medium);
            m_menuEntries.push_back({"Menu timeout",
                                     MenuEntryType::Choice,
                                     SettingMenuTimeout,
                                     0,
                                     (int)MenuTimeout::MaxValue - 1,
                                     [](int value) {
                                         std::string labels[] = {"Short", "Medium", "Long"};
                                         return labels[value];
                                     }});
            m_configManager->setEnumDefault(SettingMenuTimeout, MenuTimeout::Medium);
            m_menuEntries.push_back(
                {"Menu eye offset", MenuEntryType::Slider, SettingOverlayEyeOffset, -500, 500, [](int value) {
                     return fmt::format("{}px", value);
                 }});
            m_menuEntries.push_back({"Restore defaults", MenuEntryType::RestoreDefaultsButton, BUTTON_OR_SEPARATOR});
            m_menuEntries.push_back({"Exit menu", MenuEntryType::ExitButton, BUTTON_OR_SEPARATOR});
        }

        void handleInput() override {
            const auto now = std::chrono::steady_clock::now();

            // Check whether this is a long press and the event needs to be repeated.
            const double keyRepeat = GetAsyncKeyState(VK_SHIFT) ? KeyRepeat / 10 : KeyRepeat;
            const bool repeat = std::chrono::duration<double>(now - m_lastInput).count() > keyRepeat;

            const bool moveLeft = UpdateKeyState(m_moveLeftKeyState, VK_CONTROL, VK_F1, repeat);
            const bool moveRight = UpdateKeyState(m_moveRightKeyState, VK_CONTROL, VK_F3, repeat);
            const bool menuControl = UpdateKeyState(m_menuControlKeyState, VK_CONTROL, VK_F2, repeat);

            if (menuControl) {
                if (m_state != MenuState::Visible) {
                    m_state = MenuState::Visible;

                    m_needRestart = checkNeedRestartCondition();
                    m_menuEntriesTitleWidth = 0.0f;
                    m_menuEntriesRight = m_menuEntriesBottom = 0.0f;
                } else {
                    do {
                        m_selectedItem = (m_selectedItem + 1) % m_menuEntries.size();
                    } while (m_menuEntries[m_selectedItem].type == MenuEntryType::Separator ||
                             !m_menuEntries[m_selectedItem].visible);
                }

                m_lastInput = now;
            }

            if (m_state == MenuState::Visible && (moveLeft || moveRight)) {
                const auto& menuEntry = m_menuEntries[m_selectedItem];

                switch (menuEntry.type) {
                case MenuEntryType::Separator:
                    break;

                case MenuEntryType::RestoreDefaultsButton:
                    m_configManager->resetToDefaults();
                    break;

                case MenuEntryType::ExitButton:
                    m_selectedItem = 0;
                    m_state = MenuState::NotVisible;
                    break;

                default:
                    const int value = m_configManager->peekValue(menuEntry.configName);
                    const int newValue =
                        std::clamp(value + (moveLeft ? -1 : 1), menuEntry.minValue, menuEntry.maxValue);

                    // When changing the upscaling, people might immediately exit VR to test the change. Bypass the
                    // commit delay.
                    const bool noCommitDelay =
                        menuEntry.configName == SettingScalingType || menuEntry.configName == SettingScaling;

                    m_configManager->setValue(menuEntry.configName, newValue, noCommitDelay);

                    // When changing some settings, display the warning that the session must be restarted.
                    const bool wasRestartNeeded = std::exchange(m_needRestart, checkNeedRestartCondition());

                    // When changing the font size or displaying the restart banner, force re-alignment/re-size.
                    if (menuEntry.configName == SettingMenuFontSize || wasRestartNeeded != m_needRestart) {
                        m_menuEntriesTitleWidth = 0.0f;
                        m_menuEntriesRight = m_menuEntriesBottom = 0.0f;
                    }

                    break;
                }

                m_lastInput = now;
            }

            if (menuControl && moveLeft && moveRight) {
                m_configManager->hardReset();
                m_state = MenuState::Splash;
                m_selectedItem = 0;
            }

            handleGroups();
        }

        // Show/hide subgroups based on the current config.
        void handleGroups() {
            auto updateGroupVisibility = [&](MenuGroup& group, bool visible) {
                for (size_t i = group.start; i < group.end; i++) {
                    m_menuEntries[i].visible = visible;
                }
            };

            updateGroupVisibility(m_upscalingGroup, getCurrentScalingType() != ScalingType::None);
            updateGroupVisibility(m_handTrackingGroup, isHandTrackingEnabled());
        }

        void calibrate(const XrPosef& poseLeft,
                       const XrFovf& fovLeft,
                       const XrSwapchainCreateInfo& leftImageInfo,
                       const XrPosef& poseRight,
                       const XrFovf& fovRight,
                       const XrSwapchainCreateInfo& rightImageInfo) override {
            // Project a single point 1m in front of the head from both eyes to clip space and compute the offset. Seems
            // to work well.
            const XrVector3f worldPos3 =
                Pose::Multiply(poseLeft, Pose::Translation(XrVector3f{0.0f, 0.0f, 1.0f})).position;
            const XrVector4f worldPos4 = XrVector4f{worldPos3.x, worldPos3.y, worldPos3.z, 1.0f};
            const DirectX::XMVECTOR worldPos = LoadXrVector4(worldPos4);
            auto projectPoint = [&worldPos](
                                    const XrPosef& eyePose, const XrFovf& fov, const XrSwapchainCreateInfo& imageInfo) {
                const DirectX::XMMATRIX projMatrix = ComposeProjectionMatrix(fov, NearFar{0.001f, 100.0f});
                const DirectX::XMVECTOR viewPos = DirectX::XMVector4Transform(worldPos, LoadInvertedXrPose(eyePose));
                const DirectX::XMVECTOR clipPos = DirectX::XMVector4Transform(viewPos, projMatrix);
                XrVector4f clipPos4;
                StoreXrVector4(&clipPos4, clipPos);
                XrVector2f screenPos{clipPos4.x / clipPos4.w, clipPos4.y / clipPos4.w};
                return XrVector2f{screenPos.x * imageInfo.width, screenPos.y * imageInfo.height};
            };

            const auto offset =
                projectPoint(poseRight, fovRight, rightImageInfo) - projectPoint(poseLeft, fovLeft, leftImageInfo);

            m_configManager->setDefault(SettingOverlayEyeOffset, (int)offset.x);
        }

        void render(uint32_t eye, const XrPosef& pose, std::shared_ptr<ITexture> renderTarget) const override {
            assert(eye == 0 || eye == 1);

            const float leftEyeOffset = 0.0f;
            const float rightEyeOffset = (float)m_configManager->getValue(SettingOverlayEyeOffset);
            const float eyeOffset = eye ? rightEyeOffset : leftEyeOffset;

            const float leftAlign = (renderTarget->getInfo().width / 4.0f) + eyeOffset;
            const float rightAlign = (2 * renderTarget->getInfo().width / 3.0f) + eyeOffset;
            const float topAlign = renderTarget->getInfo().height / 3.0f;

            const float fontSizes[(int)MenuFontSize::MaxValue] = {
                renderTarget->getInfo().height * 0.0075f,
                renderTarget->getInfo().height * 0.015f,
                renderTarget->getInfo().height * 0.02f,
            };
            const float fontSize = fontSizes[m_configManager->getValue(SettingMenuFontSize)];

            const double timeouts[(int)MenuTimeout::MaxValue] = {3.0, 10.0, 60.0};
            const double timeout =
                m_state == MenuState::Splash ? 10.0 : timeouts[m_configManager->getValue(SettingMenuTimeout)];

            const auto now = std::chrono::steady_clock::now();
            const auto duration = std::chrono::duration<double>(now - m_lastInput).count();

            // Apply menu fade.
            const auto alpha = (unsigned int)(std::clamp(timeout - duration, 0.0, 1.0) * 255.0);
            const auto colorNormal = (ColorDefault & 0xffffff) | (alpha << 24);
            const auto colorSelected = (ColorSelected & 0xffffff) | (alpha << 24);
            const auto colorWarning = (ColorWarning & 0xffffff) | (alpha << 24);

            // Leave upon timeout.
            if (duration >= timeout) {
                m_state = MenuState::NotVisible;
            }

            if (m_state == MenuState::Splash) {
                m_device->drawString(
                    fmt::format("Press CTRL+F2 to bring up the menu ({}s)", (int)(std::ceil(timeout - duration))),
                    TextStyle::Normal,
                    fontSize,
                    leftAlign,
                    topAlign,
                    colorSelected);

                m_device->drawString(fmt::format("(this message will be displayed {} more time{})",
                                                 m_numSplashLeft,
                                                 m_numSplashLeft == 1 ? "" : "s"),
                                     TextStyle::Normal,
                                     fontSize * 0.75f,
                                     leftAlign,
                                     topAlign + 1.05f * fontSize,
                                     colorSelected);
            } else if (m_state == MenuState::Visible) {
                const bool measure = m_menuEntriesRight == 0.0f;
                if (!measure) {
                    XrColor4f background({0.0f, 0.0f, 0.0f, 1.0f});
                    m_device->clearColor(
                        topAlign, leftAlign, m_menuEntriesBottom, m_menuEntriesRight + eyeOffset, background);
                }

                float top = topAlign;
                float left = leftAlign;

                left += m_device->drawString(L"\xE112 : CTRL+F1   \xE1FC : CTRL+F2   \xE111 : CTRL+F3",
                                             TextStyle::Normal,
                                             fontSize,
                                             leftAlign,
                                             top,
                                             colorNormal,
                                             measure);
                top += 1.05f * fontSize;
                m_device->drawString(
                    L"Use SHIFT to scroll faster", TextStyle::Normal, fontSize, leftAlign, top, colorNormal, measure);
                top += 1.5f * fontSize;

                if (eye == 0) {
                    m_menuEntriesRight = max(m_menuEntriesRight, left);
                }

                float menuEntriesTitleWidth = m_menuEntriesTitleWidth;

                // Display each menu entry.
                for (unsigned int i = 0; i < m_menuEntries.size(); i++) {
                    left = leftAlign;
                    const auto& menuEntry = m_menuEntries[i];

                    // Always account for entries, even invisible ones, when calculating the alignment.
                    float entryWidth = 0.0f;
                    if (menuEntriesTitleWidth == 0.0f) {
                        // Worst case should be Selected (bold).
                        entryWidth = m_device->measureString(menuEntry.title, TextStyle::Bold, fontSize) + 50;
                        m_menuEntriesTitleWidth = max(m_menuEntriesTitleWidth, entryWidth);
                    }

                    if (!menuEntry.visible) {
                        continue;
                    }

                    const auto entryStyle = i == m_selectedItem ? TextStyle::Bold : TextStyle::Normal;
                    const auto entryColor = i == m_selectedItem ? colorSelected : colorNormal;

                    m_device->drawString(menuEntry.title, entryStyle, fontSize, left, top, entryColor);
                    if (menuEntriesTitleWidth == 0.0f) {
                        left += entryWidth;
                    } else {
                        left += menuEntriesTitleWidth;
                    }

                    const int value = m_configManager->peekValue(menuEntry.configName);

                    // Display the current value.
                    switch (menuEntry.type) {
                    case MenuEntryType::Slider:
                        left += m_device->drawString(
                            menuEntry.valueToString(value), entryStyle, fontSize, left, top, entryColor, measure);
                        break;

                    case MenuEntryType::Choice:
                        for (int j = menuEntry.minValue; j <= menuEntry.maxValue; j++) {
                            const std::string label = menuEntry.valueToString(j);

                            const auto valueStyle = j == value ? TextStyle::Bold : TextStyle::Normal;
                            const auto valueColor = j == value ? colorSelected : colorNormal;

                            left +=
                                m_device->drawString(label, valueStyle, fontSize, left, top, valueColor, true) + 20.0f;
                        }
                        break;

                    case MenuEntryType::Separator:
                        // Counteract the auto-down and add our own spacing.
                        top -= 1.05f * fontSize;
                        top += fontSize / 3;
                        break;

                    case MenuEntryType::RestoreDefaultsButton:
                        break;

                    case MenuEntryType::ExitButton:
                        if (duration > 1.0) {
                            left += m_device->drawString(fmt::format("({}s)", (int)(std::ceil(timeout - duration))),
                                                         entryStyle,
                                                         fontSize,
                                                         left,
                                                         top,
                                                         entryColor,
                                                         measure);
                        }
                        break;
                    }

                    top += 1.05f * fontSize;

                    if (eye == 0) {
                        m_menuEntriesRight = max(m_menuEntriesRight, left);
                    }
                }

                {
                    float left = leftAlign;
                    if (m_configManager->isSafeMode()) {
                        top += fontSize;

                        left += m_device->drawString(
                            L"\x26A0  Running in safe mode, settings are cleared when restarting the VR session \x26A0",
                            TextStyle::Bold,
                            fontSize,
                            left,
                            top,
                            colorWarning,
                            measure);

                        top += 1.05f * fontSize;
                    } else if (m_needRestart) {
                        top += fontSize;

                        left += m_device->drawString(L"\x26A0  Some settings require to restart the VR session \x26A0",
                                                     TextStyle::Bold,
                                                     fontSize,
                                                     left,
                                                     top,
                                                     colorWarning,
                                                     measure);

                        top += 1.05f * fontSize;
                    }

                    if (eye == 0) {
                        m_menuEntriesRight = max(m_menuEntriesRight, left);
                    }
                }
                m_menuEntriesBottom = top + fontSize * 0.2f;
            }

            auto overlayType = m_configManager->getEnumValue<OverlayType>(SettingOverlayType);
            if (overlayType != OverlayType::None) {
                float top = topAlign;

#define OVERLAY_COMMON TextStyle::Normal, fontSize, rightAlign, top, ColorSelected, true

                m_device->drawString(fmt::format("FPS: {}", m_stats.fps), OVERLAY_COMMON);
                top += 1.05f * fontSize;

                // Advanced displasy.
                if (overlayType == OverlayType::Advanced) {
                    m_device->drawString(fmt::format("app CPU: {}", m_stats.appCpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_device->drawString(fmt::format("app GPU: {}", m_stats.appGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_device->drawString(fmt::format("lay CPU: {}", m_stats.endFrameCpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_device->drawString(fmt::format("pre GPU: {}", m_stats.preProcessorGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_device->drawString(fmt::format("scl GPU: {}", m_stats.upscalerGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_device->drawString(fmt::format("pst GPU: {}", m_stats.postProcessorGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_device->drawString(fmt::format("ovl CPU: {}", m_stats.overlayCpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_device->drawString(fmt::format("ovl GPU: {}", m_stats.overlayGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                }
#undef OVERLAY_COMMON
            }
        }

        void updateStatistics(const LayerStatistics& stats) override {
            m_stats = stats;
        }

      private:
        ScalingType getCurrentScalingType() const {
            return m_configManager->getEnumValue<ScalingType>(SettingScalingType);
        }

        bool isHandTrackingEnabled() const {
            return m_isHandTrackingSupported && m_configManager->getEnumValue<HandTrackingEnabled>(
                                                    SettingHandTrackingEnabled) != HandTrackingEnabled::Off;
        }

        uint32_t getCurrentScaling() const {
            return m_configManager->getValue(SettingScaling);
        }

        bool checkNeedRestartCondition() const {
            if (m_originalHandTrackingEnabled != isHandTrackingEnabled() ||
                m_originalScalingType != getCurrentScalingType()) {
                return true;
            }

            if (m_originalScalingType != ScalingType::None) {
                return m_originalScalingValue != getCurrentScaling();
            }

            return false;
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_displayWidth;
        const uint32_t m_displayHeight;
        const bool m_isHandTrackingSupported;
        LayerStatistics m_stats{};

        int m_numSplashLeft;
        std::vector<MenuEntry> m_menuEntries;
        unsigned int m_selectedItem{0};
        std::chrono::steady_clock::time_point m_lastInput;
        bool m_moveLeftKeyState{false};
        bool m_moveRightKeyState{false};
        bool m_menuControlKeyState{false};

        MenuGroup m_upscalingGroup;
        MenuGroup m_handTrackingGroup;

        uint32_t m_originalScalingValue{0};
        ScalingType m_originalScalingType{ScalingType::None};
        bool m_originalHandTrackingEnabled{false};
        bool m_needRestart{false};

        mutable MenuState m_state{MenuState::NotVisible};
        mutable float m_menuEntriesTitleWidth{0.0f};
        mutable float m_menuEntriesRight{0.0f};
        mutable float m_menuEntriesBottom{0.0f};
    };

} // namespace

namespace toolkit::menu {

    std::shared_ptr<IMenuHandler> CreateMenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                    std::shared_ptr<toolkit::graphics::IDevice> device,
                                                    uint32_t displayWidth,
                                                    uint32_t displayHeight,
                                                    bool isHandTrackingSupported,
                                                    bool isPredictionDampeningSupported) {
        return std::make_shared<MenuHandler>(configManager,
                                             device,
                                             displayWidth,
                                             displayHeight,
                                             isHandTrackingSupported,
                                             isPredictionDampeningSupported);
    }

} // namespace toolkit::menu
