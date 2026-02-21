#ifndef AUDIO_ES8388_DRIVER_H
#define AUDIO_ES8388_DRIVER_H

#include <Arduino.h>

class Es8388Driver {
public:
    bool begin(int sda_pin, int scl_pin, uint8_t address = 0x10);
    bool setVolume(uint8_t percent);
    bool setMute(bool enabled);
    bool setRoute(const String& route);

    bool isReady() const;
    uint8_t volume() const;
    bool muted() const;
    String route() const;

private:
    bool writeReg(uint8_t reg, uint8_t value);

    bool ready_ = false;
    uint8_t address_ = 0x10;
    uint8_t volume_ = 80;
    bool muted_ = false;
    String route_ = "rtc";
};

#endif  // AUDIO_ES8388_DRIVER_H
