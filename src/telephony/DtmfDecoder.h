#pragma once
#include <functional>
#include <vector>

class DtmfDecoder {
public:
    using DigitCallback = std::function<void(char)>;
    DtmfDecoder();
    void feedAudioSamples(const int16_t* samples, size_t count);
    void setDigitCallback(DigitCallback cb);
private:
    DigitCallback onDigit;
    // ...internals Goertzel...
};
