#include "audio/Es8388Driver.h"

#include <Wire.h>

#include <algorithm>
#include <initializer_list>

namespace {
uint8_t clampVolumeToReg(uint8_t percent) {
    const uint8_t clamped = std::min<uint8_t>(100, percent);
    // ES8388 DAC digital volume registers (0x1A/0x1B):
    //   0x00 = 0 dB, 0xC0 = -96 dB.
    return static_cast<uint8_t>(((100U - clamped) * 0xC0U) / 100U);
}
}  // namespace

bool Es8388Driver::begin(int sda_pin, int scl_pin, uint8_t address) {
    address_ = address;
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(100000);

    const auto write_sequence = [&](std::initializer_list<std::pair<uint8_t, uint8_t>> seq) {
        bool ok = true;
        for (const auto& it : seq) {
            ok &= writeReg(it.first, it.second);
        }
        return ok;
    };

    // ES8388 setup aligned with Espressif ADF defaults (codec in I2S slave mode).
    // Register map: https://github.com/espressif/esp-adf (es8388 driver).
    const bool ok = write_sequence(
        {
            {0x19, 0x04},  // DACCONTROL3: mute during init.
            {0x01, 0x50},  // CONTROL2
            {0x02, 0x00},  // CHIPPOWER normal mode
            {0x35, 0xA0},  // Disable internal DLL for low sample rates stability.
            {0x37, 0xD0},
            {0x39, 0xD0},
            {0x08, 0x00},  // MASTERMODE: codec slave
            {0x04, 0xC0},  // DACPOWER: DAC outputs disabled while configuring
            {0x00, 0x12},  // CONTROL1: play+record mode
            {0x17, 0x18},  // DACCONTROL1: 16-bit I2S
            {0x18, 0x02},  // DACCONTROL2: single speed, ratio 256
            {0x26, 0x00},  // DACCONTROL16: DAC to mixer
            {0x27, 0x90},  // DACCONTROL17: left DAC to left mixer
            {0x2A, 0x90},  // DACCONTROL20: right DAC to right mixer
            {0x2B, 0x80},  // DACCONTROL21
            {0x2D, 0x00},  // DACCONTROL23
            {0x2E, 0x24},  // DACCONTROL24: analog output boosted (bench tuning)
            {0x2F, 0x24},  // DACCONTROL25: analog output boosted (bench tuning)
            {0x30, 0x00},  // DACCONTROL26
            {0x31, 0x00},  // DACCONTROL27
            {0x04, 0x3C},  // DACPOWER: enable LOUT/ROUT
            {0x03, 0xFF},  // ADCPOWER: power down before ADC config
            {0x09, 0xBB},  // ADCCONTROL1: PGA gain defaults
            {0x0A, 0x00},  // ADCCONTROL2: LIN1/RIN1
            {0x0B, 0x02},  // ADCCONTROL3
            {0x0C, 0x0C},  // ADCCONTROL4: 16-bit I2S
            {0x0D, 0x02},  // ADCCONTROL5: single speed, ratio 256
            {0x10, 0x00},  // ADCCONTROL8: 0 dB
            {0x11, 0x00},  // ADCCONTROL9: 0 dB
            {0x03, 0x09},  // ADCPOWER: enable ADC path
        });

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
    // DAC digital volume controls.
    return writeReg(0x1A, reg) && writeReg(0x1B, reg);
}

bool Es8388Driver::setMute(bool enabled) {
    muted_ = enabled;
    if (!ready_) {
        return false;
    }
    // DACCONTROL3 bit2 is mute.
    return writeReg(0x19, enabled ? 0x04 : 0x00);
}

bool Es8388Driver::setRoute(const String& route) {
    route_ = route;
    route_.toLowerCase();
    if (!ready_) {
        return false;
    }

    // Current hardware path uses DAC->mixer->line output for both RTC and BT.
    // Keep route as metadata and ensure output path is enabled.
    if (route_ == "bluetooth" || route_ == "rtc") {
        return writeReg(0x26, 0x00) && writeReg(0x27, 0x90) && writeReg(0x2A, 0x90) &&
               writeReg(0x04, 0x3C);
    }
    if (route_ == "none") {
        return writeReg(0x04, 0xC0);
    }
    route_ = "rtc";
    return writeReg(0x26, 0x00) && writeReg(0x27, 0x90) && writeReg(0x2A, 0x90) &&
           writeReg(0x04, 0x3C);
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
