#include "bluetooth/BluetoothManager.h"

#include "audio/AudioEngine.h"
#include "core/AgentSupervisor.h"

#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <WiFi.h>
#include <esp32-hal-bt.h>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_hf_client_api.h>
#include <esp_wifi.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <vector>

namespace {

constexpr char kBtDeviceName[] = "RTC_BL_PHONE_A252";
constexpr char kLegacyPinCode[] = "1234";
constexpr esp_bt_io_cap_t kSspIoCap = ESP_BT_IO_CAP_NONE;
constexpr uint8_t kBtCodMinorHandsFree = 0x02;
constexpr uint32_t kSlcWaitTimeoutMs = 20000U;
constexpr uint32_t kPendingDialRetryMs = 1200U;
constexpr uint32_t kInitialBondReconnectDelayMs = 4000U;
constexpr uint32_t kReconnectDeferMs = 1500U;
constexpr uint32_t kMinHeapForHfpConnectBytes = 45000U;

constexpr char kBleServiceUuid[] = "8fce0001-93ea-4f8f-8bde-4e8f0ea20001";
constexpr char kBleCmdUuid[] = "8fce0002-93ea-4f8f-8bde-4e8f0ea20002";
constexpr char kBleStatusUuid[] = "8fce0003-93ea-4f8f-8bde-4e8f0ea20003";

BluetoothManager* g_manager = nullptr;
BLEServer* g_ble_server = nullptr;
BLECharacteristic* g_ble_cmd_char = nullptr;
BLECharacteristic* g_ble_status_char = nullptr;
bool g_gap_callback_registered = false;
bool g_hfp_callback_registered = false;
bool g_hfp_data_callback_registered = false;
constexpr bool kEnableBtAgentSupervisorNotify = false;
constexpr bool kVerboseGapLogs = false;

void notifyBluetooth(const char* state, const char* error = "") {
    if (!kEnableBtAgentSupervisorNotify) {
        return;
    }
    AgentStatus status{state != nullptr ? state : "", error != nullptr ? error : "", millis()};
    AgentSupervisor::instance().notify("bluetooth", status);
}

String errToString(esp_err_t err) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04x", static_cast<unsigned>(err));
    return String(buf);
}

void enforceBtWifiCoexPolicy() {
    wifi_mode_t mode = WIFI_MODE_NULL;
    const esp_err_t mode_err = esp_wifi_get_mode(&mode);
    if (mode_err != ESP_OK || mode == WIFI_MODE_NULL) {
        return;
    }
    WiFi.setSleep(true);
    const esp_err_t ps_err = esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
    if (ps_err != ESP_OK && ps_err != ESP_ERR_WIFI_NOT_INIT && ps_err != ESP_ERR_WIFI_NOT_STARTED) {
        Serial.printf("[BluetoothManager] warn: esp_wifi_set_ps(min_modem) failed err=0x%04x\n",
                      static_cast<unsigned>(ps_err));
    }
}

bool isWifiScanRunning() {
    return WiFi.scanComplete() == WIFI_SCAN_RUNNING;
}

void writeMacToString(const uint8_t* mac, String& out) {
    if (mac == nullptr) {
        out = "";
        return;
    }
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    out = buf;
}

bool isDialableChar(char c) {
    return (c >= '0' && c <= '9') || c == '+' || c == '*' || c == '#' || c == ',' || c == 'p' || c == 'P' ||
           c == 'w' || c == 'W';
}

bool isValidDialNumber(const String& number) {
    if (number.isEmpty()) {
        return false;
    }
    for (size_t i = 0; i < static_cast<size_t>(number.length()); ++i) {
        if (!isDialableChar(number[static_cast<unsigned int>(i)])) {
            return false;
        }
    }
    return true;
}

bool callStateNeedsAudio(const String& call_state) {
    return call_state == "dialing" || call_state == "alerting" || call_state == "ringing" || call_state == "active" ||
           call_state == "held" || call_state == "held_active";
}

const char* callSetupStateToString(esp_hf_call_setup_status_t status) {
    switch (status) {
        case ESP_HF_CALL_SETUP_STATUS_IDLE:
            return "idle";
        case ESP_HF_CALL_SETUP_STATUS_INCOMING:
            return "ringing";
        case ESP_HF_CALL_SETUP_STATUS_OUTGOING_DIALING:
            return "dialing";
        case ESP_HF_CALL_SETUP_STATUS_OUTGOING_ALERTING:
            return "alerting";
        default:
            return "unknown";
    }
}

const char* callHeldStateToString(esp_hf_call_held_status_t status) {
    switch (status) {
        case ESP_HF_CALL_HELD_STATUS_NONE:
            return "active";
        case ESP_HF_CALL_HELD_STATUS_HELD_AND_ACTIVE:
            return "held_active";
        case ESP_HF_CALL_HELD_STATUS_HELD:
            return "held";
        default:
            return "unknown";
    }
}

const char* clccStateToString(esp_hf_current_call_status_t status) {
    switch (status) {
        case ESP_HF_CURRENT_CALL_STATUS_ACTIVE:
            return "active";
        case ESP_HF_CURRENT_CALL_STATUS_HELD:
            return "held";
        case ESP_HF_CURRENT_CALL_STATUS_DIALING:
            return "dialing";
        case ESP_HF_CURRENT_CALL_STATUS_ALERTING:
            return "alerting";
        case ESP_HF_CURRENT_CALL_STATUS_INCOMING:
        case ESP_HF_CURRENT_CALL_STATUS_WAITING:
            return "ringing";
        case ESP_HF_CURRENT_CALL_STATUS_HELD_BY_RESP_HOLD:
            return "held";
        default:
            return "unknown";
    }
}

const char* atResponseCodeToString(esp_hf_at_response_code_t code) {
    switch (code) {
        case ESP_HF_AT_RESPONSE_CODE_OK:
            return "ok";
        case ESP_HF_AT_RESPONSE_CODE_ERR:
            return "error";
        case ESP_HF_AT_RESPONSE_CODE_NO_CARRIER:
            return "no_carrier";
        case ESP_HF_AT_RESPONSE_CODE_BUSY:
            return "busy";
        case ESP_HF_AT_RESPONSE_CODE_NO_ANSWER:
            return "no_answer";
        case ESP_HF_AT_RESPONSE_CODE_DELAYED:
            return "delayed";
        case ESP_HF_AT_RESPONSE_CODE_BLACKLISTED:
            return "blacklisted";
        case ESP_HF_AT_RESPONSE_CODE_CME:
            return "cme";
        default:
            return "unknown";
    }
}

