#include "core/PlatformProfile.h"

BoardProfile detectBoardProfile() {
#if defined(BOARD_PROFILE_ESP32_S3) || defined(CONFIG_IDF_TARGET_ESP32S3)
    return BoardProfile::ESP32_S3;
#else
    return BoardProfile::ESP32_A252;
#endif
}

FeatureMatrix getFeatureMatrix(BoardProfile profile) {
    switch (profile) {
        case BoardProfile::ESP32_A252:
            return FeatureMatrix{
                .has_bt_classic = true,
                .has_hfp = true,
                .has_full_duplex_i2s = true,
                .has_ble_control = true,
            };
        case BoardProfile::ESP32_S3:
            return FeatureMatrix{
                .has_bt_classic = false,
                .has_hfp = false,
                .has_full_duplex_i2s = false,
                .has_ble_control = true,
            };
        default:
            return FeatureMatrix{
                .has_bt_classic = false,
                .has_hfp = false,
                .has_full_duplex_i2s = false,
                .has_ble_control = false,
            };
    }
}

const char* boardProfileToString(BoardProfile profile) {
    switch (profile) {
        case BoardProfile::ESP32_A252:
            return "ESP32_A252";
        case BoardProfile::ESP32_S3:
            return "ESP32_S3";
        default:
            return "UNKNOWN";
    }
}
