// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
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

    enum class TextStyle { Normal, Selected };

    // A generic text rendering interface.
    struct ITextRenderer {
        virtual ~ITextRenderer() = default;

        virtual void begin(std::shared_ptr<ITexture> renderTarget,
                           float leftEyeOffset = 0.0f,
                           float rightEyeOffset = 0.0f) = 0;
        virtual void end() = 0;

        virtual void clear(float top, float left, float bottom, float right) const = 0;

        virtual float drawString(std::wstring string,
                                 TextStyle style,
                                 float size,
                                 float x,
                                 float y,
                                 uint32_t color,
                                 bool measure = false,
                                 bool alignRight = false) const = 0;
        virtual float drawString(std::string string,
                                 TextStyle style,
                                 float size,
                                 float x,
                                 float y,
                                 uint32_t color,
                                 bool measure = false,
                                 bool alignRight = false) const = 0;
        virtual float measureString(std::wstring string, TextStyle style, float size) const = 0;
        virtual float measureString(std::string string, TextStyle style, float size) const = 0;
    };

    // A D3D11 text rendered based on FW1FontWrapper.
    class D3D11TextRenderer : public ITextRenderer {
      private:
        const std::wstring FontFamily = L"Arial";

      public:
        D3D11TextRenderer(std::shared_ptr<IDevice> device) : m_device(device) {
            CHECK_HRCMD(FW1CreateFactory(FW1_VERSION, &m_fontWrapperFactory));

            CHECK_HRCMD(
                m_fontWrapperFactory->CreateFontWrapper(device->getNative<D3D11>(), FontFamily.c_str(), &m_fontNormal));

            IDWriteFactory* dwriteFactory = nullptr;
            CHECK_HRCMD(m_fontNormal->GetDWriteFactory(&dwriteFactory));
            FW1_FONTWRAPPERCREATEPARAMS params;
            ZeroMemory(&params, sizeof(params));
            params.DefaultFontParams.pszFontFamily = FontFamily.c_str();
            params.DefaultFontParams.FontWeight = DWRITE_FONT_WEIGHT_BOLD;
            params.DefaultFontParams.FontStretch = DWRITE_FONT_STRETCH_NORMAL;
            params.DefaultFontParams.FontStyle = DWRITE_FONT_STYLE_NORMAL;
            CHECK_HRCMD(m_fontWrapperFactory->CreateFontWrapper(
                device->getNative<D3D11>(), dwriteFactory, &params, &m_fontSelected));
        }

        void begin(std::shared_ptr<ITexture> renderTarget, float leftEyeOffset, float rightEyeOffset) override {
            m_renderTarget = renderTarget;
            m_leftEyeOffset = leftEyeOffset;
            m_rightEyeOffset = rightEyeOffset;
        }

        void end() override {
            if (m_deferredContext) {
                m_fontNormal->Flush(m_deferredContext.Get());
                m_fontSelected->Flush(m_deferredContext.Get());

                ComPtr<ID3D11CommandList> commandList;
                CHECK_HRCMD(m_deferredContext->FinishCommandList(FALSE, &commandList));

                m_device->getContext<D3D11>()->ExecuteCommandList(commandList.Get(), TRUE);
                m_deferredContext = nullptr;
                m_renderTarget = nullptr;
            }
        }

        void clear(float top, float left, float bottom, float right) const override {
            ComPtr<ID3D11DeviceContext1> d3d11Context;
            if (FAILED(m_device->getContext<D3D11>()->QueryInterface(
                    __uuidof(ID3D11DeviceContext1), reinterpret_cast<void**>(d3d11Context.GetAddressOf())))) {
                // The app did not use a sufficient FEATURE_LEVEL. Nothing we can do.
                return;
            }

            float clearColor[] = {0, 0, 0, 1};
            D3D11_RECT rect;
            rect.top = (LONG)top;
            rect.left = (LONG)left;
            rect.bottom = (LONG)bottom;
            rect.right = (LONG)right;
            if (!m_renderTarget->isArray()) {
                d3d11Context->ClearView(
                    m_renderTarget->getRenderTargetView()->getNative<D3D11>(), clearColor, &rect, 1);
            } else {
                // When VPRT is used, draw each eye.
                {
                    D3D11_RECT correctedRect = rect;
                    correctedRect.left += (LONG)m_leftEyeOffset;
                    correctedRect.right += (LONG)m_leftEyeOffset;
                    d3d11Context->ClearView(
                        m_renderTarget->getRenderTargetView(0)->getNative<D3D11>(), clearColor, &rect, 1);
                }
                {
                    D3D11_RECT correctedRect = rect;
                    correctedRect.left += (LONG)m_rightEyeOffset;
                    correctedRect.right += (LONG)m_rightEyeOffset;
                    d3d11Context->ClearView(
                        m_renderTarget->getRenderTargetView(1)->getNative<D3D11>(), clearColor, &rect, 1);
                }
            }
        }

        float drawString(std::wstring string,
                         TextStyle style,
                         float size,
                         float x,
                         float y,
                         uint32_t color,
                         bool measure,
                         bool alignRight) const override {
            // Upon first call for this frame, we create the deferred context.
            // This is needed because FW1_RESTORESTATE won't help us in the case of multiple consecutive draw calls.
            if (!m_deferredContext) {
                CHECK_HRCMD(m_device->getNative<D3D11>()->CreateDeferredContext(0, &m_deferredContext));

                m_deferredContext->ClearState();

                if (!m_renderTarget->isArray()) {
                    ID3D11RenderTargetView* rtvs[] = {m_renderTarget->getRenderTargetView()->getNative<D3D11>()};
                    m_deferredContext->OMSetRenderTargets(1, rtvs, nullptr);
                }

                D3D11_VIEWPORT viewport;
                viewport.TopLeftX = 0;
                viewport.TopLeftY = 0;
                viewport.Width = (float)m_renderTarget->getInfo().width;
                viewport.Height = (float)m_renderTarget->getInfo().height;
                viewport.MinDepth = viewport.MaxDepth = 0.0f;
                m_deferredContext->RSSetViewports(1, &viewport);
            }

            auto& font = style == TextStyle::Selected ? m_fontSelected : m_fontNormal;

            if (!m_renderTarget->isArray()) {
                font->DrawString(m_deferredContext.Get(),
                                 string.c_str(),
                                 size,
                                 x,
                                 y,
                                 color,
                                 (alignRight ? FW1_RIGHT : FW1_LEFT) | FW1_NOFLUSH);
            } else {
                // When VPRT is used, draw each eye.
                for (uint32_t eye = 0; eye < 2; eye++) {
                    ID3D11RenderTargetView* rtvs[] = {m_renderTarget->getRenderTargetView(eye)->getNative<D3D11>()};
                    m_deferredContext->OMSetRenderTargets(1, rtvs, nullptr);

                    const float eyeOffset = eye ? m_rightEyeOffset : m_leftEyeOffset;

                    font->DrawString(m_deferredContext.Get(),
                                     string.c_str(),
                                     size,
                                     x + eyeOffset,
                                     y,
                                     color,
                                     (alignRight ? FW1_RIGHT : FW1_LEFT) | FW1_NOFLUSH);
                }
            }

            return measure ? measureString(string, style, size) : 0.0f;
        }

        float drawString(std::string string,
                         TextStyle style,
                         float size,
                         float x,
                         float y,
                         uint32_t color,
                         bool measure,
                         bool alignRight) const override {
            return drawString(
                std::wstring(string.begin(), string.end()), style, size, x, y, color, measure, alignRight);
        }

        float measureString(std::wstring string, TextStyle style, float size) const override {
            auto& font = style == TextStyle::Selected ? m_fontSelected : m_fontNormal;

            // XXX: This API is not very well documented - here is my guess on how to use the rect values...
            FW1_RECTF inRect;
            ZeroMemory(&inRect, sizeof(inRect));
            inRect.Right = inRect.Bottom = 1000.0f;
            const auto rect =
                font->MeasureString(string.c_str(), FontFamily.c_str(), size, &inRect, FW1_LEFT | FW1_TOP);
            return 1000.0f + rect.Right;
        }

        float measureString(std::string string, TextStyle style, float size) const override {
            return measureString(std::wstring(string.begin(), string.end()), style, size);
        }

      private:
        const std::shared_ptr<IDevice> m_device;

        ComPtr<IFW1Factory> m_fontWrapperFactory;
        ComPtr<IFW1FontWrapper> m_fontNormal;
        ComPtr<IFW1FontWrapper> m_fontSelected;

        std::shared_ptr<ITexture> m_renderTarget;
        float m_leftEyeOffset{0.0f};
        float m_rightEyeOffset{0.0f};
        mutable ComPtr<ID3D11DeviceContext> m_deferredContext;
    };

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
                    uint32_t displayHeight)
            : m_configManager(configManager), m_device(device), m_displayWidth(displayWidth),
              m_displayHeight(displayHeight) {
            if (m_device->getApi() == Api::D3D11) {
                m_textRenderer = std::make_shared<D3D11TextRenderer>(device);
            } else {
                Log("Unsupported graphics runtime.\n");
            }
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
                                     (int)OverlayType::MaxValue - m_configManager->isExperimentalMode() ? 1 : 2,
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
            m_menuEntries.push_back({"Factor", MenuEntryType::Slider, SettingScaling, 100, 200, [&](int value) {
                                         // We don't even use value, the utility function below will query it.
                                         const auto& resolution =
                                             GetScaledResolution(m_configManager, m_displayWidth, m_displayHeight);
                                         return fmt::format("{}% ({}x{})", value, resolution.first, resolution.second);
                                     }});
            m_originalScalingValue = getCurrentScaling();
            m_menuEntries.push_back({"Sharpness", MenuEntryType::Slider, SettingSharpness, 0, 100, [](int value) {
                                         return fmt::format("{}%", value);
                                     }});
            m_upscalingGroup.end = m_menuEntries.size();
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});

            // The unit for ICD is tenth of millimeters.
            m_menuEntries.push_back({"ICD", MenuEntryType::Slider, SettingICD, 1, 10000, [](int value) {
                                         return fmt::format("{}mm", value / 10.0f);
                                     }});
            m_menuEntries.push_back({"FOV", MenuEntryType::Slider, SettingFOV, 50, 150, [](int value) {
                                         return fmt::format("{}%", value);
                                     }});
            m_menuEntries.push_back({"", MenuEntryType::Separator, BUTTON_OR_SEPARATOR});

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
            m_configManager->setDefault(SettingOverlayEyeOffset, 0);
            m_menuEntries.push_back({"Restore defaults", MenuEntryType::RestoreDefaultsButton, BUTTON_OR_SEPARATOR});
            m_menuEntries.push_back({"Exit menu", MenuEntryType::ExitButton, BUTTON_OR_SEPARATOR});
        }

        uint32_t getCurrentScaling() const {
            return m_configManager->getEnumValue<ScalingType>(SettingScalingType) != ScalingType::None
                       ? m_configManager->getValue(SettingScaling)
                       : 0;
        }

        void handleInput() override {
            const auto now = std::chrono::steady_clock::now();

            // Check whether this is a long press and the event needs to be repeated.
            const double keyRepeat = GetAsyncKeyState(VK_SHIFT) ? KeyRepeat / 10 : KeyRepeat;
            const bool repeat = std::chrono::duration<double>(now - m_lastInput).count() > keyRepeat;

            m_wasF1Pressed = m_wasF1Pressed && !repeat;
            const bool isF1Pressed = GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_F1);
            const bool moveLeft = !m_wasF1Pressed && isF1Pressed;
            m_wasF1Pressed = isF1Pressed;

            m_wasF2Pressed = m_wasF2Pressed && !repeat;
            const bool isF2Pressed = GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_F2);
            const bool menuControl = !m_wasF2Pressed && isF2Pressed;
            m_wasF2Pressed = isF2Pressed;

            m_wasF3Pressed = m_wasF3Pressed && !repeat;
            const bool isF3Pressed = GetAsyncKeyState(VK_CONTROL) && GetAsyncKeyState(VK_F3);
            const bool moveRight = !m_wasF3Pressed && isF3Pressed;
            m_wasF3Pressed = isF3Pressed;

            if (menuControl) {
                if (m_state != MenuState::Visible) {
                    m_state = MenuState::Visible;
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

                    // When changing the upscaling, display the warning.
                    const bool wasRestartNeeded = m_needRestart;
                    m_needRestart = m_originalScalingValue != getCurrentScaling();

                    // When changing the font size, force re-alignment.
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

            updateGroupVisibility(m_upscalingGroup,
                                  m_configManager->getEnumValue<ScalingType>(SettingScalingType) != ScalingType::None);
        }

        void render(std::shared_ptr<ITexture> renderTarget, uint32_t eye) const override {
            assert(eye == 0 || eye == 1);

            const float leftEyeOffset = 0.0f;
            const float rightEyeOffset = (float)m_configManager->getValue(SettingOverlayEyeOffset);
            const float eyeOffset = eye ? rightEyeOffset : leftEyeOffset;

            m_textRenderer->begin(renderTarget, leftEyeOffset, rightEyeOffset);

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
                m_textRenderer->drawString(
                    fmt::format("Press CTRL+F2 to bring up the menu ({}s)", (int)(std::ceil(timeout - duration))),
                    TextStyle::Normal,
                    fontSize,
                    leftAlign,
                    topAlign,
                    colorSelected);

                m_textRenderer->drawString(fmt::format("(this message will be displayed {} more time{})",
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
                    m_textRenderer->clear(topAlign, leftAlign, m_menuEntriesBottom, m_menuEntriesRight + eyeOffset);
                }

                float top = topAlign;
                float left = leftAlign;

                left += m_textRenderer->drawString(L"\x25C4 : CTRL+F1   \x25BC : CTRL+F2   \x25BA : CTRL+F3",
                                                   TextStyle::Normal,
                                                   fontSize,
                                                   leftAlign,
                                                   top,
                                                   colorNormal,
                                                   measure);
                top += 1.05f * fontSize;
                m_textRenderer->drawString(
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
                        entryWidth = m_textRenderer->measureString(menuEntry.title, TextStyle::Selected, fontSize) + 50;
                        m_menuEntriesTitleWidth = max(m_menuEntriesTitleWidth, entryWidth);
                    }

                    if (!menuEntry.visible) {
                        continue;
                    }

                    const auto entryStyle = i == m_selectedItem ? TextStyle::Selected : TextStyle::Normal;
                    const auto entryColor = i == m_selectedItem ? colorSelected : colorNormal;

                    m_textRenderer->drawString(menuEntry.title, entryStyle, fontSize, left, top, entryColor);
                    if (menuEntriesTitleWidth == 0.0f) {
                        left += entryWidth;
                    } else {
                        left += menuEntriesTitleWidth;
                    }

                    const int value = m_configManager->peekValue(menuEntry.configName);

                    // Display the current value.
                    switch (menuEntry.type) {
                    case MenuEntryType::Slider:
                        left += m_textRenderer->drawString(
                            menuEntry.valueToString(value), entryStyle, fontSize, left, top, entryColor, measure);
                        break;

                    case MenuEntryType::Choice:
                        for (int j = menuEntry.minValue; j <= menuEntry.maxValue; j++) {
                            const std::string label = menuEntry.valueToString(j);

                            const auto valueStyle = j == value ? TextStyle::Selected : TextStyle::Normal;
                            const auto valueColor = j == value ? colorSelected : colorNormal;

                            left +=
                                m_textRenderer->drawString(label, valueStyle, fontSize, left, top, valueColor, true) +
                                20.0f;
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
                            left +=
                                m_textRenderer->drawString(fmt::format("({}s)", (int)(std::ceil(timeout - duration))),
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

                        left += m_textRenderer->drawString(
                            L"\x25CA  Running in safe mode, settings are cleared when restarting the VR session \x25CA",
                            TextStyle::Selected,
                            fontSize,
                            left,
                            top,
                            colorWarning,
                            measure);

                        top += 1.05f * fontSize;
                    } else if (m_needRestart) {
                        top += fontSize;

                        left += m_textRenderer->drawString(
                            L"\x25CA  Some settings require to restart the VR session \x25CA",
                            TextStyle::Selected,
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
                m_menuEntriesBottom = top;
            }

            auto overlayType = m_configManager->getEnumValue<OverlayType>(SettingOverlayType);
            if (overlayType != OverlayType::None) {
                float top = topAlign;

#define OVERLAY_COMMON TextStyle::Normal, fontSize, rightAlign, top, ColorSelected, true

                m_textRenderer->drawString(fmt::format("FPS: {}", m_stats.fps), OVERLAY_COMMON);
                top += 1.05f * fontSize;

                // Advanced displasy.
                if (overlayType == OverlayType::Advanced) {
                    m_textRenderer->drawString(fmt::format("app CPU: {}", m_stats.appCpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_textRenderer->drawString(fmt::format("app GPU: {}", m_stats.appGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_textRenderer->drawString(fmt::format("lay CPU: {}", m_stats.endFrameCpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_textRenderer->drawString(fmt::format("pre GPU: {}", m_stats.preProcessorGpuTimeUs),
                                               OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_textRenderer->drawString(fmt::format("scl GPU: {}", m_stats.upscalerGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_textRenderer->drawString(fmt::format("pst GPU: {}", m_stats.postProcessorGpuTimeUs),
                                               OVERLAY_COMMON);
                    top += 1.05f * fontSize;

                    m_textRenderer->drawString(fmt::format("ovl CPU: {}", m_stats.overlayCpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                    m_textRenderer->drawString(fmt::format("ovl GPU: {}", m_stats.overlayGpuTimeUs), OVERLAY_COMMON);
                    top += 1.05f * fontSize;
                }
#undef OVERLAY_COMMON
            }

            m_textRenderer->end();
        }

        void updateStatistics(const LayerStatistics& stats) override {
            m_stats = stats;
        }

      private:
        const std::shared_ptr<IConfigManager> m_configManager;
        const std::shared_ptr<IDevice> m_device;
        const uint32_t m_displayWidth;
        const uint32_t m_displayHeight;
        std::shared_ptr<ITextRenderer> m_textRenderer;
        LayerStatistics m_stats{};

        int m_numSplashLeft;
        std::vector<MenuEntry> m_menuEntries;
        unsigned int m_selectedItem{0};
        std::chrono::steady_clock::time_point m_lastInput;
        bool m_wasF1Pressed{false};
        bool m_wasF2Pressed{false};
        bool m_wasF3Pressed{false};

        MenuGroup m_upscalingGroup;

        uint32_t m_originalScalingValue{0};
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
                                                    uint32_t displayHeight) {
        return std::make_shared<MenuHandler>(configManager, device, displayWidth, displayHeight);
    }

} // namespace toolkit::menu