void btGapCallback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t* param) {
    if (g_manager == nullptr || param == nullptr) {
        return;
    }
    if (kVerboseGapLogs) {
        Serial.printf("[BluetoothManager] GAP event=%d\n", static_cast<int>(event));
    }

    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            const bool ok = (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS);
            if (ok) {
                if (kVerboseGapLogs) {
                    Serial.printf("[BluetoothManager] GAP auth ok bda=%02X:%02X:%02X:%02X:%02X:%02X\n",
                                  param->auth_cmpl.bda[0], param->auth_cmpl.bda[1], param->auth_cmpl.bda[2],
                                  param->auth_cmpl.bda[3], param->auth_cmpl.bda[4], param->auth_cmpl.bda[5]);
                }
                notifyBluetooth("gap_auth_ok");
            } else {
                if (kVerboseGapLogs) {
                    Serial.printf("[BluetoothManager] GAP auth failed stat=%d\n",
                                  static_cast<int>(param->auth_cmpl.stat));
                }
                notifyBluetooth("gap_auth_fail");
            }
            g_manager->onGapAuthComplete(param->auth_cmpl.bda, ok);
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT: {
            if (param->pin_req.min_16_digit) {
                esp_bt_pin_code_t pin_code;
                memset(pin_code, '0', sizeof(pin_code));
                esp_bt_gap_pin_reply(param->pin_req.bda, true, 16, pin_code);
                notifyBluetooth("gap_pin_reply_16");
                break;
            }
            esp_bt_pin_code_t pin_code;
            memset(pin_code, 0, sizeof(pin_code));
            memcpy(pin_code, kLegacyPinCode, strlen(kLegacyPinCode));
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            notifyBluetooth("gap_pin_reply");
            break;
        }
        case ESP_BT_GAP_CFM_REQ_EVT: {
            if (kVerboseGapLogs) {
                Serial.printf("[BluetoothManager] GAP cfm num=%u\n",
                              static_cast<unsigned>(param->cfm_req.num_val));
            }
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            notifyBluetooth("gap_ssp_confirm");
            break;
        }
        case ESP_BT_GAP_KEY_NOTIF_EVT: {
            if (kVerboseGapLogs) {
                Serial.printf("[BluetoothManager] GAP key notif passkey=%u\n",
                              static_cast<unsigned>(param->key_notif.passkey));
            }
            notifyBluetooth("gap_ssp_key_notif");
            break;
        }
        case ESP_BT_GAP_KEY_REQ_EVT: {
            if (kVerboseGapLogs) {
                Serial.println("[BluetoothManager] GAP key request -> passkey 123456");
            }
            esp_bt_gap_ssp_passkey_reply(param->key_req.bda, true, 123456);
            notifyBluetooth("gap_ssp_passkey");
            break;
        }
        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT: {
            const uint16_t stat = static_cast<uint16_t>(param->acl_conn_cmpl_stat.stat);
            if (kVerboseGapLogs) {
                Serial.printf("[BluetoothManager] GAP acl_conn stat=%u bda=%02X:%02X:%02X:%02X:%02X:%02X\n",
                              static_cast<unsigned>(stat),
                              param->acl_conn_cmpl_stat.bda[0], param->acl_conn_cmpl_stat.bda[1],
                              param->acl_conn_cmpl_stat.bda[2], param->acl_conn_cmpl_stat.bda[3],
                              param->acl_conn_cmpl_stat.bda[4], param->acl_conn_cmpl_stat.bda[5]);
            }
            // Do not trigger HFP actions from ACL callback context (can re-enter BT stack).
            break;
        }
        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT: {
            if (kVerboseGapLogs) {
                Serial.printf("[BluetoothManager] GAP acl_disconn reason=%d bda=%02X:%02X:%02X:%02X:%02X:%02X\n",
                              static_cast<int>(param->acl_disconn_cmpl_stat.reason),
                              param->acl_disconn_cmpl_stat.bda[0], param->acl_disconn_cmpl_stat.bda[1],
                              param->acl_disconn_cmpl_stat.bda[2], param->acl_disconn_cmpl_stat.bda[3],
                              param->acl_disconn_cmpl_stat.bda[4], param->acl_disconn_cmpl_stat.bda[5]);
            }
            g_manager->onGapAclDisconnected(param->acl_disconn_cmpl_stat.bda,
                                            param->acl_disconn_cmpl_stat.reason);
            break;
        }
        default:
            break;
    }
}

void hfpClientCallback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t* param) {
    if (g_manager != nullptr) {
        g_manager->handleHfpEvent(static_cast<int>(event), param);
    }
}

void hfpIncomingDataCallback(const uint8_t* buf, uint32_t len) {
    if (g_manager != nullptr) {
        g_manager->onHfpIncomingAudio(buf, len);
    }
}

uint32_t hfpOutgoingDataCallback(uint8_t* buf, uint32_t len) {
    if (g_manager == nullptr) {
        return 0;
    }
    return g_manager->onHfpOutgoingAudio(buf, len);
}

class ManagerBleServerCallbacks : public BLEServerCallbacks {
public:
    explicit ManagerBleServerCallbacks(BluetoothManager* manager) : manager_(manager) {}

    void onConnect(BLEServer* server) override {
        (void)server;
        if (manager_ != nullptr) {
            manager_->onBleClientConnected(true);
        }
    }

    void onDisconnect(BLEServer* server) override {
        if (manager_ != nullptr) {
            manager_->onBleClientConnected(false);
        }
        if (server != nullptr) {
            server->startAdvertising();
        }
    }

private:
    BluetoothManager* manager_;
};

class ManagerBleCommandCallbacks : public BLECharacteristicCallbacks {
public:
    explicit ManagerBleCommandCallbacks(BluetoothManager* manager) : manager_(manager) {}

    void onWrite(BLECharacteristic* characteristic) override {
        if (manager_ == nullptr || characteristic == nullptr) {
            return;
        }
        std::string raw = characteristic->getValue();
        String cmd = String(raw.c_str());
        cmd.trim();
        if (cmd.isEmpty()) {
            return;
        }
        manager_->executeBleCommand(cmd);
        manager_->publishBleStatus();
    }

private:
    BluetoothManager* manager_;
};

}  // namespace

BluetoothManager::BluetoothManager()
    : features_(getFeatureMatrix(BoardProfile::ESP32_A252)),
      stack_ready_(false),
      hfp_initialized_(false),
      hfp_requested_(false),
      auto_reconnect_enabled_(true),
      ble_stack_initialized_(false),
      ble_service_ready_(false),
      ble_client_connected_(false),
      connected_(false),
      hfp_active_(false),
      slc_connected_(false),
      call_setup_active_(false),
      ble_active_(false),
      discoverable_(false),
      security_enabled_(false),
      pbap_supported_(false),
      pbap_synced_(false),
      hfp_data_callback_registered_(false),
      hfp_audio_link_up_(false),
      peer_mac_(""),
      peer_addr_{0},
      peer_addr_valid_(false),
      hfp_connect_inflight_(false),
      hfp_reconnect_attempts_(0),
      next_hfp_reconnect_ms_(0),
      reconnect_suspended_until_ms_(0),
      slc_wait_deadline_ms_(0),
      next_audio_retry_ms_(0),
      next_pending_dial_retry_ms_(0),
      pending_status_publish_(false),
      pending_discoverable_policy_(false),
      call_state_("idle"),
      pending_dial_number_(""),
      last_dialed_number_(""),
      pbap_last_error_("pbap_not_available_on_esp32_arduino_bluedroid"),
      last_hfp_event_("idle"),
      last_ble_event_("idle"),
      last_error_(""),
      ble_last_command_(""),
      ble_last_response_(""),
      sco_rx_bytes_(0),
      sco_tx_bytes_(0),
      last_sco_activity_ms_(0),
      audio_bridge_(nullptr) {}

