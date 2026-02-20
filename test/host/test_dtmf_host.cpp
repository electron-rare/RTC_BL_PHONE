#include "telephony/DtmfDecoder.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr double kPi = 3.14159265358979323846;

std::vector<int16_t> generateDtmfTone(double lowFreqHz,
                                      double highFreqHz,
                                      int sampleRateHz,
                                      double durationSeconds,
                                      int16_t amplitude = 12000) {
    const size_t sampleCount = static_cast<size_t>(durationSeconds * static_cast<double>(sampleRateHz));
    std::vector<int16_t> samples(sampleCount, 0);
    for (size_t i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sampleRateHz);
        const double value = 0.5 * (std::sin(2.0 * kPi * lowFreqHz * t) + std::sin(2.0 * kPi * highFreqHz * t));
        samples[i] = static_cast<int16_t>(std::round(static_cast<double>(amplitude) * value));
    }
    return samples;
}

std::vector<int16_t> generateSingleTone(double freqHz,
                                        int sampleRateHz,
                                        double durationSeconds,
                                        int16_t amplitude = 12000) {
    const size_t sampleCount = static_cast<size_t>(durationSeconds * static_cast<double>(sampleRateHz));
    std::vector<int16_t> samples(sampleCount, 0);
    for (size_t i = 0; i < sampleCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(sampleRateHz);
        const double value = std::sin(2.0 * kPi * freqHz * t);
        samples[i] = static_cast<int16_t>(std::round(static_cast<double>(amplitude) * value));
    }
    return samples;
}

bool runScenario(const std::string& name, const std::function<bool()>& scenario) {
    const bool ok = scenario();
    std::cout << "[host-dtmf] " << name << ": " << (ok ? "PASS" : "FAIL") << std::endl;
    return ok;
}
}  // namespace

int main() {
    bool allOk = true;

    allOk &= runScenario("silence_does_not_emit", []() {
        DtmfDecoder decoder(8000U, 160U);
        bool emitted = false;
        decoder.setDigitCallback([&](char) { emitted = true; });
        std::vector<int16_t> silence(800, 0);
        decoder.feedAudioSamples(silence.data(), silence.size());
        return !emitted;
    });

    allOk &= runScenario("detects_digit_5", []() {
        DtmfDecoder decoder(8000U, 160U);
        int callbackCount = 0;
        char lastDigit = '\0';
        decoder.setDigitCallback([&](char digit) {
            ++callbackCount;
            lastDigit = digit;
        });
        const std::vector<int16_t> tone = generateDtmfTone(770.0, 1336.0, 8000, 0.12);
        decoder.feedAudioSamples(tone.data(), tone.size());
        return callbackCount == 1 && lastDigit == '5';
    });

    allOk &= runScenario("single_tone_is_rejected", []() {
        DtmfDecoder decoder(8000U, 160U);
        bool emitted = false;
        decoder.setDigitCallback([&](char) { emitted = true; });
        const std::vector<int16_t> tone = generateSingleTone(770.0, 8000, 0.12);
        decoder.feedAudioSamples(tone.data(), tone.size());
        return !emitted;
    });

    allOk &= runScenario("digit_can_retrigger_after_pause", []() {
        DtmfDecoder decoder(8000U, 160U);
        int callbackCount = 0;
        decoder.setDigitCallback([&](char digit) {
            if (digit == '5') {
                ++callbackCount;
            }
        });

        const std::vector<int16_t> tone = generateDtmfTone(770.0, 1336.0, 8000, 0.12);
        std::vector<int16_t> payload;
        payload.reserve(tone.size() * 2 + 320);
        payload.insert(payload.end(), tone.begin(), tone.end());
        payload.insert(payload.end(), 320, 0);
        payload.insert(payload.end(), tone.begin(), tone.end());
        decoder.feedAudioSamples(payload.data(), payload.size());
        return callbackCount == 2;
    });

    return allOk ? 0 : 1;
}
