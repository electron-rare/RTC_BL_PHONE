#include "telephony/TelephonyService.h"

const char* telephonyStateToString(TelephonyState state) {
    switch (state) {
        case TelephonyState::IDLE:
            return "IDLE";
        case TelephonyState::RINGING:
            return "RINGING";
        case TelephonyState::PLAYING_MESSAGE:
            return "PLAYING_MESSAGE";
        case TelephonyState::OFF_HOOK:
            return "OFF_HOOK";
        default:
            return "UNKNOWN";
    }
}

TelephonyService::TelephonyService()
    : profile_(BoardProfile::ESP32_A252),
      features_(getFeatureMatrix(BoardProfile::ESP32_A252)),
      slic_(nullptr),
      audio_(nullptr),
      state_(TelephonyState::IDLE),
      incoming_ring_(false),
      ring_phase_on_(false),
      ring_cycle_start_ms_(0),
      message_path_("/welcome.wav") {}

bool TelephonyService::begin(BoardProfile profile, SlicController& slic, AudioEngine& audio) {
    profile_ = profile;
    features_ = getFeatureMatrix(profile);
    slic_ = &slic;
    audio_ = &audio;
    state_ = TelephonyState::IDLE;
    incoming_ring_ = false;
    ring_phase_on_ = false;
    ring_cycle_start_ms_ = millis();

    slic_->setRing(false);
    slic_->setLineEnabled(true);
    return true;
}

void TelephonyService::triggerIncomingRing() {
    incoming_ring_ = true;
}

void TelephonyService::tick() {
    if (slic_ == nullptr || audio_ == nullptr) {
        return;
    }

    slic_->tick();
    audio_->tick();

    const bool hook_off = slic_->isHookOff();
    switch (state_) {
        case TelephonyState::IDLE:
            if (incoming_ring_ && !hook_off) {
                ring_cycle_start_ms_ = millis();
                ring_phase_on_ = true;
                slic_->setRing(true);
                state_ = TelephonyState::RINGING;
            } else if (hook_off) {
                state_ = TelephonyState::OFF_HOOK;
            }
            break;

        case TelephonyState::RINGING: {
            if (hook_off) {
                incoming_ring_ = false;
                ring_phase_on_ = false;
                slic_->setRing(false);
                audio_->playFile(message_path_);
                state_ = TelephonyState::PLAYING_MESSAGE;
                break;
            }

            if (!incoming_ring_) {
                ring_phase_on_ = false;
                slic_->setRing(false);
                state_ = TelephonyState::IDLE;
                break;
            }

            const uint32_t elapsed = (millis() - ring_cycle_start_ms_) % 5000U;
            const bool should_ring = elapsed < 1000U;
            if (should_ring != ring_phase_on_) {
                ring_phase_on_ = should_ring;
                slic_->setRing(ring_phase_on_);
            }
            break;
        }

        case TelephonyState::PLAYING_MESSAGE:
            if (!audio_->isPlaying()) {
                state_ = hook_off ? TelephonyState::OFF_HOOK : TelephonyState::IDLE;
            }
            break;

        case TelephonyState::OFF_HOOK:
            if (!hook_off) {
                incoming_ring_ = false;
                state_ = TelephonyState::IDLE;
            }
            break;
    }
}

TelephonyState TelephonyService::state() const {
    return state_;
}