bool BluetoothManager::begin(BoardProfile profile) {
    features_ = getFeatureMatrix(profile);
    connected_ = false;
    hfp_active_ = false;
    slc_connected_ = false;
    call_setup_active_ = false;
    hfp_requested_ = false;
    ble_active_ = false;
    discoverable_ = false;
    pbap_synced_ = false;
    ble_client_connected_ = false;
    peer_mac_ = "";
    peer_addr_valid_ = false;
    hfp_connect_inflight_ = false;
    hfp_reconnect_attempts_ = 0;
    next_hfp_reconnect_ms_ = 0;
    reconnect_suspended_until_ms_ = 0;
    slc_wait_deadline_ms_ = 0;
    next_audio_retry_ms_ = 0;
    next_pending_dial_retry_ms_ = 0;
    pending_status_publish_ = false;
    pending_discoverable_policy_ = false;
    call_state_ = "idle";
    pending_dial_number_ = "";
    last_dialed_number_ = "";
    memset(peer_addr_, 0, sizeof(peer_addr_));
    last_error_ = "";
    last_hfp_event_ = "initialized";
    last_ble_event_ = "initialized";
    ble_last_command_ = "";
    ble_last_response_ = "";
    hfp_audio_link_up_ = false;
    hfp_data_callback_registered_ = false;
    sco_rx_bytes_ = 0;
    sco_tx_bytes_ = 0;
    last_sco_activity_ms_ = 0;
    peer_mac_.reserve(18);
    call_state_.reserve(16);
    pending_dial_number_.reserve(24);
    last_dialed_number_.reserve(24);
    last_hfp_event_.reserve(32);
    last_error_.reserve(96);
    g_manager = this;

    if (features_.has_hfp || features_.has_ble_control) {
        stack_ready_ = ensureBtStackReady();
    } else {
        stack_ready_ = true;
    }

    if (auto_reconnect_enabled_ && stack_ready_ && !peer_addr_valid_ && features_.has_hfp && loadBondedPeerFromStack()) {
        // Always-connected policy: if we have a bonded peer, reconnect automatically.
        hfp_requested_ = true;
        next_hfp_reconnect_ms_ = millis() + kInitialBondReconnectDelayMs;
        last_hfp_event_ = "peer_bond_restored";
    }

    if (stack_ready_ && features_.has_hfp) {
        if (!ensureHfpClientReady()) {
            Serial.printf("[BluetoothManager] HFP init at boot failed: %s\n", last_error_.c_str());
        } else {
            Serial.println("[BluetoothManager] HFP initialized at boot");
        }
    }
    applyDiscoverablePolicy();
    notifyBluetooth("initialized");
    return stack_ready_;
}

void BluetoothManager::setAudioBridge(AudioEngine* audio) {
    audio_bridge_ = audio;
}

bool BluetoothManager::connect(const char* mac) {
    if (mac == nullptr || mac[0] == '\0') {
        last_error_ = "invalid_mac";
        notifyBluetooth("connect_failed", "invalid_mac");
        return false;
    }
    if (!features_.has_hfp) {
        last_error_ = "hfp_not_supported";
        notifyBluetooth("connect_failed", "hfp_not_supported");
        return false;
    }
    if (!ensureHfpClientReady()) {
        return false;
    }

    uint8_t addr[6] = {0};
    if (!parseMac(String(mac), addr)) {
        last_error_ = "invalid_mac";
        notifyBluetooth("connect_failed", "invalid_mac");
        return false;
    }

    memcpy(peer_addr_, addr, sizeof(peer_addr_));
    peer_addr_valid_ = true;
    writeMacToString(peer_addr_, peer_mac_);
    hfp_requested_ = true;
    hfp_reconnect_attempts_ = 0;
    next_hfp_reconnect_ms_ = 0;
    return requestHfpConnect("connect_requested");
}

bool BluetoothManager::disconnect() {
    bool ok = true;
    if (hfp_initialized_ && peer_addr_valid_) {
        esp_hf_client_disconnect_audio(peer_addr_);
        const esp_err_t err = esp_hf_client_disconnect(peer_addr_);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            last_error_ = "hfp_disconnect_failed:" + errToString(err);
            ok = false;
        }
    }

    connected_ = false;
    hfp_active_ = false;
    hfp_audio_link_up_ = false;
    hfp_requested_ = false;
    hfp_reconnect_attempts_ = 0;
    next_hfp_reconnect_ms_ = 0;
    hfp_connect_inflight_ = false;
    next_audio_retry_ms_ = 0;
    next_pending_dial_retry_ms_ = 0;
    pending_dial_number_ = "";
    last_hfp_event_ = "disconnect_requested";
    notifyBluetooth(ok ? "disconnected" : "disconnect_failed", ok ? "" : last_error_.c_str());
    onHfpAudioStateChanged(false);
    applyDiscoverablePolicy();
    publishBleStatus();
    return ok;
}

bool BluetoothManager::isConnected() const {
    return connected_;
}

bool BluetoothManager::startHFP() {
    if (!features_.has_hfp) {
        last_error_ = "hfp_not_supported";
        notifyBluetooth("hfp_failed", "hfp_not_supported");
        return false;
    }
    if (!ensureHfpClientReady()) {
        return false;
    }

    hfp_requested_ = true;
    last_hfp_event_ = "audio_requested";

    if (connected_ && peer_addr_valid_) {
        const esp_err_t err = esp_hf_client_connect_audio(peer_addr_);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            last_error_ = "hfp_audio_connect_failed:" + errToString(err);
            notifyBluetooth("hfp_failed", last_error_.c_str());
            return false;
        }
    }

    notifyBluetooth("hfp_started");
    publishBleStatus();
    return true;
}

bool BluetoothManager::stopHFP() {
    hfp_requested_ = false;
    if (connected_ && peer_addr_valid_ && hfp_initialized_) {
        const esp_err_t err = esp_hf_client_disconnect_audio(peer_addr_);
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            last_error_ = "hfp_audio_disconnect_failed:" + errToString(err);
            notifyBluetooth("hfp_failed", last_error_.c_str());
            return false;
        }
    }

    hfp_active_ = false;
    hfp_audio_link_up_ = false;
    next_audio_retry_ms_ = 0;
    last_hfp_event_ = "audio_stop_requested";
    notifyBluetooth("hfp_stopped");
    onHfpAudioStateChanged(false);
    publishBleStatus();
    return true;
}

