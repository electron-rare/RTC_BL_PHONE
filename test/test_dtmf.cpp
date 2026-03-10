#include <array>
#include <unity.h>

#include "telephony/DtmfDecoder.h"

void test_dtmf_decoder_does_not_emit_on_silence() {
    DtmfDecoder decoder(8000, 160);
    bool emitted = false;
    decoder.setDigitCallback([&](char) { emitted = true; });

    std::array<int16_t, 160> samples{};
    decoder.feedAudioSamples(samples.data(), samples.size());

    TEST_ASSERT_FALSE(emitted);
}
