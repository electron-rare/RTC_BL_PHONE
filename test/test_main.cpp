

#include <unity.h>
#include "../src/AudioCodec.h"
#include "telephony/DtmfDecoder.h"

// --- AudioCodec tests ---
void test_init() {
	GenericCodec codec;
	TEST_ASSERT_TRUE(codec.init());
}
void test_volume() {
	GenericCodec codec;
	TEST_ASSERT_TRUE(codec.setVolume(42));
}
void test_mute() {
	GenericCodec codec;
	TEST_ASSERT_TRUE(codec.mute(true));
	TEST_ASSERT_TRUE(codec.mute(false));
}
void test_route() {
	GenericCodec codec;
	TEST_ASSERT_TRUE(codec.setRoute(ROUTE_RTC));
	TEST_ASSERT_TRUE(codec.setRoute(ROUTE_BLUETOOTH));
	TEST_ASSERT_TRUE(codec.setRoute(ROUTE_NONE));
}

// --- DTMF tests ---
void test_dtmf_no_digit_on_silence() {
	DtmfDecoder decoder;
	bool called = false;
	decoder.setDigitCallback([&](char d){ called = true; });
	int16_t silence[160] = {0};
	decoder.feedAudioSamples(silence, 160);
	TEST_ASSERT_FALSE(called);
}

void setup() {
    UNITY_BEGIN();
	RUN_TEST(test_init);
    RUN_TEST(test_volume);
    RUN_TEST(test_mute);
    RUN_TEST(test_route);
    RUN_TEST(test_dtmf_no_digit_on_silence);
    UNITY_END();
}

void loop() {}