bool BluetoothManager::startBLE() {
    if (!features_.has_ble_control) {
        last_error_ = "ble_not_supported";
        notifyBluetooth("ble_failed", "ble_not_supported");
        return false;
    }
    if (!ensureBtStackReady()) {
        return false;
    }

    if (!ble_stack_initialized_) {
        BLEDevice::init(kBtDeviceName);
        ble_stack_initialized_ = true;
    }

    if (!ble_service_ready_) {
        g_ble_server = BLEDevice::createServer();
        g_ble_server->setCallbacks(new ManagerBleServerCallbacks(this));

        BLEService* service = g_ble_server->createService(kBleServiceUuid);
        g_ble_cmd_char = service->createCharacteristic(
            kBleCmdUuid,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE |
                BLECharacteristic::PROPERTY_WRITE_NR);
        g_ble_status_char = service->createCharacteristic(
            kBleStatusUuid,
            BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

        g_ble_cmd_char->setCallbacks(new ManagerBleCommandCallbacks(this));
        g_ble_status_char->addDescriptor(new BLE2902());

        service->start();
        BLEAdvertising* advertising = BLEDevice::getAdvertising();
        advertising->addServiceUUID(kBleServiceUuid);
        advertising->setScanResponse(true);
        advertising->setMinPreferred(0x06);
        advertising->setMinPreferred(0x12);
        ble_service_ready_ = true;
    }

    BLEDevice::getAdvertising()->start();
    ble_active_ = true;
    last_ble_event_ = "advertising";
    notifyBluetooth("ble_started");
    publishBleStatus();
    return true;
}

bool BluetoothManager::stopBLE() {
    if (ble_stack_initialized_) {
        BLEDevice::getAdvertising()->stop();
    }
    ble_active_ = false;
    ble_client_connected_ = false;
    last_ble_event_ = "stopped";
    notifyBluetooth("ble_stopped");
    publishBleStatus();
    return true;
}

void BluetoothManager::tick() {
    const uint32_t now = millis();
    const bool reconnect_suspended = (reconnect_suspended_until_ms_ != 0U && now < reconnect_suspended_until_ms_);

    if (!pending_dial_number_.isEmpty()) {
        if (connected_ && slc_connected_ && now >= next_pending_dial_retry_ms_) {
            const String queued = pending_dial_number_;
            if (issueDialRequest(queued, "dial_pending_requested")) {
                pending_dial_number_ = "";
                next_pending_dial_retry_ms_ = 0;
            } else {
                next_pending_dial_retry_ms_ = now + kPendingDialRetryMs;
            }
        } else if (!connected_ && peer_addr_valid_ && hfp_requested_ && next_hfp_reconnect_ms_ == 0) {
            next_hfp_reconnect_ms_ = now + 300U;
        }
    }

    if (hfp_requested_ && peer_addr_valid_ && !connected_ && !hfp_connect_inflight_ && next_hfp_reconnect_ms_ != 0 &&
        now >= next_hfp_reconnect_ms_) {
        if (reconnect_suspended) {
            next_hfp_reconnect_ms_ = reconnect_suspended_until_ms_;
            last_hfp_event_ = "reconnect_suspended";
        } else if (isWifiScanRunning() || ESP.getFreeHeap() < kMinHeapForHfpConnectBytes) {
            next_hfp_reconnect_ms_ = now + kReconnectDeferMs;
            last_hfp_event_ = isWifiScanRunning() ? "reconnect_deferred_wifi_scan" : "reconnect_deferred_low_heap";
        } else if (!requestHfpConnect("reconnect_tick")) {
            const uint8_t exp = (hfp_reconnect_attempts_ < 5) ? hfp_reconnect_attempts_ : 5;
            next_hfp_reconnect_ms_ = now + 2000U * (1U << exp);
        }
    }

    const bool call_needs_audio = callStateNeedsAudio(call_state_);
    if (call_needs_audio && connected_ && slc_connected_ && peer_addr_valid_ && !hfp_audio_link_up_ &&
        now >= next_audio_retry_ms_) {
        requestAudioIfNeeded("audio_retry_tick");
    }

    if (connected_ && !slc_connected_ && peer_addr_valid_ && slc_wait_deadline_ms_ != 0 &&
        now >= slc_wait_deadline_ms_) {
        // Avoid aggressive disconnect from tick() while stack is negotiating SLC.
        // This path was triggering unstable queue teardown in Bluedroid.
        last_hfp_event_ = "slc_timeout_backoff";
        slc_wait_deadline_ms_ = now + kSlcWaitTimeoutMs;
        hfp_connect_inflight_ = false;
        if ((auto_reconnect_enabled_ || !pending_dial_number_.isEmpty()) && next_hfp_reconnect_ms_ == 0) {
            next_hfp_reconnect_ms_ = now + 2000U;
        }
    }

    if (pending_discoverable_policy_) {
        pending_discoverable_policy_ = false;
        applyDiscoverablePolicy();
    }
    if (pending_status_publish_) {
        pending_status_publish_ = false;
        publishBleStatus();
    }
}

void BluetoothManager::logStatus() const {
    Serial.printf("[BluetoothManager] stack=%s connected=%s hfp=%s ble=%s ble_client=%s security=%s peer=%s\n",
                  stack_ready_ ? "true" : "false",
                  connected_ ? "true" : "false",
                  hfp_active_ ? "true" : "false",
                  ble_active_ ? "true" : "false",
                  ble_client_connected_ ? "true" : "false",
                  security_enabled_ ? "true" : "false",
                  peer_mac_.c_str());
}

void BluetoothManager::statusToJson(JsonObject obj) const {
    obj["stack_ready"] = stack_ready_;
    obj["connected"] = connected_;
    obj["hfp_active"] = hfp_active_;
    obj["hfp_audio_link"] = hfp_audio_link_up_;
    obj["slc_connected"] = slc_connected_;
    obj["hfp_requested"] = hfp_requested_;
    obj["auto_reconnect_enabled"] = auto_reconnect_enabled_;
    obj["reconnect_suspended_until_ms"] = reconnect_suspended_until_ms_;
    obj["call_state"] = call_state_;
    obj["call_setup_active"] = call_setup_active_;
    obj["pending_dial_number"] = pending_dial_number_;
    obj["last_dialed_number"] = last_dialed_number_;
    obj["ble_active"] = ble_active_;
    obj["discoverable"] = discoverable_;
    obj["ble_client_connected"] = ble_client_connected_;
    obj["security_enabled"] = security_enabled_;
    obj["pbap_supported"] = pbap_supported_;
    obj["pbap_synced"] = pbap_synced_;
    obj["pbap_last_error"] = pbap_last_error_;
    obj["peer"] = peer_mac_;
    obj["last_hfp_event"] = last_hfp_event_;
    obj["last_ble_event"] = last_ble_event_;
    obj["last_error"] = last_error_;
    obj["sco_rx_bytes"] = sco_rx_bytes_;
    obj["sco_tx_bytes"] = sco_tx_bytes_;
    obj["last_sco_activity_ms"] = last_sco_activity_ms_;
    obj["ble_last_command"] = ble_last_command_;
    obj["ble_last_response"] = ble_last_response_;
}

void BluetoothManager::setSecurity(bool enabled) {
    security_enabled_ = enabled;
}

bool BluetoothManager::setDiscoverable(bool enabled) {
    if (!ensureBtStackReady()) {
        return false;
    }
    const bool target = enabled;
    if (discoverable_ == target) {
        return true;
    }
    const esp_bt_discovery_mode_t mode =
        target ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE;
    const esp_bt_connection_mode_t conn_mode = target ? ESP_BT_CONNECTABLE : ESP_BT_NON_CONNECTABLE;
    const esp_err_t err = esp_bt_gap_set_scan_mode(conn_mode, mode);
    if (err != ESP_OK) {
        last_error_ = "bt_discoverable_failed:" + errToString(err);
        notifyBluetooth("discoverable_failed", last_error_.c_str());
        return false;
    }
    discoverable_ = target;
    last_error_ = "";
    notifyBluetooth(target ? "discoverable_on" : "discoverable_off");
    publishBleStatus();
    return true;
}

bool BluetoothManager::setAutoReconnectEnabled(bool enabled) {
    auto_reconnect_enabled_ = enabled;
    if (!auto_reconnect_enabled_) {
        hfp_reconnect_attempts_ = 0;
        next_hfp_reconnect_ms_ = 0;
        if (pending_dial_number_.isEmpty() && !connected_) {
            hfp_requested_ = false;
        }
        last_hfp_event_ = "auto_reconnect_off";
    } else {
        hfp_requested_ = true;
        if (!connected_ && peer_addr_valid_ && !hfp_connect_inflight_) {
            next_hfp_reconnect_ms_ = millis() + 400U;
        }
        last_hfp_event_ = "auto_reconnect_on";
    }
    publishBleStatus();
    return true;
}

bool BluetoothManager::autoReconnectEnabled() const {
    return auto_reconnect_enabled_;
}

void BluetoothManager::suspendReconnect(uint32_t duration_ms) {
    if (duration_ms == 0U) {
        return;
    }
    const uint32_t until = millis() + duration_ms;
    if (reconnect_suspended_until_ms_ == 0U || until > reconnect_suspended_until_ms_) {
        reconnect_suspended_until_ms_ = until;
    }
    if (next_hfp_reconnect_ms_ != 0U && next_hfp_reconnect_ms_ < reconnect_suspended_until_ms_) {
        next_hfp_reconnect_ms_ = reconnect_suspended_until_ms_;
    }
}

bool BluetoothManager::dial(const String& number) {
    String dialed = number;
    dialed.trim();
    if (!isValidDialNumber(dialed)) {
        last_error_ = "dial_invalid_number";
        notifyBluetooth("hfp_dial_failed", last_error_.c_str());
        return false;
    }

    if (!features_.has_hfp) {
        last_error_ = "hfp_not_supported";
        notifyBluetooth("hfp_dial_failed", last_error_.c_str());
        return false;
    }

    const bool hfp_ready = ensureHfpClientReady();

    if (connected_ && slc_connected_) {
        if (hfp_ready) {
            return issueDialRequest(dialed, "dial_requested");
        }
        pending_dial_number_ = dialed;
        next_pending_dial_retry_ms_ = millis();
        hfp_requested_ = true;
        if (next_hfp_reconnect_ms_ == 0) {
            next_hfp_reconnect_ms_ = millis() + kPendingDialRetryMs;
        }
        last_hfp_event_ = "dial_queued_wait_stack";
        notifyBluetooth("hfp_dial_queued_wait_stack");
        publishBleStatus();
        return true;
    }

    pending_dial_number_ = dialed;
    next_pending_dial_retry_ms_ = millis();
    hfp_requested_ = true;
    last_error_ = "";

    if (!hfp_ready) {
        if (stack_ready_) {
            setDiscoverable(true);
        }
        last_hfp_event_ = "dial_queued_wait_stack";
        if (next_hfp_reconnect_ms_ == 0) {
            next_hfp_reconnect_ms_ = millis() + kPendingDialRetryMs;
        }
        notifyBluetooth("hfp_dial_queued_wait_stack");
        publishBleStatus();
        return true;
    }

    if (!peer_addr_valid_) {
        if (!loadBondedPeerFromStack()) {
            last_hfp_event_ = "dial_queued_wait_pair";
            setDiscoverable(true);
            notifyBluetooth("hfp_dial_queued_wait_pair");
            publishBleStatus();
            return true;
        }
        last_hfp_event_ = "dial_queued_peer_restored";
    }

    if (!connected_) {
        setDiscoverable(true);
        const bool connect_requested = requestHfpConnect("dial_queue_connect_requested");
        if (!connect_requested && next_hfp_reconnect_ms_ == 0) {
            next_hfp_reconnect_ms_ = millis() + kPendingDialRetryMs;
        }
        last_hfp_event_ = connect_requested ? "dial_queued_connecting" : "dial_queued_wait_reconnect";
        notifyBluetooth(connect_requested ? "hfp_dial_queued_connecting" : "hfp_dial_queued_wait_reconnect");
    } else {
        if (slc_wait_deadline_ms_ == 0) {
            slc_wait_deadline_ms_ = millis() + kSlcWaitTimeoutMs;
        }
        last_hfp_event_ = "dial_queued_wait_slc";
        notifyBluetooth("hfp_dial_queued_wait_slc");
    }

    publishBleStatus();
    return true;
}

bool BluetoothManager::issueDialRequest(const String& dialed, const char* event_name) {
    const esp_err_t err = esp_hf_client_dial(dialed.c_str());
    if (err != ESP_OK) {
        last_error_ = "hfp_dial_failed:" + errToString(err);
        notifyBluetooth("hfp_dial_failed", last_error_.c_str());
        return false;
    }

    last_dialed_number_ = dialed;
    call_state_ = "dialing";
    last_hfp_event_ = event_name != nullptr ? String(event_name) : String("dial_requested");
    last_error_ = "";
    notifyBluetooth("hfp_dial_requested");
    publishBleStatus();
    return true;
}

bool BluetoothManager::redial() {
    if (!requireCallControlReady("redial")) {
        return false;
    }

    const esp_err_t err = esp_hf_client_dial(nullptr);
    if (err != ESP_OK) {
        last_error_ = "hfp_redial_failed:" + errToString(err);
        notifyBluetooth("hfp_redial_failed", last_error_.c_str());
        return false;
    }

    call_state_ = "dialing";
    last_hfp_event_ = "redial_requested";
    notifyBluetooth("hfp_redial_requested");
    publishBleStatus();
    return true;
}

bool BluetoothManager::answerCall() {
    if (!requireCallControlReady("answer")) {
        return false;
    }

    const esp_err_t err = esp_hf_client_answer_call();
    if (err != ESP_OK) {
        last_error_ = "hfp_answer_failed:" + errToString(err);
        notifyBluetooth("hfp_answer_failed", last_error_.c_str());
        return false;
    }

    call_state_ = "active";
    last_hfp_event_ = "answer_requested";
    notifyBluetooth("hfp_answer_requested");
    publishBleStatus();
    return true;
}

bool BluetoothManager::hangupCall() {
    if (!requireCallControlReady("hangup")) {
        return false;
    }

    const esp_err_t err = esp_hf_client_reject_call();
    if (err != ESP_OK) {
        last_error_ = "hfp_hangup_failed:" + errToString(err);
        notifyBluetooth("hfp_hangup_failed", last_error_.c_str());
        return false;
    }

    call_state_ = "ending";
    last_hfp_event_ = "hangup_requested";
    notifyBluetooth("hfp_hangup_requested");
    publishBleStatus();
    return true;
}

bool BluetoothManager::queryCurrentCalls() {
    if (!requireCallControlReady("calls_query")) {
        return false;
    }

    const esp_err_t err = esp_hf_client_query_current_calls();
    if (err != ESP_OK) {
        last_error_ = "hfp_calls_query_failed:" + errToString(err);
        notifyBluetooth("hfp_calls_query_failed", last_error_.c_str());
        return false;
    }

    last_hfp_event_ = "calls_query_requested";
    notifyBluetooth("hfp_calls_query_requested");
    publishBleStatus();
    return true;
}

bool BluetoothManager::syncPbapContacts() {
    pbap_synced_ = false;
    pbap_last_error_ = "pbap_not_available_on_esp32_arduino_bluedroid";
    last_error_ = pbap_last_error_;
    notifyBluetooth("pbap_sync_unsupported", pbap_last_error_.c_str());
    publishBleStatus();
    return false;
}

void BluetoothManager::setBleCommandHandler(std::function<String(const String&)> handler) {
    ble_command_handler_ = std::move(handler);
}

bool BluetoothManager::isSecurityEnabled() const {
    return security_enabled_;
}

bool BluetoothManager::isHfpActive() const {
    return hfp_active_;
}

bool BluetoothManager::isBleActive() const {
    return ble_active_;
}

bool BluetoothManager::isDiscoverable() const {
    return discoverable_;
}

bool BluetoothManager::hasAnyBluetoothClientConnected() const {
    return connected_ || ble_client_connected_;
}

void BluetoothManager::applyDiscoverablePolicy() {
    if (!stack_ready_) {
        return;
    }

    // Policy: discoverable when no BT client is connected, hidden otherwise.
    const bool should_discoverable = !hasAnyBluetoothClientConnected();
    if (discoverable_ == should_discoverable) {
        return;
    }

    if (!setDiscoverable(should_discoverable)) {
        Serial.printf("[BluetoothManager] discoverable policy apply failed: target=%s err=%s\n",
                      should_discoverable ? "on" : "off",
                      last_error_.c_str());
    }
}

bool BluetoothManager::isPbapSupported() const {
    return pbap_supported_;
}

String BluetoothManager::callState() const {
    return call_state_;
}

String BluetoothManager::peerMac() const {
    return peer_mac_;
}

bool BluetoothManager::ensureBtStackReady() {
    if (stack_ready_) {
        return true;
    }

    enforceBtWifiCoexPolicy();
    if (!btStart()) {
        last_error_ = "bt_start_failed";
        notifyBluetooth("stack_failed", "bt_start_failed");
        return false;
    }

    if (esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED) {
        const esp_err_t err = esp_bluedroid_init();
        if (err != ESP_OK) {
            last_error_ = "bluedroid_init_failed:" + errToString(err);
            notifyBluetooth("stack_failed", last_error_.c_str());
            return false;
        }
    }

    if (esp_bluedroid_get_status() != ESP_BLUEDROID_STATUS_ENABLED) {
        const esp_err_t err = esp_bluedroid_enable();
        if (err != ESP_OK) {
            last_error_ = "bluedroid_enable_failed:" + errToString(err);
            notifyBluetooth("stack_failed", last_error_.c_str());
            return false;
        }
    }

    esp_bt_dev_set_device_name(kBtDeviceName);

    esp_bt_io_cap_t iocap = kSspIoCap;
    const esp_err_t sec_err =
        esp_bt_gap_set_security_param(ESP_BT_SP_IOCAP_MODE, &iocap, sizeof(iocap));
    if (sec_err != ESP_OK) {
        last_error_ = "gap_security_param_failed:" + errToString(sec_err);
        notifyBluetooth("stack_warn", last_error_.c_str());
    }

    esp_bt_cod_t cod = {};
    cod.major = ESP_BT_COD_MAJOR_DEV_AV;
    cod.minor = kBtCodMinorHandsFree;
    cod.service = ESP_BT_COD_SRVC_AUDIO | ESP_BT_COD_SRVC_TELEPHONY;
    const esp_err_t cod_err = esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_ALL);
    if (cod_err != ESP_OK) {
        last_error_ = "gap_set_cod_failed:" + errToString(cod_err);
        notifyBluetooth("stack_warn", last_error_.c_str());
    }

    if (!g_gap_callback_registered) {
        const esp_err_t gap_err = esp_bt_gap_register_callback(btGapCallback);
        if (gap_err == ESP_OK) {
            g_gap_callback_registered = true;
        } else {
            last_error_ = "gap_callback_failed:" + errToString(gap_err);
            notifyBluetooth("stack_failed", last_error_.c_str());
            return false;
        }
    }

    esp_bt_pin_code_t pin_code;
    memset(pin_code, 0, sizeof(pin_code));
    memcpy(pin_code, kLegacyPinCode, strlen(kLegacyPinCode));
    esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, pin_code);
    // Default hardening: no inbound ACL unless explicitly requested.
    const esp_err_t scan_err =
        esp_bt_gap_set_scan_mode(ESP_BT_NON_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    if (scan_err != ESP_OK) {
        last_error_ = "gap_scan_mode_failed:" + errToString(scan_err);
        notifyBluetooth("stack_failed", last_error_.c_str());
        return false;
    }
    discoverable_ = false;

    enforceBtWifiCoexPolicy();
    stack_ready_ = true;
    notifyBluetooth("stack_ready");
    return true;
}

