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
    constexpr uint32_t ColorOverlay = 0xff0099ff;
    constexpr uint32_t ColorHighlight = 0xff7772bc;
    constexpr uint32_t ColorSelected = 0xff533e5d;
    constexpr uint32_t ColorHint = 0xff8f8f8f;
    constexpr uint32_t ColorWarning = 0xff0000ff;
    constexpr XrColor4f BackgroundColor({0.16f, 0.17f, 0.20f, 1.0f});
    constexpr XrColor4f BackgroundHighlight({0.47f, 0.45f, 0.74f, 1.0f});
    constexpr XrColor4f BackgroundSelected({0.36f, 0.26f, 0.38f, 1.0f});
    constexpr float BorderSpacing = 10.f;
    constexpr uint32_t OptionSpacing = 20;
    constexpr uint32_t ValueSpacing = 20;
    constexpr uint32_t SelectionSpacing = 2;
    const std::string OptionIndent = "   ";
    const std::string SubGroupIndent = "     ";

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

        // Steps for changing the value.
        int acceleration{1};

        // Some settings might require exiting VR immediately to test the change. Bypass the commit delay when true.
        bool noCommitDelay{false};

        // Some entries are not attached to config values. Use a direct pointer in this case.
        int* pValue{nullptr};

        bool visible{false};
    };

    class MenuGroup {
      public:
        MenuGroup(std::vector<MenuGroup>& menuGroups,
                  std::vector<MenuEntry>& menuEntries,
                  std::function<bool()> isVisible,
                  bool isTab = false)
            : m_menuGroups(&menuGroups), m_menuEntries(&menuEntries), m_isVisible(isVisible),
              m_start(menuEntries.size()), m_end(menuEntries.size()), m_isTab(isTab) {
        }

        MenuGroup& operator=(const MenuGroup&) = default;

        void finalize() {
            m_end = m_menuEntries->size();

            if (!m_isTab) {
                m_menuGroups->push_back(*this);
            } else {
                // Tabs must always be evaluated first.
                m_menuGroups->insert(m_menuGroups->begin(), *this);
            }
        }

        void updateVisibility() const {
            for (size_t i = m_start; i < m_end; i++) {
                (*m_menuEntries)[i].visible = (m_isTab || (*m_menuEntries)[i].visible) && m_isVisible();
            }
        };

      private:
        std::vector<MenuGroup>* m_menuGroups;
        std::vector<MenuEntry>* m_menuEntries;
        std::function<bool()> m_isVisible;
        size_t m_start;
        size_t m_end;

        bool m_isTab;
    };

    enum class MenuTab { Performance = 0, Appearance, Inputs, Menu };

    // The logic of our menus.
    class MenuHandler : public IMenuHandler {
      public:
        MenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                    std::shared_ptr<IDevice> device,
                    uint32_t displayWidth,
                    uint32_t displayHeight,
                    std::vector<int>& keyModifiers,
                    bool isHandTrackingSupported,
                    bool isPredictionDampeningSupported,
                    bool isMotionReprojectionRateSupported,
                    uint8_t variableRateShaderMaxDownsamplePow2)
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

            // Prepare the tabs.
            static const std::string_view tabs[] = {"Performance", "Appearance", "Inputs", "Menu"};
            m_menuEntries.push_back({"Category", MenuEntryType::Choice, "", 0, ARRAYSIZE(tabs) - 1, [&](int value) {
                                         return std::string(tabs[value]);
                                     }});
            m_menuEntries.back().pValue = reinterpret_cast<int*>(&m_currentTab);
            m_menuEntries.back().visible = true; /* Always visible. */

            // Intentionally place 2 separators to create space.
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.back().visible = true; /* Always visible. */
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.back().visible = true; /* Always visible. */

            setupPerformanceTab(isMotionReprojectionRateSupported, variableRateShaderMaxDownsamplePow2);
            setupAppearanceTab();
            setupInputsTab(isPredictionDampeningSupported);
            setupMenuTab();

            // Intentionally place 2 separators to create space.
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.back().visible = true; /* Always visible. */
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.back().visible = true; /* Always visible. */

            m_menuEntries.push_back({"Exit menu", MenuEntryType::ExitButton, BUTTON_OR_SEPARATOR});
            m_menuEntries.back().visible = true; /* Always visible. */
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
                    m_menuEntriesWidth = m_menuEntriesHeight = 0.0f;

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
                auto& menuEntry = m_menuEntries[m_selectedItem];

                switch (menuEntry.type) {
                case MenuEntryType::Separator:
                    break;

                case MenuEntryType::RestoreDefaultsButton:
                    if (m_resetArmed) {
                        m_configManager->resetToDefaults();

                        m_needRestart = checkNeedRestartCondition();
                        m_menuEntriesTitleWidth = 0.0f;
                        m_menuEntriesWidth = m_menuEntriesHeight = 0.0f;
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
                    const auto previousValue = peekEntryValue(menuEntry);
                    setEntryValue(menuEntry,
                                  previousValue +
                                      (moveLeft ? -1 : 1) * (!m_isAccelerating ? 1 : menuEntry.acceleration));

                    // When changing anamorphic setting, toggle the config value sign.
                    if (menuEntry.pValue == &m_useAnamorphic) {
                        auto value = m_configManager->peekValue(SettingAnamorphic);

                        // Fail safe: ensure there is a valid condition to toggle the sign of the value.
                        // However in this case, since useAnamorphic is binary, we could also toggle the sign of the
                        // value directly.

                        if ((m_useAnamorphic != 0) ^ (value > 0))
                            m_configManager->setValue(SettingAnamorphic, -value, true);
                    }

                    // When changing some settings, display the warning that the session must be restarted.
                    const bool wasRestartNeeded = std::exchange(m_needRestart, checkNeedRestartCondition());

                    // When changing the font size or displaying the restart banner, force re-alignment/re-size.
                    if ((menuEntry.configName == SettingMenuFontSize &&
                         previousValue != m_configManager->peekValue(SettingMenuFontSize)) ||
                        wasRestartNeeded != m_needRestart) {
                        m_menuEntriesTitleWidth = 0.0f;
                        m_menuEntriesWidth = m_menuEntriesHeight = 0.0f;
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

            for (const auto& group : m_menuGroups) {
                group.updateVisibility();
            }
        }

        void render(Eye eye, std::shared_ptr<ITexture> renderTarget) const override {
            const float leftEyeOffset = 0.0f;
            const float rightEyeOffset = (float)m_configManager->getValue(SettingMenuEyeOffset);
            const float eyeOffset = eye == Eye::Left ? leftEyeOffset : rightEyeOffset;

            const float leftAnchor = (renderTarget->getInfo().width - m_menuEntriesWidth) / 2;
            const float leftAlign = leftAnchor + eyeOffset;
            const float centerAlign = leftAnchor + m_menuEntriesWidth / 2 + eyeOffset;
            const float rightAlign = leftAnchor + m_menuEntriesWidth + eyeOffset;
            const float overlayAlign = (2 * renderTarget->getInfo().width / 3.0f) + eyeOffset;
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
            const auto colorHighlight = (ColorHighlight & 0xffffff) | (alpha << 24);
            const auto colorSelected = (ColorSelected & 0xffffff) | (alpha << 24);
            const auto colorHint = (ColorHint & 0xffffff) | (alpha << 24);
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
                const bool measure = m_menuEntriesWidth == 0.0f;
                if (!measure) {
                    m_device->clearColor(topAlign - BorderSpacing,
                                         leftAlign - BorderSpacing,
                                         topAlign + m_menuEntriesHeight + BorderSpacing,
                                         leftAlign + m_menuEntriesWidth + BorderSpacing,
                                         BackgroundColor);
                }

                float top = topAlign;

                m_device->drawString(toolkit::LayerPrettyNameFull,
                                     TextStyle::Bold,
                                     fontSize,
                                     centerAlign,
                                     top,
                                     colorNormal,
                                     measure,
                                     FW1_CENTER);

                top += 1.25f * fontSize;

                float menuEntriesTitleWidth = m_menuEntriesTitleWidth;

                // Display each menu entry.
                for (unsigned int i = 0; i < m_menuEntries.size(); i++) {
                    float left = leftAlign;
                    const auto& menuEntry = m_menuEntries[i];
                    const auto& title = (menuEntry.type == MenuEntryType::RestoreDefaultsButton && m_resetArmed)
                                            ? OptionIndent + "Confirm?"
                                            : menuEntry.title;

                    // Always account for entries, even invisible ones, when calculating the alignment.
                    float entryWidth = 0.0f;
                    if (menuEntriesTitleWidth == 0.0f) {
                        // Worst case should be Selected (bold).
                        entryWidth = m_device->measureString(title, TextStyle::Bold, fontSize) + OptionSpacing;
                        m_menuEntriesTitleWidth = std::max(m_menuEntriesTitleWidth, entryWidth);
                    }

                    if (!menuEntry.visible) {
                        continue;
                    }

                    const auto entryStyle = i == m_selectedItem ? TextStyle::Bold : TextStyle::Normal;
                    const auto entryColor = i == m_selectedItem ? colorHighlight : colorNormal;

                    m_device->drawString(title, entryStyle, fontSize, left, top, entryColor);
                    if (menuEntriesTitleWidth == 0.0f) {
                        left += entryWidth;
                    } else {
                        left += menuEntriesTitleWidth;
                    }

                    const int value = peekEntryValue(menuEntry);

                    // Display the current value.
                    switch (menuEntry.type) {
                    case MenuEntryType::Slider:
                        left += m_device->drawString(menuEntry.valueToString(value),
                                                     TextStyle::Normal,
                                                     fontSize,
                                                     left,
                                                     top,
                                                     entryColor,
                                                     measure);
                        break;

                    case MenuEntryType::Choice:
                        for (int j = menuEntry.minValue; j <= menuEntry.maxValue; j++) {
                            const std::string label = menuEntry.valueToString(j);

                            const auto valueColor = i == m_selectedItem && value != j ? colorHighlight : colorNormal;
                            const auto backgroundColor = i == m_selectedItem ? BackgroundHighlight : BackgroundSelected;

                            const auto width = m_device->measureString(label, TextStyle::Normal, fontSize);

                            if (j == value) {
                                m_device->clearColor(top + 4,
                                                     left - SelectionSpacing,
                                                     top + 4 + fontSize + SelectionSpacing,
                                                     left + width + SelectionSpacing + 2,
                                                     backgroundColor);
                            }

                            m_device->drawString(label, TextStyle::Normal, fontSize, left, top, valueColor);
                            left += width + ValueSpacing;
                        }
                        break;

                    case MenuEntryType::Separator:
                        // Counteract the auto-down and add our own spacing.
                        top -= 1.1f * fontSize;
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

                    top += 1.1f * fontSize;

                    if (eye == Eye::Left) {
                        m_menuEntriesWidth = std::max(m_menuEntriesWidth, left - leftAlign);
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

                    // Create a little spacing.
                    top += 10;

                    m_device->drawString(fmt::format(L"\xE2B6 : {0}{1}   {4} : {0}{2}   \xE2B7 : {0}{3}",
                                                     m_keyModifiersLabel,
                                                     m_keyLeftLabel,
                                                     m_keyMenuLabel,
                                                     m_keyRightLabel,
                                                     m_isAccelerating ? L"\xE1FE" : L"\xE1FC"),
                                         TextStyle::Normal,
                                         fontSize * 0.75f,
                                         rightAlign,
                                         top,
                                         colorHint,
                                         measure,
                                         FW1_RIGHT);
                    top += 0.8f * fontSize;
                    m_device->drawString(L"Change values faster with SHIFT",
                                         TextStyle::Normal,
                                         fontSize * 0.75f,
                                         rightAlign,
                                         top,
                                         colorHint,
                                         measure,
                                         FW1_RIGHT);
                    top += 0.8f * fontSize;

                    if (eye == Eye::Left) {
                        m_menuEntriesWidth = std::max(m_menuEntriesWidth, left - leftAlign);
                    }
                }
                m_menuEntriesHeight = (top + fontSize * 0.2f) - topAlign;
            }

            auto overlayType = m_configManager->getEnumValue<OverlayType>(SettingOverlayType);
            if (m_state != MenuState::Splash && overlayType != OverlayType::None) {
                float top = m_state != MenuState::Visible ? topAlign : topAlign - BorderSpacing - 1.1f * fontSize;

#define OVERLAY_COMMON TextStyle::Normal, fontSize, overlayAlign - 200, top, ColorOverlay, true, FW1_LEFT

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

                    m_device->drawString(fmt::format("{}{} / {}{}",
                                                     m_stats.hasColorBuffer[0] ? "C" : "_",
                                                     m_stats.hasDepthBuffer[0] ? "D" : "_",
                                                     m_stats.hasColorBuffer[1] ? "C" : "_",
                                                     m_stats.hasDepthBuffer[1] ? "D" : "_"),
                                         OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_device->drawString(fmt::format("VRS RTV: {}", m_stats.numRenderTargetsWithVRS), OVERLAY_COMMON);
                    top += 1.05f * fontSize;

#undef TIMING_STAT

                    top += 1.05f * fontSize;

#define GESTURE_STATE(label, name)                                                                                     \
    m_device->drawString(                                                                                              \
        fmt::format(label ": {:.2f}/{:.2f}", m_gesturesState.name##Value[0], m_gesturesState.name##Value[1]),          \
        OVERLAY_COMMON);                                                                                               \
    top += 1.05f * fontSize;

                    if (isHandTrackingEnabled()) {
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

        void updateStatistics(const MenuStatistics& stats) override {
            m_stats = stats;
        }

        void updateGesturesState(const GesturesState& state) override {
            m_gesturesState = state;
        }

      private:
        void setupPerformanceTab(bool isMotionReprojectionRateSupported, uint8_t variableRateShaderMaxDownsamplePow2) {
            MenuGroup performanceTab(
                m_menuGroups,
                m_menuEntries,
                [&] { return m_currentTab == MenuTab::Performance; } /* visible condition */,
                true /* isTab */);

            // Performance Overlay Settings.
            m_menuEntries.push_back({OptionIndent + "Overlay",
                                     MenuEntryType::Choice,
                                     SettingOverlayType,
                                     0,
                                     (int)OverlayType::MaxValue - (m_configManager->isExperimentalMode() ? 1 : 2),
                                     [](int value) {
                                         const std::string_view labels[] = {"Off", "FPS", "Detailed"};
                                         return std::string(labels[value]);
                                     }});
            m_configManager->setEnumDefault(SettingOverlayType, OverlayType::None);

            // Upscaling Settings.
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_originalScalingType = getCurrentScalingType();
            m_originalScalingValue = getCurrentScaling();
            m_originalAnamorphicValue = getCurrentAnamorphic();
            m_useAnamorphic = m_originalAnamorphicValue > 0 ? 1 : 0;

            m_menuEntries.push_back({OptionIndent + "Upscaling",
                                     MenuEntryType::Choice,
                                     SettingScalingType,
                                     0,
                                     (int)ScalingType::MaxValue - 1,
                                     [](int value) {
                                         const std::string_view labels[] = {"Off", "NIS", "FSR"};
                                         return std::string(labels[value]);
                                     }});
            m_menuEntries.back().noCommitDelay = true;

            // Scaling sub-group.
            MenuGroup upscalingGroup(m_menuGroups, m_menuEntries, [&] {
                return getCurrentScalingType() != ScalingType::None;
            } /* visible condition */);
            m_menuEntries.push_back({SubGroupIndent + "Anamorphic", MenuEntryType::Choice, "", 0, 1, [](int value) {
                                         const std::string_view labels[] = {"Off", "On"};
                                         return std::string(labels[value]);
                                     }});
            m_menuEntries.back().noCommitDelay = true;
            m_menuEntries.back().pValue = &m_useAnamorphic;

            // Proportional sub-group.
            MenuGroup proportionalGroup(m_menuGroups, m_menuEntries, [&] {
                return getCurrentScalingType() != ScalingType::None && !m_useAnamorphic;
            } /* visible condition */);
            m_menuEntries.push_back(
                {SubGroupIndent + "Size", MenuEntryType::Slider, SettingScaling, 25, 400, [&](int value) {
                     // We don't even use value, the utility function below will query it.
                     return fmt::format("{}% ({}x{})",
                                        value,
                                        GetScaledInputSize(m_displayWidth, value, 2),
                                        GetScaledInputSize(m_displayHeight, value, 2));
                 }});
            m_menuEntries.back().noCommitDelay = true;
            proportionalGroup.finalize();

            // Anamorphic sub-group.
            MenuGroup anamorphicGroup(m_menuGroups, m_menuEntries, [&] {
                return getCurrentScalingType() != ScalingType::None && m_useAnamorphic;
            } /* visible condition */);
            m_menuEntries.push_back(
                {SubGroupIndent + "Width", MenuEntryType::Slider, SettingScaling, 25, 400, [&](int value) {
                     return fmt::format("{}% ({} pixels)", value, GetScaledInputSize(m_displayWidth, value, 2));
                 }});
            m_menuEntries.back().noCommitDelay = true;

            m_menuEntries.push_back(
                {SubGroupIndent + "Height", MenuEntryType::Slider, SettingAnamorphic, 25, 400, [&](int value) {
                     return fmt::format("{}% ({} pixels)", value, GetScaledInputSize(m_displayHeight, value, 2));
                 }});
            m_menuEntries.back().noCommitDelay = true;
            anamorphicGroup.finalize();

            m_menuEntries.push_back(
                {SubGroupIndent + "Sharpness", MenuEntryType::Slider, SettingSharpness, 0, 100, [](int value) {
                     return fmt::format("{}%", value);
                 }});
            upscalingGroup.finalize();

            // Motion Reprojection Settings.
            if (isMotionReprojectionRateSupported) {
                m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
                m_menuEntries.push_back({OptionIndent + "Lock motion reprojection",
                                         MenuEntryType::Slider,
                                         SettingMotionReprojectionRate,
                                         (int)MotionReprojectionRate::Off,
                                         (int)MotionReprojectionRate::MaxValue - 1,
                                         [&](int value) {
                                             std::string_view labels[] = {"Unlocked", "1/half", "1/third", "1/quarter"};
                                             return std::string(labels[value - 1]);
                                         }});
            }

            // Fixed Foveated Rendering (VRS) Settings.
            if (variableRateShaderMaxDownsamplePow2) {
                m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
                m_menuEntries.push_back({OptionIndent + "Fixed foveated rendering",
                                         MenuEntryType::Choice,
                                         SettingVRS,
                                         0,
                                         (int)VariableShadingRateType::MaxValue - 1,
                                         [&](int value) {
                                             const std::string_view labels[] = {"Off", "Preset", "Custom"};
                                             return std::string(labels[value]);
                                         }});

                // Preset sub-group.
                MenuGroup variableRateShaderPresetGroup(m_menuGroups, m_menuEntries, [&] {
                    return m_configManager->peekEnumValue<VariableShadingRateType>(SettingVRS) ==
                           VariableShadingRateType::Preset;
                } /* visible condition */);
                m_menuEntries.push_back({SubGroupIndent + "Quality",
                                         MenuEntryType::Slider,
                                         SettingVRSQuality,
                                         0,
                                         (int)VariableShadingRateQuality::MaxValue - 1,
                                         [&](int value) {
                                             const std::string_view labels[] = {"Performance", "Balanced", "Quality"};
                                             return std::string(labels[value]);
                                         }});
                m_menuEntries.push_back({SubGroupIndent + "Pattern",
                                         MenuEntryType::Slider,
                                         SettingVRSPattern,
                                         0,
                                         (int)VariableShadingRatePattern::MaxValue - 1,
                                         [&](int value) {
                                             const std::string_view labels[] = {"Wide", "Balanced", "Narrow"};
                                             return std::string(labels[value]);
                                         }});
                variableRateShaderPresetGroup.finalize();

                // Custom sub-group.
                MenuGroup variableRateShaderCustomGroup(m_menuGroups, m_menuEntries, [&] {
                    return m_configManager->peekEnumValue<VariableShadingRateType>(SettingVRS) ==
                           VariableShadingRateType::Custom;
                } /* visible condition */);
                {
                    static auto samplePow2ToString = [](int value) -> std::string {
                        switch (value) {
                        case 0:
                            return "1x";
                        default:
                            return fmt::format("1/{}x", 1 << value);
                        case 5:
                            return "Cull";
                        }
                    };
                    static auto radiusToString = [](int value) { return fmt::format("{}%", value); };
                    m_menuEntries.push_back({SubGroupIndent + "Inner resolution",
                                             MenuEntryType::Slider,
                                             SettingVRSInner,
                                             0,
                                             variableRateShaderMaxDownsamplePow2,
                                             samplePow2ToString});
                    m_menuEntries.push_back({SubGroupIndent + "Inner ring size",
                                             MenuEntryType::Slider,
                                             SettingVRSInnerRadius,
                                             0,
                                             100,
                                             radiusToString});
                    m_menuEntries.push_back({SubGroupIndent + "Middle resolution",
                                             MenuEntryType::Slider,
                                             SettingVRSMiddle,
                                             1, // Exclude 1x to discourage people from using poor settings!
                                             variableRateShaderMaxDownsamplePow2,
                                             samplePow2ToString});
                    m_menuEntries.push_back({SubGroupIndent + "Outer ring size",
                                             MenuEntryType::Slider,
                                             SettingVRSOuterRadius,
                                             0,
                                             100,
                                             radiusToString});
                    m_menuEntries.push_back({SubGroupIndent + "Outer resolution",
                                             MenuEntryType::Slider,
                                             SettingVRSOuter,
                                             1, // Exclude 1x to discourage people from using poor settings!
                                             variableRateShaderMaxDownsamplePow2,
                                             samplePow2ToString});
                }
                variableRateShaderCustomGroup.finalize();
            }

            // Must be kept last.
            performanceTab.finalize();
        }

        void setupAppearanceTab() {
            MenuGroup appearanceTab(
                m_menuGroups,
                m_menuEntries,
                [&] { return m_currentTab == MenuTab::Appearance; } /* visible condition */,
                true /* isTab */);
            m_menuEntries.push_back(
                {OptionIndent + "World scale", MenuEntryType::Slider, SettingICD, 1, 10000, [&](int value) {
                     return fmt::format("{:.1f}% ({:.1f}mm)", value / 10.f, m_stats.icd * 1000);
                 }});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back(
                {OptionIndent + "Field of view", MenuEntryType::Slider, SettingFOV, 50, 150, [](int value) {
                     return fmt::format("{}%", value);
                 }});

            // Must be kept last.
            appearanceTab.finalize();
        }

        void setupInputsTab(bool isPredictionDampeningSupported) {
            MenuGroup inputsTab(
                m_menuGroups,
                m_menuEntries,
                [&] { return m_currentTab == MenuTab::Inputs; } /* visible condition */,
                true /* isTab */);

            if (isPredictionDampeningSupported) {
                m_menuEntries.push_back({OptionIndent + "Shaking attenuation",
                                         MenuEntryType::Slider,
                                         SettingPredictionDampen,
                                         0,
                                         100,
                                         [&](int value) {
                                             if (value == 100) {
                                                 return fmt::format("{}%", value - 100);
                                             } else {
                                                 return fmt::format("{}% (of {:.1f}ms)",
                                                                    value - 100,
                                                                    m_stats.predictionTimeUs / 1000000.0f);
                                             }
                                         }});
                m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            }

            if (m_isHandTrackingSupported) {
                m_menuEntries.push_back({OptionIndent + "Controller emulation",
                                         MenuEntryType::Choice,
                                         SettingHandTrackingEnabled,
                                         0,
                                         (int)HandTrackingEnabled::MaxValue - 1,
                                         [](int value) {
                                             const std::string_view labels[] = {"Off", "Both", "Left", "Right"};
                                             return std::string(labels[value]);
                                         }});
                m_originalHandTrackingEnabled = isHandTrackingEnabled();

                MenuGroup handTrackingGroup(
                    m_menuGroups, m_menuEntries, [&] { return isHandTrackingEnabled(); } /* visible condition */);
                m_menuEntries.push_back(
                    {SubGroupIndent + "Hand skeleton",
                     MenuEntryType::Slider,
                     SettingHandVisibilityAndSkinTone,
                     0,
                     4,
                     [](int value) {
                         const std::string_view labels[] = {"Hidden", "Bright", "Medium", "Dark", "Darker"};
                         return std::string(labels[value]);
                     }});
                m_menuEntries.push_back({SubGroupIndent + "Controller timeout",
                                         MenuEntryType::Slider,
                                         SettingHandTimeout,
                                         0,
                                         60,
                                         [](int value) {
                                             if (value == 0) {
                                                 return std::string("Always on");
                                             } else {
                                                 return fmt::format("{}s", value);
                                             }
                                         }});
                handTrackingGroup.finalize();
            }

            // Must be kept last.
            inputsTab.finalize();
        }

        void setupMenuTab() {
            MenuGroup menuTab(
                m_menuGroups,
                m_menuEntries,
                [&] { return m_currentTab == MenuTab::Menu; } /* visible condition */,
                true /* isTab */);

            m_menuEntries.push_back({OptionIndent + "Font size",
                                     MenuEntryType::Slider,
                                     SettingMenuFontSize,
                                     0,
                                     (int)MenuFontSize::MaxValue - 1,
                                     [](int value) {
                                         const std::string_view labels[] = {"Small", "Medium", "Large"};
                                         return std::string(labels[value]);
                                     }});
            m_configManager->setEnumDefault(SettingMenuFontSize, MenuFontSize::Medium);
            m_menuEntries.push_back({OptionIndent + "Menu timeout",
                                     MenuEntryType::Slider,
                                     SettingMenuTimeout,
                                     0,
                                     (int)MenuTimeout::MaxValue - 1,
                                     [](int value) {
                                         const std::string_view labels[] = {"Short", "Medium", "Long"};
                                         return std::string(labels[value]);
                                     }});
            m_configManager->setEnumDefault(SettingMenuTimeout, MenuTimeout::Medium);
            m_menuEntries.push_back({OptionIndent + "Menu eye offset",
                                     MenuEntryType::Slider,
                                     SettingMenuEyeOffset,
                                     -3000,
                                     3000,
                                     [](int value) { return fmt::format("{} pixels", value); }});
            m_menuEntries.back().acceleration = 10;

            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.push_back(
                {OptionIndent + "Restore defaults", MenuEntryType::RestoreDefaultsButton, BUTTON_OR_SEPARATOR});

            // Must be kept last.
            menuTab.finalize();
        }

        int peekEntryValue(const MenuEntry& menuEntry) const {
            return menuEntry.pValue ? *menuEntry.pValue : m_configManager->peekValue(menuEntry.configName);
        }

        void setEntryValue(MenuEntry& menuEntry, int newValue) {
            newValue = std::clamp(newValue, menuEntry.minValue, menuEntry.maxValue);
            if (!menuEntry.pValue) {
                m_configManager->setValue(menuEntry.configName, newValue, menuEntry.noCommitDelay);
            } else {
                *menuEntry.pValue = newValue;
            }
        }

        std::wstring keyToString(int key) const {
            wchar_t buf[16];
            GetKeyNameTextW(MAKELPARAM(0, MapVirtualKeyA(key, MAPVK_VK_TO_VSC)), buf, ARRAYSIZE(buf));
            return buf;
        }

        bool isHandTrackingEnabled() const {
            return m_isHandTrackingSupported && m_configManager->peekEnumValue<HandTrackingEnabled>(
                                                    SettingHandTrackingEnabled) != HandTrackingEnabled::Off;
        }

        int getCurrentScaling() const {
            return m_configManager->peekValue(SettingScaling);
        }

        int getCurrentAnamorphic() const {
            return m_configManager->peekValue(SettingAnamorphic);
        }

        ScalingType getCurrentScalingType() const {
            return m_configManager->peekEnumValue<ScalingType>(SettingScalingType);
        }

        bool checkNeedRestartCondition() const {
            if (m_originalHandTrackingEnabled != isHandTrackingEnabled() ||
                m_originalScalingType != getCurrentScalingType()) {
                return true;
            }

            if (m_originalScalingType != ScalingType::None) {
                return m_originalScalingValue != getCurrentScaling() ||
                       m_originalAnamorphicValue != getCurrentAnamorphic();
            }

            return false;
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_displayWidth;
        const uint32_t m_displayHeight;
        const bool m_isHandTrackingSupported;
        MenuStatistics m_stats{};
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
        std::vector<MenuGroup> m_menuGroups;
        size_t m_selectedItem{0};
        MenuTab m_currentTab{MenuTab::Performance};
        std::chrono::steady_clock::time_point m_lastInput;
        bool m_moveLeftKeyState{false};
        bool m_moveRightKeyState{false};
        bool m_menuControlKeyState{false};
        bool m_resetArmed{false};

        // animation control
        bool m_isAccelerating{false};

        ScalingType m_originalScalingType{ScalingType::None};
        int m_originalScalingValue{0};
        int m_originalAnamorphicValue{0};
        int m_useAnamorphic{0};

        bool m_originalHandTrackingEnabled{false};
        bool m_needRestart{false};

        mutable MenuState m_state{MenuState::NotVisible};
        mutable float m_menuEntriesTitleWidth{0.0f};
        mutable float m_menuEntriesWidth{0.0f};
        mutable float m_menuEntriesHeight{0.0f};
    };

} // namespace

namespace toolkit::menu {
    std::shared_ptr<IMenuHandler> CreateMenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                    std::shared_ptr<toolkit::graphics::IDevice> device,
                                                    uint32_t displayWidth,
                                                    uint32_t displayHeight,
                                                    std::vector<int>& keyModifiers,
                                                    bool isHandTrackingSupported,
                                                    bool isPredictionDampeningSupported,
                                                    bool isMotionReprojectionRateSupported,
                                                    uint8_t variableRateShaderMaxDownsamplePow2) {
        return std::make_shared<MenuHandler>(configManager,
                                             device,
                                             displayWidth,
                                             displayHeight,
                                             keyModifiers,
                                             isHandTrackingSupported,
                                             isPredictionDampeningSupported,
                                             isMotionReprojectionRateSupported,
                                             variableRateShaderMaxDownsamplePow2);
    }

} // namespace toolkit::menu
