// MIT License
//
// Copyright(c) 2021 Matthieu Bucchianeri
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
      public:
        void start() override {
            m_timeStart = std::chrono::high_resolution_clock::now();
        }

        void stop() override {
            m_duration = std::chrono::duration(std::chrono::high_resolution_clock::now() - m_timeStart).count() / 1000;
        }

        uint64_t query(bool reset) const override {
            const uint64_t duration = m_duration;

            if (reset) {
                m_duration = 0;
            }

            return duration;
        }

      private:
        std::chrono::time_point<std::chrono::high_resolution_clock> m_timeStart;

        mutable uint64_t m_duration{0};
    };

} // namespace

namespace toolkit::utilities {

    using namespace toolkit::config;

    std::shared_ptr<ICpuTimer> CreateCpuTimer() {
        return std::make_shared<CpuTimer>();
    }

    std::pair<uint32_t, uint32_t> GetScaledResolution(std::shared_ptr<IConfigManager> configManager,
                                                      uint32_t outputWidth,
                                                      uint32_t outputHeight) {
        uint32_t inputWidth = outputWidth;
        uint32_t inputHeight = outputHeight;

        const int upscalingPercent = configManager->getValue(SettingScaling);
        if (upscalingPercent > 100) {
            inputWidth = (uint32_t)((100.0f / upscalingPercent) * outputWidth);
            if (inputWidth % 2) {
                inputWidth++;
            }
            inputHeight = (uint32_t)((100.0f / upscalingPercent) * outputHeight);
            if (inputHeight % 2) {
                inputHeight++;
            }
        }

        return std::make_pair(inputWidth, inputHeight);
    }

} // namespace toolkit::utilities