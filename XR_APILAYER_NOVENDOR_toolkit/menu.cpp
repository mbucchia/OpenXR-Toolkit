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

    // Utility macros/functions for color manipulation.
    constexpr uint8_t MakeColorU8(float c) {
        return static_cast<uint8_t>(c * 255.f + 0.5f);
    }
    constexpr uint32_t MakeRGB24(const XrColor4f& color) {
        return (MakeColorU8(color.r) << 0) | (MakeColorU8(color.g) << 8) | (MakeColorU8(color.b) << 16);
    }

    constexpr float sRGBToLinear(float c) {
        return std::clamp(c <= 0.04045f ? c * (1 / 12.92f) : pow((c + 0.055f) * (1.f / 1.055f), 2.4f), 0.f, 1.f);
    }
    constexpr XrColor4f sRGBToLinear(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        return XrColor4f({sRGBToLinear(r / 255.f), sRGBToLinear(g / 255.f), sRGBToLinear(b / 255.f), a / 255.f});
    }

    // Text colors
    const auto ColorNormal = sRGBToLinear(145, 141, 201);
    const auto ColorOverlay = sRGBToLinear(247, 198, 20);
    const auto ColorHint = sRGBToLinear(163, 163, 163);
    const auto ColorWarning = sRGBToLinear(255, 96, 96);
    const auto ColorSelected = sRGBToLinear(255, 255, 255);
    const auto ColorHighlightText = sRGBToLinear(34, 36, 42);

    // Shape colors.
    const auto ColorBackground = sRGBToLinear(34, 36, 42);
    const auto ColorHeader = sRGBToLinear(50, 52, 62);
    const auto ColorHeaderSeparator = sRGBToLinear(120, 126, 145);
    constexpr float HeaderLineWeight = 1.f;

    // Spacing and indentation.
    constexpr float BorderHorizontalSpacing = 20.f;
    constexpr float BorderVerticalSpacing = 10.f;
    constexpr uint32_t OptionSpacing = 30;
    constexpr uint32_t ValueSpacing = 20;
    constexpr uint32_t SelectionHorizontalSpacing = 8;
    constexpr uint32_t SelectionVerticalSpacing = 4;

    enum class MenuIndent { NoIndent = 0, OptionIndent = 0, SubGroupIndent = 20 };

    enum class MenuState { Splash, NotVisible, Visible };
    enum class MenuEntryType { Tabs, Slider, Choice, Separator, RestoreDefaults, ReloadShaders, ExitButton };

    class MenuHandler;

    class MenuGroup {
      public:
        MenuGroup(MenuHandler* menu, std::function<bool()> isVisible, bool isTab = false);
        MenuGroup& operator=(const MenuGroup&) = default;

        void finalize();
        void updateVisibility(bool showExpert) const;

      private:
        MenuHandler* m_menu;
        std::function<bool()> m_isVisible;
        size_t m_start;
        size_t m_count;
        bool m_isTab;
    };

    struct MenuEntry {
        template <typename E>
        static int LastVal();

        template <typename E>
        static std::string FmtEnum(int value);
        template <size_t N>
        static std::string FmtDecimal(int value);
        static std::string FmtPostVal(int value);
        static std::string FmtPercent(int value);
        static std::string FmtVrsRate(int value);
        static std::string FmtNone(int);

        MenuIndent indent;
        std::string title;
        MenuEntryType type;
#define BUTTON_OR_SEPARATOR "", 0, 0, MenuEntry::FmtNone
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

        bool expert{false};
        bool visible{false};
        bool disable{false};
    };

    enum class MenuTab { Performance = 0, Appearance, Inputs, System, Menu, Developer };

    // The logic of our menus.
    class MenuHandler : public IMenuHandler {
      public:
        MenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                    std::shared_ptr<IDevice> device,
                    const MenuInfo& menuInfo)
            : m_configManager(configManager), m_device(device), m_displayWidth(menuInfo.displayWidth),
              m_displayHeight(menuInfo.displayHeight), m_keyModifiers(menuInfo.keyModifiers),
              m_isHandTrackingSupported(menuInfo.isHandTrackingSupported),
              m_isEyeTrackingSupported(menuInfo.isEyeTrackingSupported),
              m_resolutionHeightRatio(menuInfo.resolutionHeightRatio),
              m_isMotionReprojectionRateSupported(menuInfo.isMotionReprojectionRateSupported),
              m_displayRefreshRate(menuInfo.displayRefreshRate), m_supportFOVHack(menuInfo.isPimaxFovHackSupported) {
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
            m_keyLeft = m_configManager->getValue(SettingMenuKeyLeft);
            m_keyLeftLabel = keyToString(m_keyLeft);
            m_keyRight = m_configManager->getValue(SettingMenuKeyRight);
            m_keyRightLabel = keyToString(m_keyRight);
            m_keyMenu = m_configManager->getValue(SettingMenuKeyDown);
            m_keyMenuLabel = keyToString(m_keyMenu);
            m_keyUp = m_configManager->getValue(SettingMenuKeyUp);
            if (m_keyUp) {
                m_keyUpLabel = keyToString(m_keyUp);
            } else {
                m_keyUpLabel = L"SHIFT+" + m_keyMenuLabel;
            }

            // Prepare the tabs.
            static const std::string_view tabs[] = {
                "Performance", "Appearance", "Inputs", "System", "Menu", "Developer"};
            m_menuEntries.push_back({MenuIndent::NoIndent,
                                     "",
                                     MenuEntryType::Tabs,
                                     "",
                                     0,
                                     ARRAYSIZE(tabs) - (m_configManager->getValue(SettingDeveloper) ? 1 : 2),
                                     [&](int value) { return std::string(tabs[value]); }});
            m_menuEntries.back().pValue = reinterpret_cast<int*>(&m_currentTab);
            m_menuEntries.back().visible = true; /* Always visible. */

            m_menuEntries.push_back({MenuIndent::NoIndent, "", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.back().visible = true; /* Always visible. */

            setupPerformanceTab(menuInfo);
            setupAppearanceTab(menuInfo);
            setupInputsTab(menuInfo);
            setupSystemTab(menuInfo);
            setupMenuTab(menuInfo);
            setupDeveloperTab(menuInfo);

            m_menuEntries.push_back(
                {MenuIndent::NoIndent, "Exit menu", MenuEntryType::ExitButton, BUTTON_OR_SEPARATOR});
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
            const bool moveUp = m_keyUp ? UpdateKeyState(m_moveUpKeyState, m_keyModifiers, m_keyUp, false)
                                        : menuControl && m_isAccelerating;

            if (menuControl || moveUp) {
                if (menuControl && m_state != MenuState::Visible) {
                    m_state = MenuState::Visible;

                    Log("Opening menu\n");

                    m_needRestart = checkNeedRestartCondition();
                    m_resetTextLayout = m_resetBackgroundLayout = true;

                } else if (m_state == MenuState::Visible) {
                    do {
                        static_assert(std::is_unsigned_v<decltype(m_selectedItem)>);

                        m_selectedItem += moveUp ? -1 : 1;
                        if (m_selectedItem >= m_menuEntries.size()) {
                            m_selectedItem = moveUp ? m_menuEntries.size() - 1 : 0;
                        }

                    } while (m_menuEntries[m_selectedItem].type == MenuEntryType::Separator ||
                             m_menuEntries[m_selectedItem].disable || !m_menuEntries[m_selectedItem].visible);
                }

                m_resetArmed = false;
                m_lastInput = now;
            }

            if (m_state == MenuState::Visible && (moveLeft || moveRight)) {
                auto& menuEntry = m_menuEntries[m_selectedItem];

                switch (menuEntry.type) {
                case MenuEntryType::Separator:
                    break;

                case MenuEntryType::RestoreDefaults:
                    if (m_resetArmed) {
                        m_resetArmed = false;
                        m_configManager->resetToDefaults();
                        m_needRestart = checkNeedRestartCondition();
                        m_resetTextLayout = m_resetBackgroundLayout = true;
                    } else {
                        m_resetArmed = true;
                    }
                    break;

                case MenuEntryType::ReloadShaders:
                    if (m_resetArmed) {
                        m_resetArmed = false;
                        m_configManager->setValue(config::SettingReloadShaders, 1, false);
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

                    // When switching tab, changing the font size, switching expert menu or displaying the restart
                    // banner, force re-alignment/re-size.
                    if (wasRestartNeeded != m_needRestart ||
                        ((menuEntry.type == MenuEntryType::Tabs || menuEntry.configName == SettingMenuFontSize ||
                          menuEntry.configName == SettingMenuExpert || menuEntry.configName == SettingPostProcess) &&
                         previousValue != peekEntryValue(menuEntry))) {
                        m_resetTextLayout = m_resetBackgroundLayout = true;
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

            const bool showExpert = m_configManager->getValue(SettingMenuExpert);
            for (const auto& group : m_menuGroups) {
                group.updateVisibility(showExpert);
            }

            for (auto& entry : m_menuEntries) {
                // Special case: the Overlay choice has more entries in Expert mode.
                if (entry.configName == SettingOverlayType) {
                    entry.maxValue = MenuEntry::LastVal<OverlayType>() - !showExpert;
                }
                // ... add more special cases here.
            }
        }

        void render(std::shared_ptr<ITexture> renderTarget) const override {
            const auto& renderTargetInfo = renderTarget->getInfo();

            const float leftAlign = (renderTargetInfo.width - m_menuBackgroundWidth) / 2;
            const float centerAlign = leftAlign + m_menuBackgroundWidth / 2;
            const float rightAlign = leftAlign + m_menuBackgroundWidth;
            const float topAlign = (renderTargetInfo.height - m_menuBackgroundHeight) / 2;

            const float fontSize = m_configManager->getValue(SettingMenuFontSize) * 0.75f; // pt -> px

            const double timeouts[to_integral(MenuTimeout::MaxValue)] = {3.0, 12.0, 60.0, INFINITY};
            const double timeout =
                m_state == MenuState::Splash ? 10.0 : timeouts[m_configManager->getValue(SettingMenuTimeout)];

            const auto now = std::chrono::steady_clock::now();
            const auto duration = std::chrono::duration<double>(now - m_lastInput).count();

            // Apply menu fade.
            const auto alphaValue = static_cast<float>(std::clamp(timeout - duration, 0.0, 1.0));
            const auto alpha = static_cast<uint32_t>(alphaValue * 255) << 24;
            const auto textColorOverlay = MakeRGB24(ColorOverlay) | alpha;

            // Leave upon timeout.
            if (duration >= timeout) {
                m_state = MenuState::NotVisible;
            }

            if (m_state == MenuState::Splash) {
                // The helper "splash screen".
                const auto banner = fmt::format(L"Press {}{} to bring up the menu ({}s)",
                                                m_keyModifiersLabel,
                                                m_keyMenuLabel,
                                                (int)(std::ceil(timeout - duration)));
                const auto width = m_device->measureString(banner, TextStyle::Normal, fontSize * 1.5f);
                m_device->drawString(
                    banner, TextStyle::Normal, fontSize * 1.5f, leftAlign - width / 2, topAlign, textColorOverlay);

                m_device->drawString(fmt::format("(this message will be displayed {} more time{})",
                                                 m_numSplashLeft,
                                                 m_numSplashLeft == 1 ? "" : "s"),
                                     TextStyle::Normal,
                                     fontSize,
                                     leftAlign - width / 2,
                                     topAlign + 1.55f * fontSize,
                                     textColorOverlay);

            } else if (m_state == MenuState::Visible) {
                // The actual menu.

                // Apply menu fade.
                const auto textColorNormal = MakeRGB24(ColorNormal) | alpha;
                const auto textColorHighlightText = MakeRGB24(ColorHighlightText) | alpha;
                const auto textColorSelected = MakeRGB24(ColorSelected) | alpha;
                const auto textColorHint = MakeRGB24(ColorHint) | alpha;
                const auto textColorWarning = MakeRGB24(ColorWarning) | alpha;

                // Measurements must be done in 2 steps: first mesure the necessary spacing for alignment of the values,
                // then measure the background area.
                const bool measureEntriesTitleWidth = m_resetTextLayout;
                const bool measureBackgroundWidth = !measureEntriesTitleWidth && m_resetBackgroundLayout;

                // Draw the background.
                if (!measureEntriesTitleWidth && !measureBackgroundWidth) {
                    m_device->clearColor(topAlign - BorderVerticalSpacing,
                                         leftAlign - BorderHorizontalSpacing,
                                         topAlign + m_menuHeaderHeight,
                                         leftAlign + m_menuBackgroundWidth + BorderHorizontalSpacing,
                                         XrColor4f{ColorHeader.r, ColorHeader.g, ColorHeader.b, alphaValue});
                    m_device->clearColor(
                        topAlign + m_menuHeaderHeight,
                        leftAlign - BorderHorizontalSpacing,
                        topAlign + m_menuHeaderHeight + HeaderLineWeight,
                        leftAlign + m_menuBackgroundWidth + BorderHorizontalSpacing,
                        XrColor4f{ColorHeaderSeparator.r, ColorHeaderSeparator.g, ColorHeaderSeparator.b, alphaValue});
                    m_device->clearColor(
                        topAlign + m_menuHeaderHeight + HeaderLineWeight,
                        leftAlign - BorderHorizontalSpacing,
                        topAlign + m_menuBackgroundHeight + BorderVerticalSpacing,
                        leftAlign + m_menuBackgroundWidth + BorderHorizontalSpacing,
                        XrColor4f{ColorBackground.r, ColorBackground.g, ColorBackground.b, alphaValue});
                }

                // To avoid flickering, we do this after drawing the background.
                if (measureEntriesTitleWidth) {
                    m_menuEntriesTitleWidth = 0.f;
                    m_resetTextLayout = false;
                }
                if (measureBackgroundWidth) {
                    m_menuBackgroundWidth = m_menuBackgroundHeight = m_menuHeaderHeight = 0.f;
                    m_resetBackgroundLayout = false;
                }

                float top = topAlign;

                // This is the title.
                m_device->drawString(
                    toolkit::LayerPrettyNameFull, TextStyle::Bold, fontSize * 0.75f, leftAlign, top, textColorHint);

                top += 1.2f * fontSize;

                // Display each menu entry.
                for (unsigned int i = 0; i < m_menuEntries.size(); i++) {
                    const auto& menuEntry = m_menuEntries[i];
                    const auto& title = m_resetArmed && (menuEntry.type == MenuEntryType::RestoreDefaults ||
                                                         menuEntry.type == MenuEntryType::ReloadShaders)
                                            ? "Confirm?"
                                            : menuEntry.title;

                    float left = leftAlign + (int)menuEntry.indent;

                    // Always account for entries, even invisible ones, when calculating the alignment.
                    float entryWidth = 0.0f;
                    if (measureEntriesTitleWidth) {
                        entryWidth = (int)menuEntry.indent + m_device->measureString(title, TextStyle::Bold, fontSize) +
                                     OptionSpacing;
                        m_menuEntriesTitleWidth = std::max(m_menuEntriesTitleWidth, entryWidth);
                    }

                    if (!menuEntry.visible) {
                        continue;
                    }

                    const auto entryColor = i == m_selectedItem ? textColorSelected : textColorNormal;

                    if (menuEntry.type != MenuEntryType::Tabs) {
                        m_device->drawString(title, TextStyle::Bold, fontSize, left, top, entryColor);
                        if (measureEntriesTitleWidth) {
                            left += entryWidth;
                        } else {
                            left += m_menuEntriesTitleWidth;
                        }
                    }

                    // Display the current value.
                    const int value = peekEntryValue(menuEntry);

                    switch (menuEntry.type) {
                    case MenuEntryType::Slider:
                        // clang-format off
                        {
                            // clang-format on
                            std::string label;
                            try {
                                label = menuEntry.valueToString(value);
                            } catch (std::exception&) {
                                label = "Error";
                            }
                            left += m_device->drawString(
                                label, TextStyle::Normal, fontSize, left, top, entryColor, measureBackgroundWidth);
                        }

                        left += ValueSpacing;

                        break;

                    case MenuEntryType::Tabs:
                    case MenuEntryType::Choice:
                        for (int j = menuEntry.minValue; j <= menuEntry.maxValue; j++) {
                            const auto style =
                                menuEntry.type == MenuEntryType::Tabs ? TextStyle::Bold : TextStyle::Normal;
                            const auto valueColor = value == j ? textColorHighlightText : entryColor;
                            const auto backgroundColor = i == m_selectedItem ? ColorSelected : ColorNormal;

                            const std::string label = menuEntry.valueToString(j);
                            const auto width = m_device->measureString(label, style, fontSize);

                            if (j == value) {
                                m_device->clearColor(
                                    top + SelectionVerticalSpacing,
                                    left - SelectionHorizontalSpacing,
                                    top + SelectionVerticalSpacing + 1.33f * fontSize - 1,
                                    left + width + SelectionHorizontalSpacing + 2,
                                    XrColor4f{backgroundColor.r, backgroundColor.g, backgroundColor.b, alphaValue});
                            }

                            m_device->drawString(label, style, fontSize, left, top, valueColor);
                            left += width + ValueSpacing;
                        }
                        break;

                    case MenuEntryType::Separator:
                        if (menuEntry.title.empty()) {
                            // Counteract the auto-down and add our own spacing.
                            top -= 1.5f * fontSize;
                            top += fontSize / 3;
                        }
                        break;

                    case MenuEntryType::RestoreDefaults:
                    case MenuEntryType::ReloadShaders:
                        break;

                    case MenuEntryType::ExitButton:
                        if (duration > 1.0 && duration <= 60.0 && isfinite(timeout)) {
                            left += m_device->drawString(fmt::format("({}s)", (int)(std::ceil(timeout - duration))),
                                                         TextStyle::Normal,
                                                         fontSize,
                                                         left,
                                                         top,
                                                         entryColor,
                                                         measureBackgroundWidth);
                        }
                        break;
                    }

                    top += 1.5f * fontSize;

                    if (measureBackgroundWidth) {
                        m_menuBackgroundWidth = std::max(m_menuBackgroundWidth, left - leftAlign);
                    }

                    if (menuEntry.type == MenuEntryType::Tabs) {
                        m_menuHeaderHeight = top - topAlign + 3;
                    }
                }

                {
                    float left = leftAlign;
                    if (m_configManager->isSafeMode()) {
                        top += fontSize;

                        left += m_device->drawString(L"\x26A0  Running in safe mode with defaults settings  \x26A0",
                                                     TextStyle::Bold,
                                                     fontSize,
                                                     leftAlign,
                                                     top,
                                                     textColorWarning,
                                                     measureBackgroundWidth);

                        top += 1.05f * fontSize;
                    } else if (m_needRestart) {
                        top += fontSize;

                        left += m_device->drawString(L"\x26A0  Restart the VR session to apply changes  \x26A0",
                                                     TextStyle::Bold,
                                                     fontSize,
                                                     leftAlign,
                                                     top,
                                                     textColorWarning,
                                                     measureBackgroundWidth);

                        top += 1.05f * fontSize;
                    }

                    if (measureBackgroundWidth) {
                        m_menuBackgroundWidth = std::max(m_menuBackgroundWidth, left - leftAlign);
                    }
                }

                {
                    float left = leftAlign;

                    // Create a little spacing.
                    top += 10;

                    left += m_device->drawString(
                        fmt::format(L"{0}  ( {1} : \xE2B6 )   ( {2} : \xE1FE)   ( {3} : \xE1FC)   ( {4} : \xE2B7 )",
                                    m_keyModifiersLabel,
                                    m_keyLeftLabel,
                                    m_keyUpLabel,
                                    m_keyMenuLabel,
                                    m_keyRightLabel),
                        TextStyle::Normal,
                        fontSize * 0.75f,
                        leftAlign,
                        top,
                        textColorHint,
                        measureBackgroundWidth);
                    top += 0.8f * fontSize;

                    m_device->drawString(L"Change values faster with SHIFT",
                                         TextStyle::Normal,
                                         fontSize * 0.75f,
                                         leftAlign,
                                         top,
                                         textColorHint);
                    top += 0.8f * fontSize;

                    if (measureBackgroundWidth) {
                        m_menuBackgroundWidth = std::max(m_menuBackgroundWidth, left - leftAlign);
                    }
                }
                m_menuBackgroundHeight = (top + fontSize * 0.2f) - topAlign;

            } // if (m_state == MenuState::Visible)

            auto overlayType = m_configManager->getEnumValue<OverlayType>(SettingOverlayType);
            if (m_state != MenuState::Splash && overlayType != OverlayType::None) {
                const auto textColorOverlayNoFade = MakeRGB24(ColorOverlay) | 0xff000000;
                const auto textColorRedNoFade = MakeRGB24(ColorWarning) | 0xff000000;

                const auto screenOffset = NdcToScreen({m_configManager->getValue(SettingOverlayXOffset) / 100.f,
                                                       m_configManager->getValue(SettingOverlayYOffset) / 100.f});

                const float overlayAlign = screenOffset.x * renderTargetInfo.width;
                float top = screenOffset.y * renderTargetInfo.height;

                // FPS display.
                m_device->drawString(fmt::format("FPS: {}", m_stats.fps),
                                     TextStyle::Normal,
                                     fontSize,
                                     (m_state != MenuState::Visible ? overlayAlign : rightAlign) - 300,
                                     m_state != MenuState::Visible ? top
                                                                   : topAlign - BorderVerticalSpacing - 1.1f * fontSize,
                                     textColorOverlayNoFade,
                                     true,
                                     FW1_LEFT);
                top += 1.05f * fontSize;

                if (m_state == MenuState::Visible) {
                    if (m_currentTab == MenuTab::Menu) {
                        // Display a reference to help aligning the overlay.
                        m_device->drawString("(overlay position)",
                                             TextStyle::Normal,
                                             fontSize,
                                             overlayAlign - 300,
                                             top,
                                             textColorOverlayNoFade,
                                             true,
                                             FW1_LEFT);
                    }
                } else {
#define OVERLAY_COMMON TextStyle::Normal, fontSize, overlayAlign - 300, top, textColorOverlayNoFade, true, FW1_LEFT

                    MotionReprojectionRate targetDivider;
                    if (m_isMotionReprojectionRateSupported &&
                        (targetDivider = m_configManager->peekEnumValue<MotionReprojectionRate>(
                             SettingMotionReprojectionRate)) != MotionReprojectionRate::Off) {
                        // Give 2 FPS headroom.
                        const auto targetFps = m_displayRefreshRate / (float)targetDivider;
                        if (m_stats.fps + 2 < targetFps) {
                            m_device->drawString("Target missed",
                                                 TextStyle::Normal,
                                                 fontSize,
                                                 overlayAlign - 300,
                                                 top,
                                                 textColorRedNoFade,
                                                 true,
                                                 FW1_LEFT);
                        } else {
                            const auto frameTimeMs = 1000.f / targetFps;
                            const auto headroomPercent = (int)((m_stats.waitCpuTimeUs / 10.f) / frameTimeMs);
                            const auto headroom = overlayType == OverlayType::Advanced
                                                      ? fmt::format("CPU headroom: +{}% ({:.1f}ms)",
                                                                    headroomPercent,
                                                                    m_stats.waitCpuTimeUs / 1000.f)
                                                      : fmt::format("CPU headroom: +{}%", headroomPercent);
                            m_device->drawString(headroom,
                                                 TextStyle::Normal,
                                                 fontSize,
                                                 overlayAlign - 300,
                                                 top,
                                                 headroomPercent < 15 ? textColorRedNoFade : textColorOverlayNoFade,
                                                 true,
                                                 FW1_LEFT);
                        }
                        top += 1.05f * fontSize;
                    }

                    // We give a little headroom to avoid flickering (hysteresis).
                    if (m_stats.appCpuTimeUs + 500 > m_stats.appGpuTimeUs) {
                        m_device->drawString(
                            fmt::format(
                                "CPU bound (+{:.1f}ms)",
                                std::max(0.f,
                                         ((int64_t)m_stats.appCpuTimeUs - (int64_t)m_stats.appGpuTimeUs) / 1000.f)),
                            TextStyle::Normal,
                            fontSize,
                            overlayAlign - 300,
                            top,
                            textColorRedNoFade,
                            true,
                            FW1_LEFT);
                    }
                    top += 1.05f * fontSize;

                    // Advanced display.
                    if (overlayType == OverlayType::Advanced || overlayType == OverlayType::Developer) {
#define TIMING_STAT(label, name)                                                                                       \
    m_device->drawString(fmt::format(label ": {}", m_stats.name), OVERLAY_COMMON);                                     \
    top += 1.05f * fontSize;

                        TIMING_STAT("app CPU", appCpuTimeUs);
                        TIMING_STAT("app GPU", appGpuTimeUs);
                        top += 1.05f * fontSize;

                        if (overlayType == OverlayType::Developer) {
                            TIMING_STAT("lay CPU", endFrameCpuTimeUs);
                            TIMING_STAT("pre GPU", processorGpuTimeUs[0]);
                            TIMING_STAT("scl GPU", processorGpuTimeUs[1]);
                            TIMING_STAT("pst GPU", processorGpuTimeUs[2]);
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

                            m_device->drawString(fmt::format("biased: {}", m_stats.numBiasedSamplers), OVERLAY_COMMON);
                            top += 1.05f * fontSize;
                            m_device->drawString(fmt::format("VRS RTV: {}", m_stats.numRenderTargetsWithVRS),
                                                 OVERLAY_COMMON);
                            top += 1.05f * fontSize;

#undef TIMING_STAT

                            top += 1.05f * fontSize;

#define GESTURE_STATE(label, name)                                                                                     \
    if (!isnan(m_gesturesState.name##Value[0]) || !isnan(m_gesturesState.name##Value[1])) {                            \
        m_device->drawString(                                                                                          \
            fmt::format(label ": {:.2f}/{:.2f}", m_gesturesState.name##Value[0], m_gesturesState.name##Value[1]),      \
            OVERLAY_COMMON);                                                                                           \
        top += 1.05f * fontSize;                                                                                       \
    }

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

                                m_device->drawString(fmt::format("hptf: {:.3f}/{:.3f}",
                                                                 m_gesturesState.hapticsFrequency[0],
                                                                 m_gesturesState.hapticsFrequency[1]),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;
                                m_device->drawString(fmt::format("hptd: {:.1f}/{:.1f}",
                                                                 m_gesturesState.hapticsDurationUs[0] / 1000000.0f,
                                                                 m_gesturesState.hapticsDurationUs[1] / 1000000.0f),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;

                                m_device->drawString(fmt::format("loss: {}/{}",
                                                                 m_gesturesState.numTrackingLosses[0] % 256,
                                                                 m_gesturesState.numTrackingLosses[1] % 256),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;

                                m_device->drawString(fmt::format("cache: {}/{}",
                                                                 m_gesturesState.cacheSize[0],
                                                                 m_gesturesState.cacheSize[1]),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;

                                m_device->drawString(fmt::format("age: {:.1f}/{:.1f}",
                                                                 m_gesturesState.handposeAgeUs[0] / 1000000.0f,
                                                                 m_gesturesState.handposeAgeUs[1] / 1000000.0f),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;
                            }
#undef GESTURE_STATE

                            if (isEyeTrackingEnabled()) {
                                m_device->drawString(fmt::format("gaze: {:.3f},{:.3f},{:.3f}",
                                                                 m_eyeGazeState.gazeRay.x,
                                                                 m_eyeGazeState.gazeRay.y,
                                                                 m_eyeGazeState.gazeRay.z),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;

                                m_device->drawString(fmt::format("eye.l: {:.3f},{:.3f}",
                                                                 m_eyeGazeState.leftPoint.x,
                                                                 m_eyeGazeState.leftPoint.y),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;

                                m_device->drawString(fmt::format("eye.r: {:.3f},{:.3f}",
                                                                 m_eyeGazeState.rightPoint.x,
                                                                 m_eyeGazeState.rightPoint.y),
                                                     OVERLAY_COMMON);
                                top += 1.05f * fontSize;
                            }
                        }
                    }
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

        void updateEyeGazeState(const input::EyeGazeState& state) override {
            m_eyeGazeState = state;
        }

        bool isVisible() const {
            return m_state != MenuState::NotVisible ||
                   m_configManager->getEnumValue<OverlayType>(SettingOverlayType) != OverlayType::None;
        }

      private:
        friend class MenuGroup;

        void setupPerformanceTab(const MenuInfo& menuInfo) {
            MenuGroup performanceTab(
                this, [&] { return m_currentTab == MenuTab::Performance; }, true);

            // Performance Overlay Settings.
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Overlay",
                                     MenuEntryType::Choice,
                                     SettingOverlayType,
                                     0,
                                     MenuEntry::LastVal<OverlayType>(),
                                     MenuEntry::FmtEnum<OverlayType>});

            // Upscaling Settings.
            m_originalScalingType = getCurrentScalingType();
            m_originalScalingValue = getCurrentScaling();
            m_originalAnamorphicValue = getCurrentAnamorphic();
            m_useAnamorphic = m_originalAnamorphicValue > 0 ? 1 : 0;

            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Upscaling",
                                     MenuEntryType::Choice,
                                     SettingScalingType,
                                     0,
                                     MenuEntry::LastVal<ScalingType>(),
                                     MenuEntry::FmtEnum<ScalingType>});
            m_menuEntries.back().noCommitDelay = true;

            // Scaling sub-group.
            MenuGroup upscalingGroup(this, [&] { return getCurrentScalingType() != ScalingType::None; });
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Anamorphic",
                                     MenuEntryType::Choice,
                                     "",
                                     0,
                                     MenuEntry::LastVal<OffOnType>(),
                                     MenuEntry::FmtEnum<OffOnType>});
            m_menuEntries.back().noCommitDelay = true;
            m_menuEntries.back().pValue = &m_useAnamorphic;

            // Proportional sub-group.
            MenuGroup proportionalGroup(
                this, [&] { return !m_useAnamorphic && getCurrentScalingType() != ScalingType::None; });
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent, "Size", MenuEntryType::Slider, SettingScaling, 25, 400, [&](int value) {
                     return fmt::format("{}% ({}x{})",
                                        value,
                                        GetScaledInputSize(getDisplayWidth(), value, 2),
                                        GetScaledInputSize(getDisplayHeight(), value, 2));
                 }});
            m_menuEntries.back().noCommitDelay = true;
            proportionalGroup.finalize();

            // Anamorphic sub-group.
            MenuGroup anamorphicGroup(this,
                                      [&] { return m_useAnamorphic && getCurrentScalingType() != ScalingType::None; });
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent, "Width", MenuEntryType::Slider, SettingScaling, 25, 400, [&](int value) {
                     return fmt::format("{}% ({} pixels)", value, GetScaledInputSize(getDisplayWidth(), value, 2));
                 }});
            m_menuEntries.back().noCommitDelay = true;

            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent,
                 "Height",
                 MenuEntryType::Slider,
                 SettingAnamorphic,
                 25,
                 400,
                 [&](int value) {
                     return fmt::format("{}% ({} pixels)", value, GetScaledInputSize(getDisplayHeight(), value, 2));
                 }});
            m_menuEntries.back().noCommitDelay = true;
            anamorphicGroup.finalize();

            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Sharpness",
                                     MenuEntryType::Slider,
                                     SettingSharpness,
                                     0,
                                     100,
                                     MenuEntry::FmtPercent});
            // TODO: Mip-map biasing is only support on D3D11.
            if (m_device->getApi() == Api::D3D11) {
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Mip-map bias",
                                         MenuEntryType::Slider,
                                         SettingMipMapBias,
                                         0,
                                         MenuEntry::LastVal<MipMapBias>(),
                                         MenuEntry::FmtEnum<MipMapBias>});
                m_menuEntries.back().expert = true;
            }
            upscalingGroup.finalize();

            // Fixed Foveated Rendering (VRS) Settings.
            if (menuInfo.variableRateShaderMaxRate) {
                m_menuEntries.push_back({MenuIndent::OptionIndent,
                                         !m_isEyeTrackingSupported ? "Fixed foveated rendering" : "Foveated rendering",
                                         MenuEntryType::Choice,
                                         SettingVRS,
                                         0,
                                         MenuEntry::LastVal<VariableShadingRateType>(),
                                         MenuEntry::FmtEnum<VariableShadingRateType>});

                // Common sub-group.
                MenuGroup variableRateShaderCommonGroup(this, [&] {
                    return m_configManager->peekEnumValue<VariableShadingRateType>(SettingVRS) !=
                           VariableShadingRateType::None;
                });
                if (m_isEyeTrackingSupported) {
                    // Eye tracking sub-group.
                    MenuGroup variableRateShaderEyeTrackingGroup(this, [&] {
                        // We only show eye tracking availability if we are able to distinguish left/right eyes.
                        return m_stats.hasColorBuffer[0] && m_stats.hasColorBuffer[1];
                    });
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Eye tracking",
                                             MenuEntryType::Choice,
                                             SettingEyeTrackingEnabled,
                                             0,
                                             MenuEntry::LastVal<OffOnType>(),
                                             MenuEntry::FmtEnum<OffOnType>});
                    m_originalEyeTrackingEnabled = isEyeTrackingEnabled();
                    variableRateShaderEyeTrackingGroup.finalize();

                    // Eye tracking settings sub-group.
                    MenuGroup variableRateShaderEyeTrackingSettingsGroup(
                        this, [&] { return m_configManager->peekValue(SettingEyeTrackingEnabled); });
                    if (menuInfo.isEyeTrackingProjectionDistanceSupported) {
                        m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                                 "Eye projection distance",
                                                 MenuEntryType::Slider,
                                                 SettingEyeProjectionDistance,
                                                 10,
                                                 10000,
                                                 [](int value) { return fmt::format("{:.2f}m", value / 100.f); }});
                        m_menuEntries.back().acceleration = 5;
                    }
                    variableRateShaderEyeTrackingSettingsGroup.finalize();
                }
                variableRateShaderCommonGroup.finalize();

                // Preset sub-group.
                MenuGroup variableRateShaderPresetGroup(this, [&] {
                    return m_configManager->peekEnumValue<VariableShadingRateType>(SettingVRS) ==
                           VariableShadingRateType::Preset;
                });
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Mode",
                                         MenuEntryType::Slider,
                                         SettingVRSQuality,
                                         0,
                                         MenuEntry::LastVal<VariableShadingRateQuality>(),
                                         MenuEntry::FmtEnum<VariableShadingRateQuality>});
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Pattern",
                                         MenuEntryType::Slider,
                                         SettingVRSPattern,
                                         0,
                                         MenuEntry::LastVal<VariableShadingRatePattern>(),
                                         MenuEntry::FmtEnum<VariableShadingRatePattern>});
                variableRateShaderPresetGroup.finalize();

                // Custom sub-group.
                MenuGroup variableRateShaderCustomGroup(this, [&] {
                    return m_configManager->peekEnumValue<VariableShadingRateType>(SettingVRS) ==
                           VariableShadingRateType::Custom;
                });
                {
                    const auto maxVRSLeftRightBias =
                        std::min(int(menuInfo.variableRateShaderMaxRate), to_integral(VariableShadingRateVal::R_4x4));

                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Inner resolution",
                                             MenuEntryType::Slider,
                                             SettingVRSInner,
                                             0,
                                             menuInfo.variableRateShaderMaxRate,
                                             MenuEntry::FmtEnum<VariableShadingRateVal>});
                    m_menuEntries.back().expert = true;
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Inner ring size",
                                             MenuEntryType::Slider,
                                             SettingVRSInnerRadius,
                                             0,
                                             100,
                                             MenuEntry::FmtPercent});
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Middle resolution",
                                             MenuEntryType::Slider,
                                             SettingVRSMiddle,
                                             1, // Exclude 1x to discourage people from using poor settings!
                                             menuInfo.variableRateShaderMaxRate,
                                             MenuEntry::FmtEnum<VariableShadingRateVal>});
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Outer ring size",
                                             MenuEntryType::Slider,
                                             SettingVRSOuterRadius,
                                             0,
                                             100,
                                             MenuEntry::FmtPercent});
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Outer resolution",
                                             MenuEntryType::Slider,
                                             SettingVRSOuter,
                                             1, // Exclude 1x to discourage people from using poor settings!
                                             menuInfo.variableRateShaderMaxRate,
                                             MenuEntry::FmtEnum<VariableShadingRateVal>});
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Prefer resolution",
                                             MenuEntryType::Choice,
                                             SettingVRSPreferHorizontal,
                                             0,
                                             MenuEntry::LastVal<VariableShadingRateDir>(),
                                             MenuEntry::FmtEnum<VariableShadingRateDir>});
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Horizontal scale",
                                             MenuEntryType::Slider,
                                             SettingVRSXScale,
                                             10,
                                             200,
                                             MenuEntry::FmtPercent});
                    m_menuEntries.back().expert = true;
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Horizontal offset",
                                             MenuEntryType::Slider,
                                             SettingVRSXOffset,
                                             -100,
                                             100,
                                             MenuEntry::FmtPercent});
                    m_menuEntries.back().expert = true;
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Vertical offset",
                                             MenuEntryType::Slider,
                                             SettingVRSYOffset,
                                             -100,
                                             100,
                                             MenuEntry::FmtPercent});
                    m_menuEntries.back().expert = true;
                    m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                             "Left/Right Bias",
                                             MenuEntryType::Slider,
                                             SettingVRSLeftRightBias,
                                             -maxVRSLeftRightBias,
                                             maxVRSLeftRightBias,
                                             MenuEntry::FmtVrsRate});
                    m_menuEntries.back().expert = true;
                }
                variableRateShaderCustomGroup.finalize();
            }

            // Must be kept last.
            performanceTab.finalize();
        }

        void setupAppearanceTab(const MenuInfo& menuInfo) {
            MenuGroup appearanceTab(
                this, [&] { return m_currentTab == MenuTab::Appearance; }, true);

            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Post Processing",
                                     MenuEntryType::Choice,
                                     SettingPostProcess,
                                     0,
                                     MenuEntry::LastVal<PostProcessType>(),
                                     MenuEntry::FmtEnum<PostProcessType>});
            MenuGroup postProcessGroup(this, [&] {
                return m_configManager->peekEnumValue<PostProcessType>(SettingPostProcess) != PostProcessType::Off;
            });
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Sun Glasses",
                                     MenuEntryType::Choice,
                                     SettingPostSunGlasses,
                                     0,
                                     MenuEntry::LastVal<PostSunGlassesType>(),
                                     MenuEntry::FmtEnum<PostSunGlassesType>});
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Contrast",
                                     MenuEntryType::Slider,
                                     SettingPostContrast,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Brightness",
                                     MenuEntryType::Slider,
                                     SettingPostBrightness,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Exposure",
                                     MenuEntryType::Slider,
                                     SettingPostExposure,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Saturation",
                                     MenuEntryType::Slider,
                                     SettingPostSaturation,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Vibrance",
                                     MenuEntryType::Slider,
                                     SettingPostVibrance,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Highlights",
                                     MenuEntryType::Slider,
                                     SettingPostHighlights,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Shadows",
                                     MenuEntryType::Slider,
                                     SettingPostShadows,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            postProcessGroup.finalize();

            m_menuEntries.push_back(
                {MenuIndent::OptionIndent, "World scale", MenuEntryType::Slider, SettingICD, 1, 10000, [&](int value) {
                     return fmt::format("{:.1f}% ({:.1f}mm)", value / 10.f, m_stats.icd * 1000);
                 }});
            m_menuEntries.back().acceleration = 5;

            // Must be kept last.
            appearanceTab.finalize();
        }

        void setupInputsTab(const MenuInfo& menuInfo) {
            MenuGroup inputsTab(
                this, [&] { return m_currentTab == MenuTab::Inputs; }, true);

            if (menuInfo.isPredictionDampeningSupported) {
                m_menuEntries.push_back({MenuIndent::OptionIndent,
                                         "Shaking reduction",
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
            }

            if (m_isHandTrackingSupported) {
                m_menuEntries.push_back({MenuIndent::OptionIndent,
                                         "Controller emulation",
                                         MenuEntryType::Choice,
                                         SettingHandTrackingEnabled,
                                         0,
                                         MenuEntry::LastVal<HandTrackingEnabled>(),
                                         MenuEntry::FmtEnum<HandTrackingEnabled>});
                m_originalHandTrackingEnabled = isHandTrackingEnabled();

                MenuGroup handTrackingGroup(this, [&] { return isHandTrackingEnabled(); });
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Hand skeleton",
                                         MenuEntryType::Slider,
                                         SettingHandVisibilityAndSkinTone,
                                         0,
                                         MenuEntry::LastVal<HandTrackingVisibility>(),
                                         MenuEntry::FmtEnum<HandTrackingVisibility>});
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Controller timeout",
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

        void setupSystemTab(const MenuInfo& menuInfo) {
            MenuGroup systemTab(
                this, [&] { return m_currentTab == MenuTab::System; }, true);

            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Override resolution",
                                     MenuEntryType::Choice,
                                     SettingResolutionOverride,
                                     0,
                                     MenuEntry::LastVal<NoYesType>(),
                                     MenuEntry::FmtEnum<NoYesType>});

            MenuGroup resolutionGroup(this, [&] { return m_configManager->peekValue(SettingResolutionOverride); });
            m_originalResolutionWidth = m_displayWidth;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Display resolution (per eye)",
                                     MenuEntryType::Slider,
                                     SettingResolutionWidth,
                                     500,
                                     static_cast<int>(menuInfo.maxDisplayWidth),
                                     [&](int value) {
                                         return fmt::format(
                                             "{}x{}", value, static_cast<int>(value * m_resolutionHeightRatio));
                                     }});
            m_menuEntries.back().acceleration = 10;
            resolutionGroup.finalize();

            if (m_isMotionReprojectionRateSupported) {
                m_originalMotionReprojectionEnabled = isMotionReprojectionEnabled();
                m_menuEntries.push_back({MenuIndent::OptionIndent,
                                         "Motion reprojection",
                                         MenuEntryType::Choice,
                                         SettingMotionReprojection,
                                         0,
                                         MenuEntry::LastVal<MotionReprojection>(),
                                         MenuEntry::FmtEnum<MotionReprojection>});

                MenuGroup motionReprojectionGroup(this, [&] { return isMotionReprojectionEnabled(); });
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Lock motion reprojection",
                                         MenuEntryType::Slider,
                                         SettingMotionReprojectionRate,
                                         to_integral(MotionReprojectionRate::Off),
                                         MenuEntry::LastVal<MotionReprojectionRate>(),
                                         [&](int value) {
                                             return value == to_integral(MotionReprojectionRate::Off)
                                                        ? "Unlocked"
                                                        : fmt::format("{:.1f} fps",
                                                                      static_cast<float>(m_displayRefreshRate) / value);
                                         }});
                motionReprojectionGroup.finalize();
            }

            m_menuEntries.push_back(
                {MenuIndent::NoIndent, "Color Gains:", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Red",
                                     MenuEntryType::Slider,
                                     SettingPostColorGainR,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Green",
                                     MenuEntryType::Slider,
                                     SettingPostColorGainG,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;
            m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                     "Blue",
                                     MenuEntryType::Slider,
                                     SettingPostColorGainB,
                                     0,
                                     1000,
                                     MenuEntry::FmtDecimal<1>});
            m_menuEntries.back().acceleration = 5;

            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Field of view",
                                     MenuEntryType::Choice,
                                     SettingFOVType,
                                     0,
                                     MenuEntry::LastVal<FovModeType>(),
                                     MenuEntry::FmtEnum<FovModeType>});
            MenuGroup fovSimpleGroup(this, [&] {
                return m_configManager->peekEnumValue<FovModeType>(SettingFOVType) == FovModeType::Simple;
            });
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent, "Adjustment", MenuEntryType::Slider, SettingFOV, 50, 100, [&](int value) {
                     return fmt::format(
                         "{}% ({:.1f}\xB0)", value, m_stats.fov[1].angleRight - m_stats.fov[0].angleLeft);
                 }});
            fovSimpleGroup.finalize();

            MenuGroup fovAdvancedGroup(this, [&] {
                return m_configManager->peekEnumValue<FovModeType>(SettingFOVType) == FovModeType::Advanced;
            });
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent, "Up", MenuEntryType::Slider, SettingFOVUp, 50, 100, [&](int value) {
                     return fmt::format(
                         "{}% ({:.1f}\xB0/{:.1f}\xB0)", value, m_stats.fov[0].angleUp, m_stats.fov[1].angleUp);
                 }});
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent, "Down", MenuEntryType::Slider, SettingFOVDown, 50, 100, [&](int value) {
                     return fmt::format(
                         "{}% ({:.1f}\xB0/{:.1f}\xB0)", value, m_stats.fov[0].angleDown, m_stats.fov[1].angleDown);
                 }});
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent,
                 "Left/Left",
                 MenuEntryType::Slider,
                 SettingFOVLeftLeft,
                 50,
                 100,
                 [&](int value) { return fmt::format("{}% ({:.1f}\xB0)", value, m_stats.fov[0].angleLeft); }});
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent,
                 "Left/Right",
                 MenuEntryType::Slider,
                 SettingFOVLeftRight,
                 50,
                 100,
                 [&](int value) { return fmt::format("{}% ({:.1f}\xB0)", value, m_stats.fov[0].angleRight); }});
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent,
                 "Right/Left",
                 MenuEntryType::Slider,
                 SettingFOVRightLeft,
                 50,
                 100,
                 [&](int value) { return fmt::format("{}% ({:.1f}\xB0)", value, m_stats.fov[1].angleLeft); }});
            m_menuEntries.push_back(
                {MenuIndent::SubGroupIndent,
                 "Right/Right",
                 MenuEntryType::Slider,
                 SettingFOVRightRight,
                 50,
                 100,
                 [&](int value) { return fmt::format("{}% ({:.1f}\xB0)", value, m_stats.fov[1].angleRight); }});
            fovAdvancedGroup.finalize();

            if (menuInfo.isPimaxFovHackSupported) {
                m_menuEntries.push_back({MenuIndent::SubGroupIndent,
                                         "Pimax WFOV Hack",
                                         MenuEntryType::Choice,
                                         SettingPimaxFOVHack,
                                         0,
                                         MenuEntry::LastVal<OffOnType>(),
                                         MenuEntry::FmtEnum<OffOnType>});
            }

            // Must be kept last.
            systemTab.finalize();
        }

        void setupMenuTab(const MenuInfo& menuInfo) {
            MenuGroup menuTab(
                this, [&] { return m_currentTab == MenuTab::Menu; }, true);

            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Show expert settings",
                                     MenuEntryType::Choice,
                                     SettingMenuExpert,
                                     0,
                                     MenuEntry::LastVal<NoYesType>(),
                                     MenuEntry::FmtEnum<NoYesType>});
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Font size",
                                     MenuEntryType::Slider,
                                     SettingMenuFontSize,
                                     8,
                                     72,
                                     [&](int value) { return fmt::format("{}pt", value); }});
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Menu timeout",
                                     MenuEntryType::Slider,
                                     SettingMenuTimeout,
                                     0,
                                     MenuEntry::LastVal<MenuTimeout>(),
                                     MenuEntry::FmtEnum<MenuTimeout>});
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Menu distance",
                                     MenuEntryType::Slider,
                                     SettingMenuDistance,
                                     30,
                                     400,
                                     [](int value) { return fmt::format("{:.2f}m", value / 100.f); }});
            m_menuEntries.back().acceleration = 10;
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Overlay horizontal offset",
                                     MenuEntryType::Slider,
                                     SettingOverlayXOffset,
                                     -100,
                                     100,
                                     MenuEntry::FmtPercent});
            m_menuEntries.back().expert = true;
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Overlay vertical offset",
                                     MenuEntryType::Slider,
                                     SettingOverlayYOffset,
                                     -100,
                                     100,
                                     MenuEntry::FmtPercent});
            m_menuEntries.back().expert = true;

            m_menuEntries.push_back(
                {MenuIndent::OptionIndent, "Restore defaults", MenuEntryType::RestoreDefaults, BUTTON_OR_SEPARATOR});

            // Must be kept last.
            menuTab.finalize();
        }

        void setupDeveloperTab(const MenuInfo& menuInfo) {
            if (!m_configManager->getValue(SettingDeveloper)) {
                return;
            }

            MenuGroup menuTab(
                this, [&] { return m_currentTab == MenuTab::Developer; }, true);

            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Simulate canting",
                                     MenuEntryType::Slider,
                                     "canting",
                                     -90,
                                     90,
                                     [](int value) { return fmt::format("{}\xB0", value); }});
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Simulate eye tracker*",
                                     MenuEntryType::Choice,
                                     SettingEyeDebugWithController,
                                     0,
                                     MenuEntry::LastVal<OffOnType>(),
                                     MenuEntry::FmtEnum<OffOnType>});
            m_menuEntries.push_back({MenuIndent::OptionIndent,
                                     "Show eye gaze",
                                     MenuEntryType::Choice,
                                     SettingEyeDebug,
                                     0,
                                     MenuEntry::LastVal<OffOnType>(),
                                     MenuEntry::FmtEnum<OffOnType>});

            m_menuEntries.push_back(
                {MenuIndent::OptionIndent, "Reload Shaders", MenuEntryType::ReloadShaders, BUTTON_OR_SEPARATOR});

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

        bool isEyeTrackingEnabled() const {
            return m_isEyeTrackingSupported && m_configManager->peekValue(SettingEyeTrackingEnabled);
        }

        bool isMotionReprojectionEnabled() const {
            return m_isMotionReprojectionRateSupported && m_configManager->peekEnumValue<MotionReprojection>(
                                                              SettingMotionReprojection) == MotionReprojection::On;
        }

        uint32_t getDisplayWidth() const {
            return m_configManager->peekValue(SettingResolutionOverride)
                       ? m_configManager->peekValue(SettingResolutionWidth)
                       : m_displayWidth;
        }

        uint32_t getDisplayHeight() const {
            return m_configManager->peekValue(SettingResolutionOverride)
                       ? (uint32_t)(m_configManager->peekValue(SettingResolutionWidth) * m_resolutionHeightRatio)
                       : m_displayHeight;
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
                m_originalScalingType != getCurrentScalingType() ||
                m_originalEyeTrackingEnabled != isEyeTrackingEnabled()) {
                return true;
            }

            if ((m_originalScalingType != ScalingType::None) && (m_originalScalingValue != getCurrentScaling() ||
                                                                 m_originalAnamorphicValue != getCurrentAnamorphic())) {
                return true;
            }

            if (m_configManager->peekValue(SettingResolutionOverride) &&
                m_originalResolutionWidth != m_configManager->peekValue(SettingResolutionWidth)) {
                return true;
            }

            if (m_originalMotionReprojectionEnabled != isMotionReprojectionEnabled()) {
                return true;
            }

            return false;
        }

        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_displayWidth;
        const uint32_t m_displayHeight;
        const bool m_isHandTrackingSupported;
        const bool m_isEyeTrackingSupported;
        const float m_resolutionHeightRatio;
        const bool m_isMotionReprojectionRateSupported;
        const uint8_t m_displayRefreshRate;
        const bool m_supportFOVHack;
        MenuStatistics m_stats{};
        GesturesState m_gesturesState{};
        EyeGazeState m_eyeGazeState{};

        std::vector<int> m_keyModifiers;
        std::wstring m_keyModifiersLabel;
        int m_keyLeft;
        std::wstring m_keyLeftLabel;
        int m_keyRight;
        std::wstring m_keyRightLabel;
        int m_keyMenu;
        std::wstring m_keyMenuLabel;
        int m_keyUp;
        std::wstring m_keyUpLabel;

        int m_numSplashLeft;
        std::vector<MenuEntry> m_menuEntries;
        std::vector<MenuGroup> m_menuGroups;
        size_t m_selectedItem{0};
        MenuTab m_currentTab{MenuTab::Performance};
        std::chrono::steady_clock::time_point m_lastInput;
        bool m_moveLeftKeyState{false};
        bool m_moveRightKeyState{false};
        bool m_menuControlKeyState{false};
        bool m_moveUpKeyState{false};
        bool m_resetArmed{false};

        // animation control
        bool m_isAccelerating{false};

        ScalingType m_originalScalingType{ScalingType::None};
        int m_originalScalingValue{0};
        int m_originalAnamorphicValue{0};
        int m_useAnamorphic{0};

        bool m_originalHandTrackingEnabled{false};
        bool m_originalEyeTrackingEnabled{false};
        int m_originalResolutionWidth{0};
        bool m_originalMotionReprojectionEnabled{false};
        bool m_needRestart{false};

        mutable MenuState m_state{MenuState::NotVisible};
        mutable float m_menuEntriesTitleWidth{0.0f};
        mutable float m_menuBackgroundWidth{0.0f};
        mutable float m_menuBackgroundHeight{0.0f};
        mutable float m_menuHeaderHeight{0.0f};
        mutable bool m_resetTextLayout{true};
        mutable bool m_resetBackgroundLayout{true};
    };

    template <typename E>
    int MenuEntry::LastVal() {
        return to_integral(E::MaxValue) - 1;
    }

    template <typename E>
    std::string MenuEntry::FmtEnum(int value) {
        return std::string(config::to_string_view(static_cast<E>(value)));
    }

    std::string MenuEntry::FmtPercent(int value) {
        return fmt::format("{}%", value);
    }

    template <size_t N>
    std::string MenuEntry::FmtDecimal(int value) {
        const uint32_t pow10 = N == 0 ? 1u : N == 1 ? 10u : N == 2 ? 100u : 1000u; // crude but working
        auto prec = value % pow10;
        return fmt::format("{:.{}f}", static_cast<float>(value) / pow10, prec ? N : 0);
    }

    std::string MenuEntry::FmtPostVal(int value) {
        if (value == 0)
            return "min";
        if (value == 1000)
            return "max";
        if (value == 500)
            return "neutral";
        return FmtDecimal<1>(value);
    }

    std::string MenuEntry::FmtVrsRate(int value) {
        auto idx = (std::clamp(value, -4, 4) + 4) * 4;
        return std::string(&"+4 L+3 L+2 L+1 Lnone+1 R+2 R+3 R+4 R"[idx], 4);
    }

    std::string MenuEntry::FmtNone(int) {
        return "";
    }

    MenuGroup::MenuGroup(MenuHandler* menu, std::function<bool()> isVisible, bool isTab)
        : m_menu(menu), m_isVisible(isVisible), m_start(menu->m_menuEntries.size()), m_count(0), m_isTab(isTab) {
    }

    void MenuGroup::finalize() {
        // Tabs must always be evaluated first.
        m_count = m_menu->m_menuEntries.size() - m_start;
        m_menu->m_menuGroups.insert(m_isTab ? m_menu->m_menuGroups.begin() : m_menu->m_menuGroups.end(), *this);
    }

    void MenuGroup::updateVisibility(bool showExpert) const {
        int groupVisible = -1; // callback once only when needed
        std::for_each_n(&m_menu->m_menuEntries[m_start], m_count, [&](auto& it) {
            auto canShow = (m_isTab || it.visible) && (!it.expert || showExpert);
            if (canShow && groupVisible < 0)
                groupVisible = m_isVisible() ? 1 : 0;
            it.visible = canShow && groupVisible;
        });
    };

} // namespace

namespace toolkit::menu {
    std::shared_ptr<IMenuHandler> CreateMenuHandler(std::shared_ptr<toolkit::config::IConfigManager> configManager,
                                                    std::shared_ptr<toolkit::graphics::IDevice> device,
                                                    const MenuInfo& menuInfo) {
        return std::make_shared<MenuHandler>(configManager, device, menuInfo);
    }

} // namespace toolkit::menu
