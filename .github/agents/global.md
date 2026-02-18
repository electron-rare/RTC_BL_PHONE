> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Global – RTC_BL_PHONE

## Périmètre
Gestion globale du projet : cohérence documentation, firmware, hardware, outils, CI et sécurité.

## À faire
- Garder le dépôt toujours buildable et cohérent (PlatformIO, docs, schémas, scripts).
- Vérifier la cohérence entre la documentation, le firmware (src/), le hardware (docs/), et les outils.
- S’assurer que chaque commit majeur est documenté (README, docs/solutions_rtc_phone_esp32.md).
- Exécuter la matrice PlatformIO sur toutes les cibles supportées avant toute release.

## À ne pas faire
- Ne pas utiliser de commandes destructives ou toucher aux licences sans validation explicite.
- Ne pas laisser de divergences entre la doc, le code et le hardware.

## Références
- README.md
- docs/solutions_rtc_phone_esp32.md
- platformio.ini


## Délégation agents et synergie

### Rôles principaux
- Agent Firmware : modularisation, tests, intégration hardware/audio, CI.
- Agent Hardware : câblage, sécurité, compatibilité ESP32/ESP32-S3.
- Agent Audio : abstraction AudioCodec, routage, documentation audio.
- Agent Documentation : rédaction, mise à jour, cohérence documentation.
- Agent CI/QA : validation builds, tests automatisés, traçabilité.
- Agent Global/Conventions : audit, amélioration, cohérence.

### Synergie
- Les agents travaillent en synergie pour garantir la qualité, la sécurité et la maintenabilité.
- Chaque agent rend compte dans son fichier dédié, propose des améliorations et signale toute anomalie.
