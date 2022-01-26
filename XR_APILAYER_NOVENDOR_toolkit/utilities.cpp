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

namespace {

    using namespace toolkit::utilities;

    class CpuTimer : public ICpuTimer {
        using clock = std::chrono::high_resolution_clock;

      public:
        void start() override {
            m_timeStart = clock::now();
        }

        void stop() override {
            m_duration = clock::now() - m_timeStart;
        }

        uint64_t query(bool reset) const override {
            const auto duration = std::chrono::duration_cast<std::chrono::microseconds>(m_duration);

            if (reset)
                m_duration = clock::duration::zero();

            return duration.count();
        }

      private:
        clock::time_point m_timeStart;
        mutable clock::duration m_duration{0};
    };

} // namespace

namespace toolkit::config {

    std::pair<uint32_t, uint32_t> GetScaledDimensions(const IConfigManager* configManager,
                                                      uint32_t outputWidth,
                                                      uint32_t outputHeight,
                                                      uint32_t blockSize) {
        const auto settingScaling = configManager->peekValue(SettingScaling);
        const auto settingAnamophic = configManager->peekValue(SettingAnamorphic);

        return std::make_pair(utilities::GetScaledInputSize(outputWidth, settingScaling, blockSize),
                              utilities::GetScaledInputSize(
                                  outputHeight, settingAnamophic > 0 ? settingAnamophic : settingScaling, blockSize));
    }

} // namespace toolkit::config

namespace toolkit::utilities {

    using namespace toolkit::config;

    std::shared_ptr<ICpuTimer> CreateCpuTimer() {
        return std::make_shared<CpuTimer>();
    }

    uint32_t GetScaledInputSize(uint32_t outputSize, int scalePercent, uint32_t blockSize) {
        scalePercent = abs(scalePercent);
        auto size = scalePercent >= 100 ? (outputSize * 100u) / scalePercent : (outputSize * scalePercent) / 100u;

        // align size to blockSize
        if (blockSize >= 2u)
            size = ((size + blockSize - 1u) / blockSize) * blockSize;

        return size;
    }

    bool UpdateKeyState(bool& keyState, int vkModifier, int vkKey, bool isRepeat) {
        const bool isPressed = GetAsyncKeyState(vkModifier) && GetAsyncKeyState(vkKey);
        const bool wasPressed = std::exchange(keyState, isPressed);
        return isPressed && (!wasPressed || isRepeat);
    }

} // namespace toolkit::utilities