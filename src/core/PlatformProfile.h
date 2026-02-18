#ifndef CORE_PLATFORM_PROFILE_H
#define CORE_PLATFORM_PROFILE_H

#include <Arduino.h>

enum class BoardProfile : uint8_t {
    ESP32_A252 = 0,
    ESP32_S3 = 1
};

struct FeatureMatrix {
    bool has_bt_classic;
    bool has_hfp;
    bool has_full_duplex_i2s;
    bool has_ble_control;
};

BoardProfile detectBoardProfile();
FeatureMatrix getFeatureMatrix(BoardProfile profile);
const char* boardProfileToString(BoardProfile profile);

#endif  // CORE_PLATFORM_PROFILE_H
