#include "audio/Es8388Driver.h"

#include <Wire.h>

#include <algorithm>

namespace {
uint8_t clampVolumeToReg(uint8_t percent) {
    const uint8_t clamped = std::min<uint8_t>(100, percent);
    return static_cast<uint8_t>((clamped * 0x21U) / 100U);
}

}  // namespace

bool Es8388Driver::begin(int sda_pin, int scl_pin, uint8_t address) {
    address_ = address;
    Wire.begin(sda_pin, scl_pin);

    // Minimal ES8388 init sequence for playback + capture paths.
    const bool ok = writeReg(0x00, 0x80) &&  // reset
                    writeReg(0x01, 0x58) &&
                    writeReg(0x02, 0x50) &&
                    writeReg(0x04, 0xC0) &&
                    writeReg(0x08, 0x00) &&
                    writeReg(0x0A, 0x00) &&
                    writeReg(0x17, 0x18) &&
                    writeReg(0x19, 0x02);

    ready_ = ok;
    if (!ready_) {
        return false;
    }

    setVolume(volume_);
    setMute(muted_);
    setRoute(route_);
    return true;
}

bool Es8388Driver::setVolume(uint8_t percent) {
    volume_ = std::min<uint8_t>(100, percent);
    if (!ready_) {
        return false;
    }
    const uint8_t reg = clampVolumeToReg(volume_);
    return writeReg(0x2B, reg) && writeReg(0x2C, reg);
}

bool Es8388Driver::setMute(bool enabled) {
    muted_ = enabled;
    if (!ready_) {
        return false;
    }
    return writeReg(0x2F, enabled ? 0x01 : 0x00);
}

bool Es8388Driver::setRoute(const String& route) {
    route_ = route;
    route_.toLowerCase();
    if (!ready_) {
        return false;
    }

    if (route_ == "bluetooth") {
        return writeReg(0x30, 0x01);
    }
    if (route_ == "none") {
        return writeReg(0x30, 0x02);
    }
    route_ = "rtc";
    return writeReg(0x30, 0x00);
}

bool Es8388Driver::isReady() const {
    return ready_;
}

uint8_t Es8388Driver::volume() const {
    return volume_;
}

bool Es8388Driver::muted() const {
    return muted_;
}

String Es8388Driver::route() const {
    return route_;
}

bool Es8388Driver::writeReg(uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address_);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}
