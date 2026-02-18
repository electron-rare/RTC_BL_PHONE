#include "audio/AudioManager.h"

AudioManager::AudioManager() : initialized_(false) {}

bool AudioManager::begin(const AudioConfig& config) {
    initialized_ = engine_.begin(config);
    return initialized_;
}

bool AudioManager::playFile(const char* path) {
    return initialized_ && engine_.playFile(path);
}

bool AudioManager::startCapture() {
    return initialized_ && engine_.startCapture();
}

size_t AudioManager::readCaptureFrame(int16_t* dst, size_t samples) {
    if (!initialized_) {
        return 0;
    }
    return engine_.readCaptureFrame(dst, samples);
}

void AudioManager::stopCapture() {
    if (!initialized_) {
        return;
    }
    engine_.stopCapture();
}

bool AudioManager::supportsFullDuplex() const {
    return initialized_ && engine_.supportsFullDuplex();
}

bool AudioManager::isPlaying() const {
    return initialized_ && engine_.isPlaying();
}

AudioRuntimeMetrics AudioManager::metrics() const {
    return engine_.metrics();
}

void AudioManager::resetMetrics() {
    engine_.resetMetrics();
}

void AudioManager::tick() {
    if (!initialized_) {
        return;
    }
    engine_.tick();
}
