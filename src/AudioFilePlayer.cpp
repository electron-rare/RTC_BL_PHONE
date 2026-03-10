#include "AudioFilePlayer.h"

namespace {
constexpr uint32_t kPlaybackDurationMs = 3000U;
}

AudioFilePlayer::AudioFilePlayer()
    : sd_ready_(false), playing_(false), play_until_ms_(0), current_file_("") {}

bool AudioFilePlayer::begin() {
    sd_ready_ = SD.begin();
    if (!sd_ready_) {
        Serial.println("[AudioFilePlayer] SD init failed");
    }
    return sd_ready_;
}

bool AudioFilePlayer::play(const char* filename) {
    if (!sd_ready_ || filename == nullptr || filename[0] == '\0') {
        return false;
    }
    if (!SD.exists(filename)) {
        Serial.printf("[AudioFilePlayer] File not found: %s\n", filename);
        return false;
    }
    current_file_ = filename;
    playing_ = true;
    play_until_ms_ = millis() + kPlaybackDurationMs;
    Serial.printf("[AudioFilePlayer] Playing %s\n", filename);
    return true;
}

void AudioFilePlayer::loop() {
    if (playing_ && static_cast<int32_t>(millis() - play_until_ms_) >= 0) {
        playing_ = false;
        Serial.printf("[AudioFilePlayer] Playback finished: %s\n", current_file_.c_str());
        current_file_ = "";
    }
}

void AudioFilePlayer::stop() {
    playing_ = false;
    current_file_ = "";
}

bool AudioFilePlayer::isPlaying() const {
    return playing_;
}
