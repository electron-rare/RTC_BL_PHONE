#ifndef TELEPHONY_SERVICE_H
#define TELEPHONY_SERVICE_H

#include <functional>

#include "audio/AudioEngine.h"
#include "core/PlatformProfile.h"
#include "slic/SlicController.h"
#include "telephony/DtmfDecoder.h"

enum class TelephonyState : uint8_t {
    IDLE = 0,
    RINGING,
    PLAYING_MESSAGE,
    OFF_HOOK
};

const char* telephonyStateToString(TelephonyState state);

class TelephonyService {
public:
    using DialCallback = std::function<bool(const String&)>;
    using AnswerCallback = std::function<bool()>;

    TelephonyService();
    bool begin(BoardProfile profile, SlicController& slic, AudioEngine& audio);
    void setDialCallback(DialCallback cb);
    void setAnswerCallback(AnswerCallback cb);
    void triggerIncomingRing();
    void setIncomingRing(bool active);
    void tick();
    TelephonyState state() const;

private:
    void onDialDigit(char digit, bool from_pulse);
    void updatePulseDecode(bool hook_off, uint32_t now);
    void commitDialBuffer(const char* reason);
    void clearDialSession();

    BoardProfile profile_;
    FeatureMatrix features_;
    SlicController* slic_;
    AudioEngine* audio_;
    DialCallback dial_callback_;
    AnswerCallback answer_callback_;
    DtmfDecoder dtmf_;
    TelephonyState state_;
    bool incoming_ring_;
    bool ring_phase_on_;
    uint32_t ring_cycle_start_ms_;
    bool capture_active_;
    bool pulse_hook_initialized_;
    bool pulse_last_hook_off_;
    bool pulse_collecting_;
    uint8_t pulse_count_;
    uint32_t last_hook_edge_ms_;
    uint32_t last_pulse_ms_;
    uint32_t dtmf_capture_start_ms_;
    uint32_t next_dtmf_read_ms_;
    uint32_t off_hook_enter_ms_;
    uint32_t last_pulse_edge_ms_;
    bool suppress_dial_tone_;
    bool dialing_started_;
    uint8_t dial_source_;
    String dial_buffer_;
    uint32_t last_digit_ms_;
    String last_dial_error_;
    const char* message_path_;
};

#endif  // TELEPHONY_SERVICE_H
