#include "slic/Ks0835SlicController.h"

Ks0835SlicController::Ks0835SlicController()
    : initialized_(false), ring_enabled_(false), fr_state_(false), last_fr_toggle_ms_(0) {
    pins_ = {0, 0, 0, -1, -1, true};
}

bool Ks0835SlicController::begin(const SlicPins& pins) {
    pins_ = pins;

    pinMode(pins_.pin_rm, OUTPUT);
    pinMode(pins_.pin_fr, OUTPUT);
    pinMode(pins_.pin_shk, INPUT_PULLUP);

    digitalWrite(pins_.pin_rm, LOW);
    digitalWrite(pins_.pin_fr, LOW);

    if (pins_.pin_line_enable >= 0) {
        pinMode(pins_.pin_line_enable, OUTPUT);
        digitalWrite(pins_.pin_line_enable, LOW);
    }

    // PD must never be actively driven HIGH for KS0835-like modules.
    if (pins_.pin_pd >= 0) {
        pinMode(pins_.pin_pd, INPUT);
    }

    initialized_ = true;
    ring_enabled_ = false;
    fr_state_ = false;
    last_fr_toggle_ms_ = millis();
    return true;
}

void Ks0835SlicController::setRing(bool enabled) {
    if (!initialized_) {
        return;
    }
    ring_enabled_ = enabled;
    digitalWrite(pins_.pin_rm, enabled ? HIGH : LOW);
    if (!enabled) {
        fr_state_ = false;
        digitalWrite(pins_.pin_fr, LOW);
    }
}

void Ks0835SlicController::setLineEnabled(bool enabled) {
    if (!initialized_ || pins_.pin_line_enable < 0) {
        return;
    }
    digitalWrite(pins_.pin_line_enable, enabled ? HIGH : LOW);
}

bool Ks0835SlicController::isHookOff() const {
    if (!initialized_) {
        return false;
    }
    const int level = digitalRead(pins_.pin_shk);
    return pins_.hook_active_high ? (level == HIGH) : (level == LOW);
}

void Ks0835SlicController::setPowerDown(bool enabled) {
    if (!initialized_ || pins_.pin_pd < 0) {
        return;
    }

    if (enabled) {
        pinMode(pins_.pin_pd, OUTPUT_OPEN_DRAIN);
        digitalWrite(pins_.pin_pd, LOW);
    } else {
        pinMode(pins_.pin_pd, INPUT);
    }
}

void Ks0835SlicController::tick() {
    if (!initialized_ || !ring_enabled_) {
        return;
    }

    const uint32_t now = millis();
    if (now - last_fr_toggle_ms_ >= 20) {
        fr_state_ = !fr_state_;
        digitalWrite(pins_.pin_fr, fr_state_ ? HIGH : LOW);
        last_fr_toggle_ms_ = now;
    }
}
