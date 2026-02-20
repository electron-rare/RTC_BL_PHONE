> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Audio – RTC_BL_PHONE

## Périmètre
Gestion de l’audio (I2S, ES8388, SLIC K50835F, ADC/DAC internes) pour le téléphone RTC sur ESP32.

## À faire
- Documenter le schéma de câblage audio réel (voir docs/solutions_rtc_phone_esp32.md).
- Vérifier la configuration I2S (pins, codec ES8388, SLIC, ADC/DAC internes).
- S’assurer que les tests audio (loopback, capture, playback) sont reproductibles sur chaque hardware supporté.
- Mettre à jour la documentation audio à chaque évolution hardware/firmware.

## À ne pas faire
- Ne pas générer ou valider des assets audio binaires inutiles (pas de pipeline audio automatisé dans ce projet).
- Ne pas référencer d’ID ou de manifestes obsolètes (ex : zacus_v1_audio.yaml).

## Références
- docs/solutions_rtc_phone_esp32.md
- docs/fiche_agent_audio_tools.md
- Schémas de câblage dans docs/
- Exemples d’initialisation I2S dans le firmware (src/main.cpp)

## Plan d’action
1. Vérifier le câblage et la config I2S/codec à chaque changement hardware.
2. Tester la capture et la lecture audio sur chaque plateforme (ESP32-DevKitC, Audio Kit, etc.).
3. Mettre à jour la doc audio et signaler toute divergence dans le README ou docs/solutions_rtc_phone_esp32.md.

## Architecture audio RTC_BL_PHONE

### Abstraction AudioCodec
- Interface pour codecs audio (I2S/I2C), méthodes : init, setVolume, mute, setRoute.
- Implémentations : ES8388 (I2S+I2C), PCM5102 (I2S), GenericCodec (fallback/tests).
- Routage audio RTC/Bluetooth via setRoute.

### ES8388
- Initialisation I2S + I2C.
- Volume/mute/routage via registres.
- Points hardware : alimentation, ESD, mapping pins.

### PCM5102
- Initialisation I2S.
- Volume/mute via atténuation I2S ou pin externe.
- Routage externe.

### Tests
- MockCodec pour tests unitaires.
- test_audio_codec.cpp : vérification init, volume, mute, routage.

### Sécurité
- Mapping pins selon ESP32/ESP32-S3.
- Filtrage alimentation, protection ESD.

### Limitations et recommandations
- ES8388 : nécessite une initialisation I2C en plus d’I2S (utiliser une librairie dédiée, ex. ESP-ADF).
- La gestion du volume/routage dépend du hardware (ES8388, PCM5102, GenericCodec).
- Toujours tester le loopback audio sur chaque plateforme.
- Vérifier la protection ESD et l’alimentation.
- Documenter toute divergence hardware/firmware dans README et docs/solutions_rtc_phone_esp32.md.
- Logs série obligatoires pour tout bug ou anomalie.

