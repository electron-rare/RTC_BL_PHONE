> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Firmware Core – RTC_BL_PHONE

## Périmètre
Tout le code source firmware sous `src/` et la configuration PlatformIO (`platformio.ini`).

## À faire
- Compiler et flasher le firmware via PlatformIO (environnements : esp32dev, esp32-s3-devkitc-1, etc.).
- Documenter toute évolution majeure dans le README et docs/solutions_rtc_phone_esp32.md.
- Vérifier la configuration I2S, ES8388, SLIC, et la gestion des GPIO (hook, ring, line enable).
- S’assurer que la machine d’états (PhoneState) et la logique RTC sont testées à chaque commit.
- Ajouter des logs série pour tout bug ou comportement inattendu.

## À ne pas faire
- Ne pas committer de binaires, logs ou artefacts de build dans le repo.
- Ne pas référencer de scripts ou chemins absents du projet.

## Références
- README.md
- docs/solutions_rtc_phone_esp32.md
- src/main.cpp
- platformio.ini

## Plan d’action
1. Compiler le firmware pour chaque cible supportée.
2. Flasher et tester la logique RTC (hook, ring, line, audio) sur hardware réel.
3. Mettre à jour la documentation à chaque évolution significative.
