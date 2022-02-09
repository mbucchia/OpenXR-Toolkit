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

#include "layer.h"
#include "factories.h"
#include "interfaces.h"
#include "log.h"

namespace {

    using namespace toolkit;
    using namespace toolkit::config;
    using namespace toolkit::graphics;
    using namespace toolkit::input;
    using namespace toolkit::menu;
    using namespace toolkit::log;
    using namespace toolkit::utilities;

    using namespace xr::math;

    constexpr auto KeyRepeatDelay = 200ms;

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
        int acceleration{1};
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
                    std::vector<int>& keyModifiers,
                    bool isHandTrackingSupported,
                    bool isPredictionDampeningSupported)
            : m_configManager(configManager), m_device(device), m_displayWidth(displayWidth),
              m_displayHeight(displayHeight), m_keyModifiers(keyModifiers),
              m_isHandTrackingSupported(isHandTrackingSupported) {
            m_lastInput = std::chrono::steady_clock::now();

            // We display the hint for menu hotkeys for the first few runs.
            int firstRun = m_configManager->getValue("first_run");
            if (firstRun <= 10) {
                m_numSplashLeft = 10 - firstRun;
                m_state = MenuState::Splash;
                m_configManager->setValue("first_run", firstRun + 1);
            }

            if (std::count(m_keyModifiers.cbegin(), m_keyModifiers.cend(), VK_CONTROL)) {
                m_keyModifiersLabel += L"CTRL+";
            }
            if (std::count(m_keyModifiers.cbegin(), m_keyModifiers.cend(), VK_MENU)) {
                m_keyModifiersLabel += L"ALT+";
            }
            m_configManager->setDefault("key_left", m_keyLeft);
            m_configManager->setDefault("key_right", m_keyRight);
            m_configManager->setDefault("key_menu", m_keyMenu);
            m_keyLeft = m_configManager->getValue("key_left");
            m_keyLeftLabel = keyToString(m_keyLeft);
            m_keyRight = m_configManager->getValue("key_right");
            m_keyRightLabel = keyToString(m_keyRight);
            m_keyMenu = m_configManager->getValue("key_menu");
            m_keyMenuLabel = keyToString(m_keyMenu);

            // Add menu entries below.

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
            m_menuEntries.push_back({"Factor", MenuEntryType::Slider, SettingScaling, 25, 400, [&](int value) {
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
            m_menuEntries.push_back({"World scale", MenuEntryType::Slider, SettingICD, 1, 10000, [&](int value) {
                                         return fmt::format("{:.1f}% ({:.1f}mm)", value / 10.f, m_stats.icd * 1000);
                                     }});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({"FOV",
                                     MenuEntryType::Slider,
                                     SettingFOV,
                                     50,
                                     150,
                                     [](int value) { return fmt::format("{}%", value); },
                                     m_configManager->isExperimentalMode()});
            m_menuEntries.push_back(
                {"Prediction dampening",
                 MenuEntryType::Slider,
                 SettingPredictionDampen,
                 0,
                 200,
                 [&](int value) {
                     if (value == 100) {
                         return fmt::format("{}%", value - 100);
                     } else {
                         return fmt::format("{}% (of {:.1f}ms)", value - 100, m_stats.predictionTimeUs / 1000000.0f);
                     }
                 },
                 isPredictionDampeningSupported});
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});

            m_menuEntries.push_back({"Hand tracking",
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
                {"Hands visibility",
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
            m_menuEntries.push_back({"Hands timeout",
                                     MenuEntryType::Slider,
                                     SettingHandTimeout,
                                     0,
                                     60,
                                     [](int value) -> std::string {
                                         if (value == 0) {
                                             return "Always on";
                                         } else {
                                             return fmt::format("{}s", value);
                                         }
                                     },
                                     isHandTrackingSupported});
            m_handTrackingGroup.end = m_menuEntries.size();
            if (isHandTrackingSupported) {
                m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            }

            m_menuEntries.push_back({"Font size",
                                     MenuEntryType::Slider,
                                     SettingMenuFontSize,
                                     0,
                                     (int)MenuFontSize::MaxValue - 1,
                                     [](int value) {
                                         std::string labels[] = {"Small", "Medium", "Large"};
                                         return labels[value];
                                     }});
            m_configManager->setEnumDefault(SettingMenuFontSize, MenuFontSize::Medium);
            m_menuEntries.push_back({"Menu timeout",
                                     MenuEntryType::Slider,
                                     SettingMenuTimeout,
                                     0,
                                     (int)MenuTimeout::MaxValue - 1,
                                     [](int value) {
                                         std::string labels[] = {"Short", "Medium", "Long"};
                                         return labels[value];
                                     }});
            m_configManager->setEnumDefault(SettingMenuTimeout, MenuTimeout::Medium);
            m_menuEntries.push_back(
                {"Menu eye offset", MenuEntryType::Slider, SettingOverlayEyeOffset, -3000, 3000, [](int value) {
                     return fmt::format("{}px", value);
                 }});
            m_menuEntries.back().acceleration = 10;

            m_menuEntries.push_back({"Restore defaults", MenuEntryType::RestoreDefaultsButton, BUTTON_OR_SEPARATOR});
            m_menuEntries.push_back({"Exit menu", MenuEntryType::ExitButton, BUTTON_OR_SEPARATOR});
        }

        void handleInput() override {
            const auto now = std::chrono::steady_clock::now();

            // Check whether this is a long press and the event needs to be repeated.
            m_isAccelerating = GetAsyncKeyState(VK_SHIFT) < 0;

            const bool isRepeat = (now - m_lastInput) > (m_isAccelerating ? KeyRepeatDelay / 10 : KeyRepeatDelay);
            const bool moveLeft = UpdateKeyState(m_moveLeftKeyState, m_keyModifiers, m_keyLeft, isRepeat);
            const bool moveRight = UpdateKeyState(m_moveRightKeyState, m_keyModifiers, m_keyRight, isRepeat);
            const bool menuControl = UpdateKeyState(m_menuControlKeyState, m_keyModifiers, m_keyMenu, false);

            if (menuControl) {
                if (m_state != MenuState::Visible) {
                    m_state = MenuState::Visible;

                    m_needRestart = checkNeedRestartCondition();
                    m_menuEntriesTitleWidth = 0.0f;
                    m_menuEntriesRight = m_menuEntriesBottom = 0.0f;

                } else {
                    do {
                        static_assert(std::is_unsigned_v<decltype(m_selectedItem)>);

                        m_selectedItem += m_isAccelerating ? -1 : 1;
                        if (m_selectedItem >= m_menuEntries.size())
                            m_selectedItem = m_isAccelerating ? m_menuEntries.size() - 1 : 0;

                    } while (m_menuEntries[m_selectedItem].type == MenuEntryType::Separator ||
                             !m_menuEntries[m_selectedItem].visible);
                }

                m_resetArmed = false;
                m_lastInput = now;
            }

            if (m_state == MenuState::Visible && (moveLeft || moveRight)) {
                const auto& menuEntry = m_menuEntries[m_selectedItem];

                switch (menuEntry.type) {
                case MenuEntryType::Separator:
                    break;

                case MenuEntryType::RestoreDefaultsButton:
                    if (m_resetArmed) {
                        m_configManager->resetToDefaults();

                        m_needRestart = checkNeedRestartCondition();
                        m_menuEntriesTitleWidth = 0.0f;
                        m_menuEntriesRight = m_menuEntriesBottom = 0.0f;
                        m_resetArmed = false;
                    } else {
                        m_resetArmed = true;
                    }
                    break;

                case MenuEntryType::ExitButton:
                    m_selectedItem = 0;
                    m_state = MenuState::NotVisible;
                    break;

                default:
                    const int value = m_configManager->peekValue(menuEntry.configName);
                    const int newValue =
                        std::clamp(value + (moveLeft ? -1 : 1) * (!m_isAccelerating ? 1 : menuEntry.acceleration),
                                   menuEntry.minValue,
                                   menuEntry.maxValue);

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
            // Project a single point 1m in front of the head from both eyes to clip space and compute the offset.
            // Seems to work well.
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

            const float leftAlign = (2.0f * renderTarget->getInfo().width / 5.0f) + eyeOffset;
            const float rightAlign = (2 * renderTarget->getInfo().width / 3.0f) + eyeOffset;
            const float topAlign = renderTarget->getInfo().height / 3.0f;

            const float fontSizes[(int)MenuFontSize::MaxValue] = {
                renderTarget->getInfo().height * 0.0075f,
                renderTarget->getInfo().height * 0.015f,
                renderTarget->getInfo().height * 0.02f,
            };
            const float fontSize = fontSizes[m_configManager->getValue(SettingMenuFontSize)];

            const double timeouts[(int)MenuTimeout::MaxValue] = {3.0, 12.0, 60.0};
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
                m_device->drawString(fmt::format(L"Press {}{} to bring up the menu ({}s)",
                                                 m_keyModifiersLabel,
                                                 m_keyMenuLabel,
                                                 (int)(std::ceil(timeout - duration))),
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

                const auto center = (leftAlign + m_menuEntriesRight + eyeOffset) / 2;

                float top = topAlign;
                float left = leftAlign;

                m_device->drawString(toolkit::LayerPrettyNameFull,
                                     TextStyle::Bold,
                                     fontSize,
                                     center,
                                     top,
                                     colorSelected,
                                     measure,
                                     FW1_CENTER);

                top += 1.10f * fontSize;

                m_device->drawString(fmt::format(L"\xE2B6 : {0}{1}   {4} : {0}{2}   \xE2B7 : {0}{3}",
                                                 m_keyModifiersLabel,
                                                 m_keyLeftLabel,
                                                 m_keyMenuLabel,
                                                 m_keyRightLabel,
                                                 m_isAccelerating ? L"\xE1FE" : L"\xE1FC"),
                                     TextStyle::Normal,
                                     fontSize * 0.75f,
                                     center,
                                     top,
                                     colorNormal,
                                     measure,
                                     FW1_CENTER);
                top += 1.05f * fontSize;
                m_device->drawString(L"Change values faster with SHIFT",
                                     TextStyle::Normal,
                                     fontSize * 0.75f,
                                     center,
                                     top,
                                     colorNormal,
                                     measure,
                                     FW1_CENTER);
                top += 1.5f * fontSize;

                if (eye == 0) {
                    m_menuEntriesRight = std::max(m_menuEntriesRight, left);
                }

                float menuEntriesTitleWidth = m_menuEntriesTitleWidth;

                // Display each menu entry.
                for (unsigned int i = 0; i < m_menuEntries.size(); i++) {
                    left = leftAlign;
                    const auto& menuEntry = m_menuEntries[i];
                    const auto& title = (menuEntry.type == MenuEntryType::RestoreDefaultsButton && m_resetArmed)
                                            ? "Confirm?"
                                            : menuEntry.title;

                    // Always account for entries, even invisible ones, when calculating the alignment.
                    float entryWidth = 0.0f;
                    if (menuEntriesTitleWidth == 0.0f) {
                        // Worst case should be Selected (bold).
                        entryWidth = m_device->measureString(title, TextStyle::Bold, fontSize) + 50;
                        m_menuEntriesTitleWidth = std::max(m_menuEntriesTitleWidth, entryWidth);
                    }

                    if (!menuEntry.visible) {
                        continue;
                    }

                    // highlight selected item line.
                    if (i == m_selectedItem && !measure) {
                        XrColor4f background({0.01f, 0.01f, 0.01f, alpha/255.f});
                        const auto highlightTop = top + (fontSize / 3.f);
                        m_device->clearColor(
                            highlightTop, left, highlightTop + fontSize, m_menuEntriesRight + eyeOffset, background);
                    }

                    const auto entryStyle = i == m_selectedItem ? TextStyle::Bold : TextStyle::Normal;
                    const auto entryColor = i == m_selectedItem ? colorSelected : colorNormal;

                    m_device->drawString(title, entryStyle, fontSize, left, top, entryColor);
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
                        m_menuEntriesRight = std::max(m_menuEntriesRight, left);
                    }
                }

                {
                    float left = leftAlign;
                    if (m_configManager->isSafeMode()) {
                        top += fontSize;

                        left += m_device->drawString(L"\x26A0  Running in safe mode, settings are cleared when "
                                                     L"restarting the VR session \x26A0",
                                                     TextStyle::Bold,
                                                     fontSize,
                                                     left,
                                                     top,
                                                     colorWarning,
                                                     measure);

                        top += 1.05f * fontSize;
                    } else if (m_needRestart) {
                        top += fontSize;

                        left += m_device->drawString(L"\x26A0  Restart the VR session to apply changes \x26A0",
                                                     TextStyle::Bold,
                                                     fontSize,
                                                     left,
                                                     top,
                                                     colorWarning,
                                                     measure);

                        top += 1.05f * fontSize;
                    }

                    if (eye == 0) {
                        m_menuEntriesRight = std::max(m_menuEntriesRight, left);
                    }
                }
                m_menuEntriesBottom = top + fontSize * 0.2f;
            }

            auto overlayType = m_configManager->getEnumValue<OverlayType>(SettingOverlayType);
            if (m_state != MenuState::Splash && overlayType != OverlayType::None) {
                float top = m_state != MenuState::Visible ? topAlign : topAlign - 1.1f * fontSize;

#define OVERLAY_COMMON TextStyle::Normal, fontSize, rightAlign - 200, top, ColorSelected, true, FW1_LEFT

                m_device->drawString(fmt::format("FPS: {}", m_stats.fps), OVERLAY_COMMON);
                top += 1.05f * fontSize;

                // Advanced display.
                if (m_state != MenuState::Visible && overlayType == OverlayType::Advanced) {
#define TIMING_STAT(label, name)                                                                                       \
    m_device->drawString(fmt::format(label ": {}", m_stats.name), OVERLAY_COMMON);                                     \
    top += 1.05f * fontSize;

                    TIMING_STAT("app CPU", appCpuTimeUs);
                    TIMING_STAT("app GPU", appGpuTimeUs);
                    TIMING_STAT("lay CPU", endFrameCpuTimeUs);
                    TIMING_STAT("pre GPU", preProcessorGpuTimeUs);
                    TIMING_STAT("scl GPU", upscalerGpuTimeUs);
                    TIMING_STAT("pst GPU", postProcessorGpuTimeUs);
                    TIMING_STAT("ovl CPU", overlayCpuTimeUs);
                    TIMING_STAT("ovl GPU", overlayGpuTimeUs);
                    if (m_isHandTrackingSupported) {
                        TIMING_STAT("hnd CPU", handTrackingCpuTimeUs);
                    }

#undef TIMING_STAT

                    top += 1.05f * fontSize;

#define GESTURE_STATE(label, name)                                                                                     \
    m_device->drawString(                                                                                              \
        fmt::format(label ": {:.2f}/{:.2f}", m_gesturesState.name##Value[0], m_gesturesState.name##Value[1]),          \
        OVERLAY_COMMON);                                                                                               \
    top += 1.05f * fontSize;

                    if (m_isHandTrackingSupported && m_configManager->getEnumValue<HandTrackingEnabled>(
                                                         SettingHandTrackingEnabled) != HandTrackingEnabled::Off) {
                        GESTURE_STATE("pinch", pinch);
                        GESTURE_STATE("thb.pr", thumbPress);
                        GESTURE_STATE("indx.b", indexBend);
                        GESTURE_STATE("f.gun", fingerGun);
                        GESTURE_STATE("squze", squeeze);
                        GESTURE_STATE("wrist", wristTap);
                        GESTURE_STATE("palm", palmTap);
                        GESTURE_STATE("tiptap", indexTipTap);
                        GESTURE_STATE("cust1", custom1);

                        m_device->drawString(fmt::format("loss: {}/{}",
                                                         m_gesturesState.numTrackingLosses[0] % 256,
                                                         m_gesturesState.numTrackingLosses[1] % 256),
                                             OVERLAY_COMMON);
                        top += 1.05f * fontSize;
                    }

#undef GESTURE_STATE
                }
#undef OVERLAY_COMMON
            }
        }

        void updateStatistics(const LayerStatistics& stats) override {
            m_stats = stats;
        }

        void updateGesturesState(const GesturesState& state) override {
            m_gesturesState = state;
        }

      private:
        std::wstring keyToString(int key) const {
            wchar_t buf[16];
            GetKeyNameTextW(MAKELPARAM(0, MapVirtualKeyA(key, MAPVK_VK_TO_VSC)), buf, ARRAYSIZE(buf));
            return buf;
        }

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
        GesturesState m_gesturesState{};

        std::vector<int> m_keyModifiers;
        std::wstring m_keyModifiersLabel;
        int m_keyLeft{VK_F1};
        std::wstring m_keyLeftLabel;
        int m_keyRight{VK_F3};
        std::wstring m_keyRightLabel;
        int m_keyMenu{VK_F2};
        std::wstring m_keyMenuLabel;

        int m_numSplashLeft;
        std::vector<MenuEntry> m_menuEntries;
        size_t m_selectedItem{0};
        std::chrono::steady_clock::time_point m_lastInput;
        bool m_moveLeftKeyState{false};
        bool m_moveRightKeyState{false};
        bool m_menuControlKeyState{false};
        bool m_resetArmed{false};

        // animation control
        bool m_isAccelerating{false};

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
                                                    std::vector<int>& keyModifiers,
                                                    bool isHandTrackingSupported,
                                                    bool isPredictionDampeningSupported) {
        return std::make_shared<MenuHandler>(configManager,
                                             device,
                                             displayWidth,
                                             displayHeight,
                                             keyModifiers,
                                             isHandTrackingSupported,
                                             isPredictionDampeningSupported);
    }

} // namespace toolkit::menu
