#ifndef TELEPHONY_SERVICE_H
#define TELEPHONY_SERVICE_H

#include "audio/AudioEngine.h"
#include "core/PlatformProfile.h"
#include "slic/SlicController.h"

enum class TelephonyState : uint8_t {
    IDLE = 0,
    RINGING,
    PLAYING_MESSAGE,
    OFF_HOOK
};

const char* telephonyStateToString(TelephonyState state);

class TelephonyService {
public:
    TelephonyService();
    bool begin(BoardProfile profile, SlicController& slic, AudioEngine& audio);
    void triggerIncomingRing();
    void tick();
    TelephonyState state() const;

private:
    BoardProfile profile_;
    FeatureMatrix features_;
    SlicController* slic_;
    AudioEngine* audio_;
    TelephonyState state_;
    bool incoming_ring_;
    bool ring_phase_on_;
    uint32_t ring_cycle_start_ms_;
    const char* message_path_;
};

#endif  // TELEPHONY_SERVICE_H
