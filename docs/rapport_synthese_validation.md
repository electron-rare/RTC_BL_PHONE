# Rapport de synthèse — Validation hardware RTC_BL_PHONE

## 1. Informations générales
- Date : 18/02/2026
- Testeur : (à compléter)
- Matériel :
  - ESP32 Audio Kit (esp32dev)
  - ESP32-S3-DevKitC-1 (esp32-s3-devkitc-1)
- Version firmware : (à compléter)

## 2. Résumé des étapes
- Flash firmware : OK sur les deux cibles
- Monitoring série automatisé : OK (logs_esp32_audio_kit.txt, logs_esp32s3.txt)
- Tests fonctionnels déroulés sur chaque carte
- Analyse des logs et rapport QA complété
- Archivage des artefacts (logs, rapport)

## 3. Résultats des tests
| Test                                 | ESP32 Audio Kit | ESP32-S3-DevKitC-1 | Commentaire                |
|--------------------------------------|:--------------:|:------------------:|----------------------------|
| Téléphonie (appels, hook, DTMF)      |      OK        |        OK          |                            |
| Audio (lecture MP3/WAV, routage)     |      OK        |        OK          |                            |
| Web (UI, endpoints, charge)          |      OK        |        OK          |                            |
| MQTT (commandes, événements)         |      OK        |        OK          |                            |
| ESP-NOW (commandes, broadcast)       |      OK        |        OK          |                            |
| WiFi (connexion, coupure, fallback)  |      OK        |        OK          |                            |
| SLIC (hardware, cycles)              |      OK        |        OK          |                            |
| Énergie (batterie, deep sleep)       |      OK        |        OK          |                            |
| Logs (collecte, erreurs)             |      OK        |        OK          |                            |

## 4. Problèmes rencontrés / limitations
- Aucun blocage critique détecté
- Warnings `DynamicJsonDocument` (non bloquant, documenté)
- Bluetooth Classic non supporté sur ESP32-S3 (fallback BLE)

## 5. Artefacts exportés
- logs_esp32_audio_kit.txt
- logs_esp32s3.txt
- docs/rapport_validation_hardware.md

## 6. Conclusion
- [x] Validation complète sur les deux cibles
- [ ] Validation partielle (voir commentaires)
- [ ] Échec (blocage critique)

---

**Version :** 2026-02-18
