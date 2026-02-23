#include "telephony/TelephonyService.h"

namespace {
constexpr uint16_t kDtmfFrameSamples = 160U;
constexpr uint32_t kHookHangupMs = 800U;
constexpr uint32_t kHookStabilizeMs = 40U;
constexpr uint32_t kPulseInterDigitGapMs = 420U;
constexpr uint32_t kPulseEdgeDebounceMs = 18U;
constexpr uint32_t kPulseDtmfGuardMs = 900U;
constexpr size_t kDialDigitsTarget = 10U;
constexpr uint32_t kDtmfCaptureStartDelayMs = 0U;
constexpr uint32_t kDtmfReadPeriodMs = 12U;
constexpr uint8_t kDialSourceNone = 0U;
constexpr uint8_t kDialSourceDtmf = 1U;
constexpr uint8_t kDialSourcePulse = 2U;
}

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
      dial_callback_(nullptr),
      answer_callback_(nullptr),
      dtmf_(8000U, kDtmfFrameSamples),
      state_(TelephonyState::IDLE),
      incoming_ring_(false),
      ring_phase_on_(false),
      ring_cycle_start_ms_(0),
      capture_active_(false),
      pulse_hook_initialized_(false),
      pulse_last_hook_off_(false),
      pulse_collecting_(false),
      pulse_count_(0),
      last_hook_edge_ms_(0),
      last_pulse_ms_(0),
      dtmf_capture_start_ms_(0),
      next_dtmf_read_ms_(0),
      off_hook_enter_ms_(0),
      last_pulse_edge_ms_(0),
      suppress_dial_tone_(false),
      dialing_started_(false),
      dial_source_(kDialSourceNone),
      dial_buffer_(""),
      last_digit_ms_(0),
      last_dial_error_(""),
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
    capture_active_ = false;
    pulse_hook_initialized_ = false;
    pulse_last_hook_off_ = false;
    pulse_collecting_ = false;
    pulse_count_ = 0;
    last_hook_edge_ms_ = 0;
    last_pulse_ms_ = 0;
    dtmf_capture_start_ms_ = 0;
    next_dtmf_read_ms_ = 0;
    off_hook_enter_ms_ = 0;
    last_pulse_edge_ms_ = 0;
    suppress_dial_tone_ = false;
    dialing_started_ = false;
    dial_source_ = kDialSourceNone;
    dial_buffer_ = "";
    last_digit_ms_ = 0;
    last_dial_error_ = "";

    dtmf_.setDigitCallback([this](char digit) {
        onDialDigit(digit, false);
    });

    slic_->setRing(false);
    slic_->setLineEnabled(true);
    return true;
}

void TelephonyService::setDialCallback(DialCallback cb) {
    dial_callback_ = cb;
}

void TelephonyService::setAnswerCallback(AnswerCallback cb) {
    answer_callback_ = cb;
}

void TelephonyService::triggerIncomingRing() {
    incoming_ring_ = true;
}

void TelephonyService::setIncomingRing(bool active) {
    incoming_ring_ = active;
}

void TelephonyService::onDialDigit(char digit, bool from_pulse) {
    if (digit < '0' || digit > '9') {
        return;
    }

    const uint32_t now = millis();
    if (!from_pulse) {
        // Rotary pulse has priority: suppress DTMF captures while pulse edges are active/recent.
        const bool pulse_recent =
            pulse_collecting_ || pulse_count_ > 0U ||
            (last_pulse_edge_ms_ != 0U && (now - last_pulse_edge_ms_) < kPulseDtmfGuardMs);
        if (pulse_recent) {
            return;
        }
    }

    const uint8_t source = from_pulse ? kDialSourcePulse : kDialSourceDtmf;
    if (dial_source_ == kDialSourceNone) {
        dial_source_ = source;
    } else if (dial_source_ != source) {
        // Allow pulse to override an early DTMF false-start (typically tone bleed).
        if (from_pulse && dial_source_ == kDialSourceDtmf && dial_buffer_.length() <= 1U) {
            dial_buffer_ = "";
            last_digit_ms_ = 0;
            dial_source_ = source;
        } else {
            // Keep strict ordering by ignoring mixed-source digits in the same session.
            return;
        }
    }

    if (audio_ != nullptr && dial_buffer_.isEmpty() && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }
    dialing_started_ = true;
    if (dial_buffer_.length() >= kDialDigitsTarget) {
        dial_buffer_ = "";
    }

    dial_buffer_ += digit;
    last_digit_ms_ = now;
    Serial.printf("[Telephony] digit=%c source=%s buffer=%s\n",
                  digit,
                  from_pulse ? "pulse" : "dtmf",
                  dial_buffer_.c_str());

    if (dial_buffer_.length() == kDialDigitsTarget) {
        commitDialBuffer("len10");
    }
}

