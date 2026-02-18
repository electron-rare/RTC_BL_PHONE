#include <unity.h>
#include "telephony/DtmfDecoder.h"

void test_dtmf_no_digit_on_silence() {
    DtmfDecoder decoder;
    bool called = false;
    decoder.setDigitCallback([&](char d){ called = true; });
    int16_t silence[160] = {0};
    decoder.feedAudioSamples(silence, 160);
    TEST_ASSERT_FALSE(called);
}

// TODO: Ajouter un test avec un signal synthétique DTMF (Goertzel)

void setup() {
    UNITY_BEGIN();
    RUN_TEST(test_dtmf_no_digit_on_silence);
    UNITY_END();
}

void loop() {}