bool BluetoothManager::ensureHfpClientReady() {
    if (hfp_initialized_) {
        return true;
    }
    if (!ensureBtStackReady()) {
        return false;
    }

    if (!g_hfp_callback_registered) {
        const esp_err_t cb_err = esp_hf_client_register_callback(hfpClientCallback);
        if (cb_err != ESP_OK) {
            last_error_ = "hfp_callback_failed:" + errToString(cb_err);
            notifyBluetooth("hfp_failed", last_error_.c_str());
            return false;
        }
        g_hfp_callback_registered = true;
    }

    const esp_err_t init_err = esp_hf_client_init();
    if (init_err != ESP_OK && init_err != ESP_ERR_INVALID_STATE) {
        last_error_ = "hfp_init_failed:" + errToString(init_err);
        notifyBluetooth("hfp_failed", last_error_.c_str());
        return false;
    }

    if (!g_hfp_data_callback_registered) {
        const esp_err_t data_err =
            esp_hf_client_register_data_callback(hfpIncomingDataCallback, hfpOutgoingDataCallback);
        if (data_err == ESP_OK) {
            g_hfp_data_callback_registered = true;
        } else {
            last_error_ = "hfp_data_callback_failed:" + errToString(data_err);
            notifyBluetooth("hfp_warn", last_error_.c_str());
        }
    }
    hfp_data_callback_registered_ = g_hfp_data_callback_registered;

    hfp_initialized_ = true;
    notifyBluetooth("hfp_ready");
    return true;
}