void TelephonyService::updatePulseDecode(bool hook_off, uint32_t now) {
    if (!pulse_hook_initialized_) {
        pulse_hook_initialized_ = true;
        pulse_last_hook_off_ = hook_off;
        last_hook_edge_ms_ = now;
        return;
    }

    if (hook_off == pulse_last_hook_off_) {
        return;
    }

    if ((now - last_pulse_edge_ms_) < kPulseEdgeDebounceMs) {
        return;
    }
    last_pulse_edge_ms_ = now;

    // Any valid hook edge during OFF_HOOK indicates dialing activity start.
    if (audio_ != nullptr && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }
    dialing_started_ = true;

    if (pulse_last_hook_off_ && !hook_off) {
        if (!pulse_collecting_) {
            pulse_collecting_ = true;
            pulse_count_ = 0;
            // Stop dial tone as soon as rotary dialing starts (first pulse edge),
            // not only after the first full decoded digit.
            if (audio_ != nullptr && audio_->isDialToneActive()) {
                audio_->stopDialTone();
            }
        }
    } else if (!pulse_last_hook_off_ && hook_off) {
        if (pulse_collecting_ && pulse_count_ < 20U) {
            ++pulse_count_;
            last_pulse_ms_ = now;
        }
    }

    pulse_last_hook_off_ = hook_off;
    last_hook_edge_ms_ = now;
}

void TelephonyService::commitDialBuffer(const char* reason) {
    if (dial_buffer_.length() != kDialDigitsTarget) {
        return;
    }

    if (audio_ != nullptr && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }

    const String number = dial_buffer_;
    const bool ok = dial_callback_ ? dial_callback_(number) : false;
    last_dial_error_ = ok ? "" : "bt_dial_failed";
    Serial.printf("[Telephony] dial_trigger reason=%s number=%s ok=%s\n",
                  reason != nullptr ? reason : "unknown",
                  number.c_str(),
                  ok ? "true" : "false");

    dial_buffer_ = "";
    last_digit_ms_ = 0;
}

void TelephonyService::clearDialSession() {
    if (audio_ != nullptr && audio_->isDialToneActive()) {
        audio_->stopDialTone();
    }
    if (audio_ != nullptr && capture_active_) {
        audio_->releaseCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
    }
    capture_active_ = false;
    dtmf_capture_start_ms_ = 0;
    next_dtmf_read_ms_ = 0;
    off_hook_enter_ms_ = 0;
    pulse_hook_initialized_ = false;
    pulse_collecting_ = false;
    pulse_count_ = 0;
    last_hook_edge_ms_ = 0;
    last_pulse_ms_ = 0;
    last_pulse_edge_ms_ = 0;
    dial_source_ = kDialSourceNone;
    dialing_started_ = false;
    suppress_dial_tone_ = false;
    dial_buffer_ = "";
    last_digit_ms_ = 0;
}

