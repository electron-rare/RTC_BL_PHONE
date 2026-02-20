#include "props/EspNowBridge.h"

#include <WiFi.h>
#include <esp_now.h>

#include <algorithm>

EspNowBridge* EspNowBridge::instance_ = nullptr;

EspNowBridge::EspNowBridge() {
    instance_ = this;
}

bool EspNowBridge::begin(const EspNowPeerStore& initial_peers) {
    store_ = initial_peers;

    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        ready_ = false;
        return false;
    }

    esp_now_register_recv_cb(onDataRecv);
    esp_now_register_send_cb(onDataSent);

    ready_ = true;

    std::vector<String> peers_copy = store_.peers;
    store_.peers.clear();
    for (const String& mac : peers_copy) {
        addPeerInternal(mac, false);
    }
    return true;
}

void EspNowBridge::tick() {
    // ESP-NOW uses callbacks, no polling required.
}

bool EspNowBridge::addPeer(const String& mac) {
    return addPeerInternal(mac, true);
}

bool EspNowBridge::deletePeer(const String& mac) {
    return deletePeerInternal(mac, true);
}

const std::vector<String>& EspNowBridge::peers() const {
    return store_.peers;
}

bool EspNowBridge::sendJson(const String& target, const String& json_payload) {
    if (!ready_) {
        return false;
    }

    if (target == "broadcast" || target == "BROADCAST") {
        const uint8_t broadcast_mac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
        return sendToMac(broadcast_mac, json_payload);
    }

    uint8_t mac[6] = {0};
    if (!A252ConfigStore::parseMac(target, mac)) {
        return false;
    }
    return sendToMac(mac, json_payload);
}

bool EspNowBridge::isReady() const {
    return ready_;
}

void EspNowBridge::setCommandCallback(std::function<void(const String&, const JsonVariantConst&)> cb) {
    command_callback_ = std::move(cb);
}

void EspNowBridge::statusToJson(JsonObject obj) const {
    obj["ready"] = ready_;
    obj["peer_count"] = static_cast<uint32_t>(store_.peers.size());
    obj["tx_ok"] = tx_ok_;
    obj["tx_fail"] = tx_fail_;
    obj["rx_count"] = rx_count_;
    obj["last_rx_mac"] = last_rx_mac_;

    JsonArray peers = obj["peers"].to<JsonArray>();
    for (const String& peer : store_.peers) {
        peers.add(peer);
    }
}

bool EspNowBridge::addPeerInternal(const String& mac, bool persist) {
    if (!ready_) {
        return false;
    }

    const String normalized = A252ConfigStore::normalizeMac(mac);
    if (normalized.isEmpty()) {
        return false;
    }

    if (std::find(store_.peers.begin(), store_.peers.end(), normalized) != store_.peers.end()) {
        return true;
    }

    uint8_t peer_mac[6] = {0};
    if (!A252ConfigStore::parseMac(normalized, peer_mac)) {
        return false;
    }

    esp_now_peer_info_t peer_info = {};
    memcpy(peer_info.peer_addr, peer_mac, 6);
    peer_info.channel = 0;
    peer_info.encrypt = false;

    if (esp_now_add_peer(&peer_info) != ESP_OK) {
        return false;
    }

    store_.peers.push_back(normalized);
    if (persist) {
        A252ConfigStore::saveEspNowPeers(store_);
    }
    return true;
}

bool EspNowBridge::deletePeerInternal(const String& mac, bool persist) {
    if (!ready_) {
        return false;
    }

    const String normalized = A252ConfigStore::normalizeMac(mac);
    if (normalized.isEmpty()) {
        return false;
    }

    uint8_t peer_mac[6] = {0};
    if (!A252ConfigStore::parseMac(normalized, peer_mac)) {
        return false;
    }

    esp_now_del_peer(peer_mac);

    const auto it = std::remove(store_.peers.begin(), store_.peers.end(), normalized);
    const bool removed = it != store_.peers.end();
    store_.peers.erase(it, store_.peers.end());
    if (removed && persist) {
        A252ConfigStore::saveEspNowPeers(store_);
    }
    return removed;
}

bool EspNowBridge::sendToMac(const uint8_t mac[6], const String& payload) {
    if (!ready_) {
        return false;
    }

    const esp_err_t err = esp_now_send(mac, reinterpret_cast<const uint8_t*>(payload.c_str()), payload.length());
    if (err == ESP_OK) {
        tx_ok_++;
        return true;
    }
    tx_fail_++;
    return false;
}

void EspNowBridge::onDataRecv(const uint8_t* mac_addr, const uint8_t* data, int len) {
    if (!instance_) {
        return;
    }

    char mac_buf[18] = {0};
    snprintf(mac_buf,
             sizeof(mac_buf),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac_addr[0],
             mac_addr[1],
             mac_addr[2],
             mac_addr[3],
             mac_addr[4],
             mac_addr[5]);

    String payload;
    payload.reserve(len + 1);
    for (int i = 0; i < len; ++i) {
        payload += static_cast<char>(data[i]);
    }

    instance_->rx_count_++;
    instance_->last_rx_mac_ = mac_buf;
    instance_->last_rx_payload_ = payload;

    if (!instance_->command_callback_) {
        return;
    }

    JsonDocument doc;
    if (deserializeJson(doc, payload) != DeserializationError::Ok) {
        doc.clear();
        doc["raw"] = payload;
    }
    instance_->command_callback_(String(mac_buf), doc.as<JsonVariantConst>());
}

void EspNowBridge::onDataSent(const uint8_t* mac_addr, esp_now_send_status_t status) {
    if (!instance_) {
        return;
    }
    if (status == ESP_NOW_SEND_SUCCESS) {
        instance_->tx_ok_++;
    } else {
        instance_->tx_fail_++;
    }
}