bool BluetoothManager::requestHfpConnect(const char* reason) {
    if (!ensureHfpClientReady()) {
        return false;
    }
    if (!peer_addr_valid_) {
        last_error_ = "hfp_connect_no_peer";
        notifyBluetooth("connect_failed", last_error_.c_str());
        return false;
    }

    if (hfp_connect_inflight_) {
        return true;
    }

    if (reconnect_suspended_until_ms_ != 0U && millis() < reconnect_suspended_until_ms_) {
        next_hfp_reconnect_ms_ = reconnect_suspended_until_ms_;
        last_hfp_event_ = "connect_suspended";
        return false;
    }

    if (isWifiScanRunning() || ESP.getFreeHeap() < kMinHeapForHfpConnectBytes) {
        next_hfp_reconnect_ms_ = millis() + kReconnectDeferMs;
        last_hfp_event_ = isWifiScanRunning() ? "connect_deferred_wifi_scan" : "connect_deferred_low_heap";
        return false;
    }

    const esp_err_t err = esp_hf_client_connect(peer_addr_);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        last_error_ = "hfp_connect_req_failed:" + errToString(err);
        notifyBluetooth("connect_failed", last_error_.c_str());
        return false;
    }

    last_hfp_event_ = reason != nullptr ? String(reason) : String("connect_requested");
    last_error_ = "";
    hfp_connect_inflight_ = true;
    next_hfp_reconnect_ms_ = 0;
    notifyBluetooth("hfp_connecting");
    return true;
}

bool BluetoothManager::requireCallControlReady(const char* operation) {
    if (!features_.has_hfp) {
        last_error_ = "hfp_not_supported";
        notifyBluetooth("hfp_call_control_unavailable", last_error_.c_str());
        return false;
    }

    if (!ensureHfpClientReady()) {
        return false;
    }

    if (!connected_ || !slc_connected_) {
        last_error_ = String(operation) + ":hfp_not_connected";
        notifyBluetooth("hfp_call_control_unavailable", last_error_.c_str());
        return false;
    }

    return true;
}

bool BluetoothManager::loadBondedPeerFromStack() {
    int dev_num = esp_bt_gap_get_bond_device_num();
    if (dev_num <= 0) {
        return false;
    }

    std::vector<esp_bd_addr_t> dev_list(static_cast<size_t>(dev_num));
    int list_count = dev_num;
    const esp_err_t err = esp_bt_gap_get_bond_device_list(&list_count, dev_list.data());
    if (err != ESP_OK || list_count <= 0) {
        return false;
    }

    memcpy(peer_addr_, dev_list[0], sizeof(peer_addr_));
    peer_addr_valid_ = true;
    writeMacToString(peer_addr_, peer_mac_);
    return true;
}

