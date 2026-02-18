#include "slic/SLICManager.h"

SLICManager::SLICManager(SlicController* controller)
    : controller_(controller), state_(SLICLineState::UNINITIALIZED), incoming_ring_(false) {}

void SLICManager::attachController(SlicController* controller) {
    controller_ = controller;
}

void SLICManager::begin() {
    if (controller_ == nullptr) {
        state_ = SLICLineState::UNINITIALIZED;
        return;
    }
    state_ = controller_->isHookOff() ? SLICLineState::OFF_HOOK : SLICLineState::ON_HOOK;
}

bool SLICManager::begin(const SlicPins& pins) {
    if (controller_ == nullptr || !controller_->begin(pins)) {
        state_ = SLICLineState::UNINITIALIZED;
        return false;
    }
    begin();
    return true;
}

void SLICManager::monitorLine() {
    if (controller_ == nullptr) {
        state_ = SLICLineState::UNINITIALIZED;
        return;
    }
    controller_->tick();
    if (incoming_ring_) {
        state_ = SLICLineState::RINGING;
    } else {
        state_ = controller_->isHookOff() ? SLICLineState::OFF_HOOK : SLICLineState::ON_HOOK;
    }
}

void SLICManager::controlCall() {
    controlCall(incoming_ring_);
}

void SLICManager::controlCall(bool incoming_ring) {
    incoming_ring_ = incoming_ring;
    if (controller_ == nullptr) {
        return;
    }
    if (incoming_ring_) {
        controller_->setRing(true);
        state_ = SLICLineState::RINGING;
    } else {
        controller_->setRing(false);
        state_ = controller_->isHookOff() ? SLICLineState::OFF_HOOK : SLICLineState::ON_HOOK;
    }
}

SLICLineState SLICManager::state() const {
    return state_;
}

bool SLICManager::isHookOff() const {
    return controller_ != nullptr && controller_->isHookOff();
}
