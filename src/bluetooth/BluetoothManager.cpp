#include "bluetooth/BluetoothManager.h"

#include "core/AgentSupervisor.h"

#include <BLE2902.h>
#include <BLEAdvertising.h>
#include <BLECharacteristic.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <esp32-hal-bt.h>
#include <esp_bt.h>
#include <esp_bt_device.h>
#include <esp_bt_main.h>
#include <esp_gap_bt_api.h>
#include <esp_hf_client_api.h>

#include <cctype>
#include <cstring>

namespace {

constexpr char kBtDeviceName[] = "RTC_BL_PHONE_A252";
constexpr char kLegacyPinCode[] = "1234";

constexpr char kBleServiceUuid[] = "8fce0001-93ea-4f8f-8bde-4e8f0ea20001";
constexpr char kBleCmdUuid[] = "8fce0002-93ea-4f8f-8bde-4e8f0ea20002";
constexpr char kBleStatusUuid[] = "8fce0003-93ea-4f8f-8bde-4e8f0ea20003";

BluetoothManager* g_manager = nullptr;
BLEServer* g_ble_server = nullptr;
BLECharacteristic* g_ble_cmd_char = nullptr;
BLECharacteristic* g_ble_status_char = nullptr;
bool g_gap_callback_registered = false;
bool g_hfp_callback_registered = false;

void notifyBluetooth(const std::string& state, const std::string& error = "") {
    AgentStatus status{state, error, millis()};
    AgentSupervisor::instance().notify("bluetooth", status);
}

String errToString(esp_err_t err) {
    char buf[16];
    snprintf(buf, sizeof(buf), "0x%04x", static_cast<unsigned>(err));
    return String(buf);
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

    switch (event) {
        case ESP_BT_GAP_AUTH_CMPL_EVT: {
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                notifyBluetooth("gap_auth_ok");
            } else {
                notifyBluetooth("gap_auth_fail");
            }
            break;
        }
        case ESP_BT_GAP_PIN_REQ_EVT: {
            esp_bt_pin_code_t pin_code;
            memset(pin_code, 0, sizeof(pin_code));
            memcpy(pin_code, kLegacyPinCode, strlen(kLegacyPinCode));
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            notifyBluetooth("gap_pin_reply");
            break;
        }
        case ESP_BT_GAP_CFM_REQ_EVT: {
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            notifyBluetooth("gap_ssp_confirm");
            break;
        }
        case ESP_BT_GAP_KEY_REQ_EVT: {
            esp_bt_gap_ssp_passkey_reply(param->key_req.bda, true, 1234);
            notifyBluetooth("gap_ssp_passkey");
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
      ble_stack_initialized_(false),
      ble_service_ready_(false),
      ble_client_connected_(false),
      connected_(false),
      hfp_active_(false),
      slc_connected_(false),
      ble_active_(false),
      discoverable_(false),
      security_enabled_(false),
      pbap_supported_(false),
      pbap_synced_(false),
      peer_mac_(""),
      peer_addr_{0},
      peer_addr_valid_(false),
      call_state_("idle"),
      last_dialed_number_(""),
      pbap_last_error_("pbap_not_available_on_esp32_arduino_bluedroid"),
      last_hfp_event_("idle"),
      last_ble_event_("idle"),
      last_error_(""),
      ble_last_command_(""),
      ble_last_response_("") {}

bool BluetoothManager::begin(BoardProfile profile) {
    features_ = getFeatureMatrix(profile);
    connected_ = false;
    hfp_active_ = false;
    slc_connected_ = false;
    ble_active_ = false;
    discoverable_ = false;
    pbap_synced_ = false;
    ble_client_connected_ = false;
    peer_mac_ = "";
    peer_addr_valid_ = false;
    call_state_ = "idle";
    last_dialed_number_ = "";
    memset(peer_addr_, 0, sizeof(peer_addr_));
    last_error_ = "";
    last_hfp_event_ = "initialized";
    last_ble_event_ = "initialized";
    ble_last_command_ = "";
    ble_last_response_ = "";
    g_manager = this;

    if (features_.has_hfp || features_.has_ble_control) {
        stack_ready_ = ensureBtStackReady();
    } else {
        stack_ready_ = true;
    }
    notifyBluetooth("initialized");
    return stack_ready_;
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
    peer_mac_ = formatMac(peer_addr_);

    const esp_err_t err = esp_hf_client_connect(peer_addr_);
    if (err != ESP_OK) {
        last_error_ = "hfp_connect_req_failed:" + errToString(err);
        notifyBluetooth("connect_failed", last_error_.c_str());
        return false;
    }

    hfp_requested_ = true;
    last_hfp_event_ = "connect_requested";
    notifyBluetooth("hfp_connecting");
    return true;
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
    hfp_requested_ = false;
    last_hfp_event_ = "disconnect_requested";
    notifyBluetooth(ok ? "disconnected" : "disconnect_failed", ok ? "" : last_error_.c_str());
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
    last_hfp_event_ = "audio_stop_requested";
    notifyBluetooth("hfp_stopped");
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
    obj["slc_connected"] = slc_connected_;
    obj["hfp_requested"] = hfp_requested_;
    obj["call_state"] = call_state_;
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
    const esp_bt_discovery_mode_t mode =
        enabled ? ESP_BT_GENERAL_DISCOVERABLE : ESP_BT_NON_DISCOVERABLE;
    const esp_err_t err = esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, mode);
    if (err != ESP_OK) {
        last_error_ = "bt_discoverable_failed:" + errToString(err);
        notifyBluetooth("discoverable_failed", last_error_.c_str());
        return false;
    }
    discoverable_ = enabled;
    last_error_ = "";
    notifyBluetooth(enabled ? "discoverable_on" : "discoverable_off");
    publishBleStatus();
    return true;
}

bool BluetoothManager::dial(const String& number) {
    if (!requireCallControlReady("dial")) {
        return false;
    }

    String dialed = number;
    dialed.trim();
    if (!isValidDialNumber(dialed)) {
        last_error_ = "dial_invalid_number";
        notifyBluetooth("hfp_dial_failed", last_error_.c_str());
        return false;
    }

    const esp_err_t err = esp_hf_client_dial(dialed.c_str());
    if (err != ESP_OK) {
        last_error_ = "hfp_dial_failed:" + errToString(err);
        notifyBluetooth("hfp_dial_failed", last_error_.c_str());
        return false;
    }

    last_dialed_number_ = dialed;
    call_state_ = "dialing";
    last_hfp_event_ = "dial_requested";
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
    // Keep classic BT connectable for outbound HFP while avoiding random inbound ACL grabs.
    const esp_err_t scan_err =
        esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_NON_DISCOVERABLE);
    if (scan_err != ESP_OK) {
        last_error_ = "gap_scan_mode_failed:" + errToString(scan_err);
        notifyBluetooth("stack_failed", last_error_.c_str());
        return false;
    }
    discoverable_ = false;

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

    hfp_initialized_ = true;
    notifyBluetooth("hfp_ready");
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

void BluetoothManager::handleHfpEvent(int event, const void* raw_param) {
    const auto* param = static_cast<const esp_hf_client_cb_param_t*>(raw_param);
    switch (static_cast<esp_hf_client_cb_event_t>(event)) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT: {
            if (param != nullptr) {
                peer_mac_ = formatMac(param->conn_stat.remote_bda);
                memcpy(peer_addr_, param->conn_stat.remote_bda, sizeof(peer_addr_));
                peer_addr_valid_ = true;
                switch (param->conn_stat.state) {
                    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED:
                        connected_ = false;
                        hfp_active_ = false;
                        slc_connected_ = false;
                        call_state_ = "idle";
                        last_hfp_event_ = "disconnected";
                        notifyBluetooth("hfp_disconnected");
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTING:
                        connected_ = false;
                        hfp_active_ = false;
                        slc_connected_ = false;
                        last_hfp_event_ = "connecting";
                        notifyBluetooth("hfp_connecting");
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED:
                        connected_ = true;
                        hfp_active_ = false;
                        slc_connected_ = false;
                        last_hfp_event_ = "rfcomm_connected";
                        notifyBluetooth("hfp_rfcomm_connected");
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_SLC_CONNECTED:
                        connected_ = true;
                        hfp_active_ = true;
                        slc_connected_ = true;
                        last_hfp_event_ = "slc_connected";
                        notifyBluetooth("hfp_slc_connected");
                        if (hfp_requested_ && peer_addr_valid_) {
                            esp_hf_client_connect_audio(peer_addr_);
                        }
                        break;
                    case ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTING:
                        slc_connected_ = false;
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
                peer_mac_ = formatMac(param->audio_stat.remote_bda);
                switch (param->audio_stat.state) {
                    case ESP_HF_CLIENT_AUDIO_STATE_DISCONNECTED:
                        hfp_active_ = connected_;
                        last_hfp_event_ = "audio_disconnected";
                        notifyBluetooth("hfp_audio_disconnected");
                        break;
                    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTING:
                        last_hfp_event_ = "audio_connecting";
                        notifyBluetooth("hfp_audio_connecting");
                        break;
                    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED:
                    case ESP_HF_CLIENT_AUDIO_STATE_CONNECTED_MSBC:
                        hfp_active_ = true;
                        last_hfp_event_ = "audio_connected";
                        notifyBluetooth("hfp_audio_connected");
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
            }
            last_hfp_event_ = "call_setup";
            notifyBluetooth("hfp_call_setup");
            break;
        case ESP_HF_CLIENT_CIND_CALL_EVT:
            if (param != nullptr) {
                if (param->call.status == ESP_HF_CALL_STATUS_CALL_IN_PROGRESS) {
                    call_state_ = "active";
                } else if (call_state_ == "ending" || call_state_ == "active" || call_state_ == "dialing" ||
                           call_state_ == "alerting" || call_state_ == "ringing") {
                    call_state_ = "idle";
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
            break;
        default:
            break;
    }
    publishBleStatus();
}

void BluetoothManager::onBleClientConnected(bool connected) {
    ble_client_connected_ = connected;
    last_ble_event_ = connected ? "client_connected" : "client_disconnected";
    notifyBluetooth(connected ? "ble_client_connected" : "ble_client_disconnected");
    publishBleStatus();
}