bool BluetoothManager::parseMac(const String& mac, uint8_t out[6]) const {
    if (out == nullptr) {
        return false;
    }

    int values[6] = {0};
    if (sscanf(mac.c_str(), "%x:%x:%x:%x:%x:%x",
               &values[0], &values[1], &values[2],
               &values[3], &values[4], &values[5]) != 6) {
        return false;
    }
    for (int i = 0; i < 6; ++i) {
        if (values[i] < 0 || values[i] > 255) {
            return false;
        }
        out[i] = static_cast<uint8_t>(values[i]);
    }
    return true;
}

String BluetoothManager::formatMac(const uint8_t* mac) const {
    if (mac == nullptr) {
        return "";
    }
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
}

void BluetoothManager::publishBleStatus() {
    if (g_ble_status_char == nullptr || !ble_service_ready_) {
        return;
    }
    JsonDocument doc;
    statusToJson(doc.to<JsonObject>());
    String payload;
    serializeJson(doc, payload);
    g_ble_status_char->setValue(payload.c_str());
    if (ble_client_connected_) {
        g_ble_status_char->notify();
    }
}

String BluetoothManager::executeBleCommand(const String& cmd) {
    ble_last_command_ = cmd;
    String response;

    if (ble_command_handler_) {
        response = ble_command_handler_(cmd);
    } else if (cmd.equalsIgnoreCase("BT_STATUS")) {
        JsonDocument doc;
        statusToJson(doc.to<JsonObject>());
        serializeJson(doc, response);
    } else {
        response = "ERR BLE_CMD unsupported";
    }

    if (response.startsWith("ERR")) {
        last_error_ = response;
    }
    ble_last_response_ = response;
    last_ble_event_ = "cmd";
    return response;
}

void BluetoothManager::onHfpIncomingAudio(const uint8_t* buf, uint32_t len) {
    if (buf == nullptr || len == 0) {
        return;
    }

    sco_rx_bytes_ += len;
    last_sco_activity_ms_ = millis();

    if (audio_bridge_ == nullptr) {
        return;
    }

    const size_t samples = static_cast<size_t>(len / sizeof(int16_t));
    if (samples == 0) {
        return;
    }
    audio_bridge_->writePlaybackFrame(reinterpret_cast<const int16_t*>(buf), samples);
}

uint32_t BluetoothManager::onHfpOutgoingAudio(uint8_t* buf, uint32_t len) {
    if (buf == nullptr || len < sizeof(int16_t)) {
        return 0;
    }

    memset(buf, 0, len);
    if (audio_bridge_ == nullptr) {
        return 0;
    }

    constexpr size_t kChunkSamples = 160;
    uint32_t produced_bytes = 0;
    uint8_t* out = buf;
    uint32_t remaining = len;

    while (remaining >= sizeof(int16_t)) {
        const size_t request_samples = std::min(static_cast<size_t>(remaining / sizeof(int16_t)), kChunkSamples);
        int16_t tmp[kChunkSamples] = {0};
        const size_t got = audio_bridge_->readCaptureFrameNonBlocking(tmp, request_samples);
        if (got == 0) {
            break;
        }
        const uint32_t chunk_bytes = static_cast<uint32_t>(got * sizeof(int16_t));
        memcpy(out, tmp, chunk_bytes);
        out += chunk_bytes;
        remaining -= chunk_bytes;
        produced_bytes += chunk_bytes;
        if (got < request_samples) {
            break;
        }
    }

    if (produced_bytes > 0) {
        sco_tx_bytes_ += produced_bytes;
        last_sco_activity_ms_ = millis();
    }
    return produced_bytes;
}

