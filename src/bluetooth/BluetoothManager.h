#ifndef BLUETOOTH_BLUETOOTH_MANAGER_H
#define BLUETOOTH_BLUETOOTH_MANAGER_H

#include <Arduino.h>
#include <ArduinoJson.h>
#include <functional>

#include "core/PlatformProfile.h"

class AudioEngine;

class BluetoothManager {
public:
    BluetoothManager();
    bool begin(BoardProfile profile);
    void setAudioBridge(AudioEngine* audio);
    bool connect(const char* mac);
    bool disconnect();
    bool isConnected() const;
    bool startHFP();
    bool stopHFP();
    bool startBLE();
    bool stopBLE();
    void tick();
    bool setDiscoverable(bool enabled);
    bool setAutoReconnectEnabled(bool enabled);
    bool autoReconnectEnabled() const;
    void suspendReconnect(uint32_t duration_ms);
    bool dial(const String& number);
    bool redial();
    bool answerCall();
    bool hangupCall();
    bool queryCurrentCalls();
    bool syncPbapContacts();
    void logStatus() const;
    void statusToJson(JsonObject obj) const;
    void setSecurity(bool enabled);
    void setBleCommandHandler(std::function<String(const String&)> handler);
    void publishBleStatus();
    String executeBleCommand(const String& cmd);
    void handleHfpEvent(int event, const void* param);
    void onHfpIncomingAudio(const uint8_t* buf, uint32_t len);
    uint32_t onHfpOutgoingAudio(uint8_t* buf, uint32_t len);
    void onBleClientConnected(bool connected);
    void onGapAuthComplete(const uint8_t remote_bda[6], bool success);
    void onGapAclDisconnected(const uint8_t remote_bda[6], uint16_t reason);
    bool isSecurityEnabled() const;
    bool isHfpActive() const;
    bool isBleActive() const;
    bool isDiscoverable() const;
    bool isPbapSupported() const;
    String callState() const;
    String peerMac() const;

private:
    bool ensureBtStackReady();
    bool ensureHfpClientReady();
    bool requireCallControlReady(const char* operation);
    bool requestHfpConnect(const char* reason);
    bool issueDialRequest(const String& dialed, const char* event_name);
    bool loadBondedPeerFromStack();
    bool parseMac(const String& mac, uint8_t out[6]) const;
    String formatMac(const uint8_t* mac) const;
    void applyDiscoverablePolicy();
    void onHfpAudioStateChanged(bool connected);
    void requestAudioIfNeeded(const char* reason);
    bool hasAnyBluetoothClientConnected() const;

    FeatureMatrix features_;
    bool stack_ready_;
    bool hfp_initialized_;
    bool hfp_requested_;
    bool auto_reconnect_enabled_;
    bool ble_stack_initialized_;
    bool ble_service_ready_;
    bool ble_client_connected_;
    bool connected_;
    bool hfp_active_;
    bool slc_connected_;
    bool call_setup_active_;
    bool ble_active_;
    bool discoverable_;
    bool security_enabled_;
    bool pbap_supported_;
    bool pbap_synced_;
    bool hfp_data_callback_registered_;
    bool hfp_audio_link_up_;
    String peer_mac_;
    uint8_t peer_addr_[6];
    bool peer_addr_valid_;
    bool hfp_connect_inflight_;
    uint8_t hfp_reconnect_attempts_;
    uint32_t next_hfp_reconnect_ms_;
    uint32_t reconnect_suspended_until_ms_;
    uint32_t slc_wait_deadline_ms_;
    uint32_t next_audio_retry_ms_;
    uint32_t next_pending_dial_retry_ms_;
    volatile bool pending_status_publish_;
    volatile bool pending_discoverable_policy_;
    String call_state_;
    String pending_dial_number_;
    String last_dialed_number_;
    String pbap_last_error_;
    String last_hfp_event_;
    String last_ble_event_;
    String last_error_;
    String ble_last_command_;
    String ble_last_response_;
    uint32_t sco_rx_bytes_;
    uint32_t sco_tx_bytes_;
    uint32_t last_sco_activity_ms_;
    AudioEngine* audio_bridge_;
    std::function<String(const String&)> ble_command_handler_;
};

#endif  // BLUETOOTH_BLUETOOTH_MANAGER_H
