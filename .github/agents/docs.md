> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Documentation – RTC_BL_PHONE

## Périmètre
Documentation utilisateur et technique sous `docs/` : schémas, guides de câblage, choix hardware, structure firmware, sécurité, etc.

## À faire

## À ne pas faire

## Références

## Délégation agents RTC_BL_PHONE

### Agent Firmware
- Modularisation, code source, tests unitaires, intégration hardware/audio, maintenance, CI.
- Surveille PR, valide builds, garantit cohérence des classes.

### Agent Hardware
- Documente câblage, variantes matérielles, sécurité, compatibilité ESP32/ESP32-S3.
- Valide schémas, teste interfaces, signale risques.

### Agent Audio
- Gère abstraction AudioCodec, intégration I2S, configuration ES8388/PCM5102, documentation audio.
- Teste codecs, valide routage RTC/Bluetooth, maintient doc audio.md.

### Agent Documentation
- Rédige, met à jour et garantit cohérence des docs.md, README.md, solutions_rtc_phone_esp32.md.
- Garantit clarté, validité des liens, actualisation.

### Agent CI/QA
- Gère CI PlatformIO, validation builds, tests automatisés, traçabilité des livrables.
- Surveille workflows, signale erreurs, archive rapports de test.

### Agent Global/Conventions
- Veille au respect des conventions, arborescence, sécurité, documentation.
- Audite le projet, propose améliorations, garantit cohérence globale.

Chaque agent rend compte dans son fichier dédié, propose des améliorations et signale toute anomalie.

## Documentation technique : AudioCodec, ES8388, PCM5102

### Abstraction AudioCodec
- Interface pour codecs audio (I2S/I2C), méthodes : init, setVolume, mute, setRoute.
- Extensible : chaque codec = classe dérivée.
- Testabilité : mock intégré, test unitaire via test_audio_codec.cpp.

### ES8388
- Initialisation I2S + I2C.
- Volume/mute/routage via registres (ex : 0x2B, 0x2C, 0x2F, 0x30).
- Routage RTC/Bluetooth configurable.
- Points hardware : alimentation stable, protection ESD, mapping pins.

### PCM5102
- Initialisation I2S.
- Volume/mute via atténuation I2S ou pin externe.
- Routage externe (multiplexeur/relais).
- Points hardware : niveau logique, alimentation, protection.

### Routage audio
- Méthode setRoute : bascule RTC/Bluetooth.
- Prévoir isolation, multiplexage ou relais.

### Testabilité
- MockCodec pour tests unitaires.
- test_audio_codec.cpp : vérification init, volume, mute, routage.

### Sécurité et bonnes pratiques
- Vérifier mapping pins selon ESP32/ESP32-S3.
- Filtrage alimentation, protection ESD.
- Logs détaillés, gestion erreurs.