void TelephonyService::tick() {
    if (slic_ == nullptr || audio_ == nullptr) {
        return;
    }

    slic_->tick();

    const bool hook_off = slic_->isHookOff();
    const uint32_t now = millis();
    const TelephonyState prev_state = state_;

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
                const bool answered = answer_callback_ ? answer_callback_() : false;
                // While transitioning from incoming ring to call answer, keep dial tone muted
                // even if BT answer fails transiently.
                suppress_dial_tone_ = true;
                last_dial_error_ = answered ? "" : "bt_answer_failed";
                state_ = TelephonyState::OFF_HOOK;
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
            if ((now - off_hook_enter_ms_) >= kHookStabilizeMs) {
                updatePulseDecode(hook_off, now);
            }

            if (!hook_off) {
                // Stop audible dial tone immediately on hangup, even if we keep
                // a short debounce before transitioning back to IDLE.
                if (audio_ != nullptr && audio_->isDialToneActive()) {
                    audio_->stopDialTone();
                }
                if (audio_ != nullptr && capture_active_) {
                    audio_->releaseCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
                    capture_active_ = false;
                }
                // Reset dialing session immediately on hangup.
                if (!dial_buffer_.isEmpty() || dial_source_ != kDialSourceNone || pulse_collecting_ || pulse_count_ > 0U) {
                    dial_buffer_ = "";
                    last_digit_ms_ = 0;
                    dial_source_ = kDialSourceNone;
                    dialing_started_ = false;
                    pulse_collecting_ = false;
                    pulse_count_ = 0;
                    last_pulse_ms_ = 0;
                }
                if ((now - last_hook_edge_ms_) >= kHookHangupMs) {
                    incoming_ring_ = false;
                    state_ = TelephonyState::IDLE;
                }
                break;
            }

            if (pulse_collecting_ && pulse_count_ > 0U && (now - last_pulse_ms_) >= kPulseInterDigitGapMs) {
                const uint8_t count = pulse_count_;
                pulse_collecting_ = false;
                pulse_count_ = 0;
                const char digit = (count == 10U) ? '0' : ((count >= 1U && count <= 9U) ? static_cast<char>('0' + count)
                                                                                           : '\0');
                if (digit != '\0') {
                    onDialDigit(digit, true);
                }
            }

            if (!capture_active_ && now >= dtmf_capture_start_ms_) {
                capture_active_ = audio_->requestCapture(AudioEngine::CAPTURE_CLIENT_TELEPHONY);
            }
            if (capture_active_ && now >= next_dtmf_read_ms_) {
                int16_t frame[kDtmfFrameSamples] = {0};
                const size_t samples_read = audio_->readCaptureFrameNonBlocking(frame, kDtmfFrameSamples);
                if (samples_read > 0U) {
                    dtmf_.feedAudioSamples(frame, samples_read);
                }
                next_dtmf_read_ms_ = now + kDtmfReadPeriodMs;
            }

            if (suppress_dial_tone_ && audio_->isDialToneActive()) {
                audio_->stopDialTone();
            }

            const bool pulse_dial_in_progress =
                pulse_collecting_ || pulse_count_ > 0U ||
                (last_pulse_edge_ms_ != 0U && (now - last_pulse_edge_ms_) < kPulseInterDigitGapMs);
            if (!suppress_dial_tone_ && !dialing_started_ && dial_buffer_.isEmpty() && !audio_->isDialToneActive() &&
                !pulse_dial_in_progress) {
                audio_->startDialTone();
            }

            if (!dial_buffer_.isEmpty() && (now - last_digit_ms_) >= 10000U) {
                // Drop stale partial numbers instead of dialing an incomplete value.
                dial_buffer_ = "";
                last_digit_ms_ = 0;
            }
            break;
    }

    if (prev_state != state_) {
        if (state_ == TelephonyState::OFF_HOOK) {
            off_hook_enter_ms_ = now;
            pulse_hook_initialized_ = false;
            pulse_collecting_ = false;
            pulse_count_ = 0;
            last_hook_edge_ms_ = now;
            last_pulse_ms_ = 0;
            last_pulse_edge_ms_ = 0;
            dial_source_ = kDialSourceNone;
            dialing_started_ = false;
            dial_buffer_ = "";
            last_digit_ms_ = 0;
            dtmf_capture_start_ms_ = now + kDtmfCaptureStartDelayMs;
            next_dtmf_read_ms_ = now;
            if (audio_ != nullptr && !suppress_dial_tone_) {
                audio_->startDialTone();
            }
        }

        if (prev_state == TelephonyState::OFF_HOOK && state_ != TelephonyState::OFF_HOOK) {
            clearDialSession();
        }
    }
}

TelephonyState TelephonyService::state() const {
    return state_;
}
