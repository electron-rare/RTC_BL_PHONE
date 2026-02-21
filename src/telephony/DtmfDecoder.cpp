#include "DtmfDecoder.h"
#include <math.h>

DtmfDecoder::DtmfDecoder() : onDigit(nullptr) {}

void DtmfDecoder::setDigitCallback(DigitCallback cb) {
    onDigit = cb;
}

void DtmfDecoder::feedAudioSamples(const int16_t* samples, size_t count) {
    // TODO: Implémentation Goertzel pour détecter DTMF
    // Si détection, appeler onDigit(digit)
}
