sequenceDiagram
    participant ESP32 as ESP32 SFP
    participant Smartphone as Smartphone
    ESP32->>Smartphone: Scan BLE
    Smartphone->>ESP32: Répond (advertising)
    ESP32->>Smartphone: Appairage (demande)
    Smartphone->>ESP32: Appairage (acceptation)
    ESP32->>Smartphone: Demande contacts (service BLE)
    Smartphone->>ESP32: Envoi contacts (JSON)
    ESP32->>ESP32: Validation et intégration
    ESP32->>Smartphone: Accusé réception
    Smartphone->>Smartphone: Logs synchronisation