void BluetoothManager::handleHfpEvent(int event, const void* raw_param) {
    const auto* param = static_cast<const esp_hf_client_cb_param_t*>(raw_param);
    switch (static_cast<esp_hf_client_cb_event_t>(event)) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT: {
            if (param != nullptr) {
                memcpy(peer_addr_, param->conn_stat.remote_bda, sizeof(peer_addr_));
                peer_addr_valid_ = true;
                writeMacToString(peer_addr_, peer_mac_);
                switch (param->conn_stat.state) {
                    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED:
                        connected_ = false;
                        hfp_connect_inflight_ = false;
                        hfp_active_ = false;
                        hfp_audio_link_up_ = false;
                        slc_connected_ = false;
                        call_setup_active_ = false;
                        slc_wait_deadline_ms_ = 0;
                        call_state_ = "idle";
                        next_audio_retry_ms_ = 0;
                        last_hfp_event_ = "disconnected";
                        notifyBluetooth("hfp_disconnected");
                        onHfpAudioStateChanged(false);
                        if ((auto_reconnect_enabled_ || !pending_dial_number_.isEmpty()) &&
                            hfp_requested_ && peer_addr_valid_) {
                            hfp_reconnect_attempts_++;
                            const uint8_t exp = (hfp_reconnect_attempts_ < 5) ? hfp_reconnect_attempts_ : 5;
                            next_hfp_reconnect_ms_ = millis() + 2000U * (1U << exp);
                        }
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING:
                        connected_ = false;
                        hfp_connect_inflight_ = true;
                        hfp_active_ = false;
                        hfp_audio_link_up_ = false;
                        slc_connected_ = false;
                        call_setup_active_ = false;
                        slc_wait_deadline_ms_ = 0;
                        last_hfp_event_ = "connecting";
                        notifyBluetooth("hfp_connecting");
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED:
                        connected_ = true;
                        hfp_connect_inflight_ = false;
                        hfp_active_ = false;
                        hfp_audio_link_up_ = false;
                        slc_connected_ = false;
                        call_setup_active_ = false;
                        slc_wait_deadline_ms_ = millis() + kSlcWaitTimeoutMs;
                        next_audio_retry_ms_ = 0;
                        hfp_reconnect_attempts_ = 0;
                        next_hfp_reconnect_ms_ = 0;
                        last_hfp_event_ = "rfcomm_connected";
                        notifyBluetooth("hfp_rfcomm_connected");
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED:
                        connected_ = true;
                        hfp_connect_inflight_ = false;
                        hfp_active_ = false;
                        hfp_audio_link_up_ = false;
                        slc_connected_ = true;
                        call_setup_active_ = false;
                        slc_wait_deadline_ms_ = 0;
                        next_audio_retry_ms_ = millis() + 500U;
                        if (!pending_dial_number_.isEmpty()) {
                            next_pending_dial_retry_ms_ = millis();
                        }
                        hfp_reconnect_attempts_ = 0;
                        next_hfp_reconnect_ms_ = 0;
                        last_hfp_event_ = "slc_connected";
                        notifyBluetooth("hfp_slc_connected");
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING:
                        hfp_connect_inflight_ = false;
                        slc_connected_ = false;
                        call_setup_active_ = false;
                        slc_wait_deadline_ms_ = 0;
                        next_audio_retry_ms_ = 0;
                        last_hfp_event_ = "disconnecting";
                        notifyBluetooth("hfp_disconnecting");
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case ESP_HF_CLIENT_AUDIO_STATE_EVT: {
            if (param != nullptr) {
                writeMacToString(param->audio_stat.remote_bda, peer_mac_);
                switch (param->audio_stat.state) {
                    case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED:
                        hfp_audio_link_up_ = false;
                        hfp_active_ = false;
                        if (call_state_ == "dialing" || call_state_ == "alerting" || call_state_ == "ringing" ||
                            call_state_ == "active" || call_state_ == "held" || call_state_ == "held_active") {
                            next_audio_retry_ms_ = millis() + 500U;
                        } else {
                            next_audio_retry_ms_ = 0;
                        }
                        last_hfp_event_ = "audio_disconnected";
                        notifyBluetooth("hfp_audio_disconnected");
                        onHfpAudioStateChanged(false);
                        break;
                    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTING:
                        last_hfp_event_ = "audio_connecting";
                        notifyBluetooth("hfp_audio_connecting");
                        break;
                    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED:
                    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC:
                        hfp_audio_link_up_ = true;
                        hfp_active_ = true;
                        next_audio_retry_ms_ = 0;
                        last_hfp_event_ = "audio_connected";
                        notifyBluetooth("hfp_audio_connected");
                        onHfpAudioStateChanged(true);
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case ESP_HF_CLIENT_CIND_CALL_SETUP_EVT:
            if (param != nullptr) {
                call_state_ = callSetupStateToString(param->call_setup.status);
                call_setup_active_ = (param->call_setup.status != ESP_HF_CALL_SETUP_STATUS_IDLE);
                if (param->call_setup.status != ESP_HF_CALL_SETUP_STATUS_IDLE) {
                    next_audio_retry_ms_ = millis() + 80U;
                }
            }
            last_hfp_event_ = "call_setup";
            notifyBluetooth("hfp_call_setup");
            break;
        case ESP_HF_CLIENT_CIND_CALL_EVT:
            if (param != nullptr) {
                if (param->call.status == ESP_HF_CALL_STATUS_CALL_IN_PROGRESS) {
                    call_state_ = "active";
                    call_setup_active_ = false;
                    next_audio_retry_ms_ = millis() + 80U;
                } else if (!call_setup_active_ &&
                           (call_state_ == "ending" || call_state_ == "active" || call_state_ == "dialing" ||
                            call_state_ == "alerting" || call_state_ == "ringing")) {
                    call_state_ = "idle";
                    next_audio_retry_ms_ = 0;
                }
            }
            last_hfp_event_ = "call_status";
            notifyBluetooth("hfp_call_status");
            break;
        case ESP_HF_CLIENT_CIND_CALL_HELD_EVT:
            if (param != nullptr) {
                call_state_ = callHeldStateToString(param->call_held.status);
            }
            last_hfp_event_ = "call_held";
            notifyBluetooth("hfp_call_held");
            break;
        case ESP_HF_CLIENT_CLCC_EVT:
            if (param != nullptr) {
                call_state_ = clccStateToString(param->clcc.status);
                if (param->clcc.number != nullptr && strlen(param->clcc.number) > 0) {
                    last_dialed_number_ = String(param->clcc.number);
                }
            }
            last_hfp_event_ = "clcc";
            notifyBluetooth("hfp_clcc");
            break;
        case ESP_HF_CLIENT_AT_RESPONSE_EVT:
            if (param != nullptr && param->at_response.code != ESP_HF_AT_RESPONSE_CODE_OK) {
                last_error_ = "hfp_at_response:" + String(atResponseCodeToString(param->at_response.code));
            }
            break;
        case ESP_HF_CLIENT_RING_IND_EVT:
            last_hfp_event_ = "ring";
            call_state_ = "ringing";
            notifyBluetooth("hfp_ring");
            next_audio_retry_ms_ = millis() + 80U;
            break;
        default:
            break;
    }
    pending_discoverable_policy_ = true;
    pending_status_publish_ = true;
}

void BluetoothManager::onHfpAudioStateChanged(bool connected) {
    if (audio_bridge_ == nullptr) {
        return;
    }
    if (connected) {
        if (callStateNeedsAudio(call_state_)) {
            audio_bridge_->stopDialTone();
        }
        if (!audio_bridge_->requestCapture(AudioEngine::CAPTURE_CLIENT_BLUETOOTH)) {
            last_error_ = "hfp_audio_capture_request_failed";
            notifyBluetooth("hfp_audio_capture_request_failed", last_error_.c_str());
        }
    } else {
        audio_bridge_->releaseCapture(AudioEngine::CAPTURE_CLIENT_BLUETOOTH);
    }
}

void BluetoothManager::requestAudioIfNeeded(const char* reason) {
    if (!connected_ || !slc_connected_ || !peer_addr_valid_ || hfp_audio_link_up_) {
        return;
    }
    const esp_err_t err = esp_hf_client_connect_audio(peer_addr_);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        last_error_ = "hfp_audio_connect_failed:" + errToString(err);
        notifyBluetooth("hfp_audio_connect_failed", last_error_.c_str());
        next_audio_retry_ms_ = millis() + 800U;
        return;
    }
    next_audio_retry_ms_ = millis() + 1200U;
    last_hfp_event_ = reason != nullptr ? String(reason) : String("audio_requested");
}

void BluetoothManager::onBleClientConnected(bool connected) {
    ble_client_connected_ = connected;
    last_ble_event_ = connected ? "client_connected" : "client_disconnected";
    notifyBluetooth(connected ? "ble_client_connected" : "ble_client_disconnected");
    pending_discoverable_policy_ = true;
    pending_status_publish_ = true;
}

void BluetoothManager::onGapAuthComplete(const uint8_t remote_bda[6], bool success) {
    if (!success || remote_bda == nullptr || !features_.has_hfp) {
        return;
    }

    memcpy(peer_addr_, remote_bda, sizeof(peer_addr_));
    peer_addr_valid_ = true;
    writeMacToString(peer_addr_, peer_mac_);
    hfp_requested_ = true;
    hfp_reconnect_attempts_ = 0;
    if (auto_reconnect_enabled_ && !connected_ && !hfp_connect_inflight_) {
        next_hfp_reconnect_ms_ = millis() + 700U;
    }
    last_hfp_event_ = "auth_ok_wait_connect";
    pending_status_publish_ = true;
}

void BluetoothManager::onGapAclDisconnected(const uint8_t remote_bda[6], uint16_t reason) {
    if (remote_bda == nullptr) {
        return;
    }

    if (peer_addr_valid_ && memcmp(remote_bda, peer_addr_, sizeof(peer_addr_)) != 0) {
        return;
    }

    connected_ = false;
    hfp_connect_inflight_ = false;
    hfp_active_ = false;
    hfp_audio_link_up_ = false;
    slc_connected_ = false;
    slc_wait_deadline_ms_ = 0;
    call_state_ = "idle";
    next_audio_retry_ms_ = 0;
    last_hfp_event_ = "acl_disconnected";
    last_error_ = "acl_disconnect_reason:" + String(reason);
    notifyBluetooth("hfp_acl_disconnected", last_error_.c_str());
    onHfpAudioStateChanged(false);

    if ((auto_reconnect_enabled_ || !pending_dial_number_.isEmpty()) && hfp_requested_ && peer_addr_valid_) {
        hfp_reconnect_attempts_++;
        const uint8_t exp = (hfp_reconnect_attempts_ < 5) ? hfp_reconnect_attempts_ : 5;
        next_hfp_reconnect_ms_ = millis() + 1500U * (1U << exp);
    }

    pending_discoverable_policy_ = true;
    pending_status_publish_ = true;
}
