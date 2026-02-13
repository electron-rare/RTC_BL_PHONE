# RTC_BL_PHONE

Projet PlatformIO ESP32 pour recycler un téléphone RTC ancien (combiné, clavier, hook).

## Démarrage rapide
1. Ouvrir le dossier dans PlatformIO.
2. Compiler et flasher l'environnement `esp32dev`.
3. Ouvrir le moniteur série à 115200 bauds.

## Choix de cartes ESP32
Voir `docs/solutions_rtc_phone_esp32.md` pour la shortlist des DevKit utilisables (ESP32-DevKitC, ESP32-S3-DevKitC-1, NodeMCU-32S, LOLIN32) et les liens de référence web.

## Contenu
- `platformio.ini`: configuration initiale du projet.
- `src/main.cpp`: prototype minimal (détection décroché/raccroché).
- `docs/solutions_rtc_phone_esp32.md`: comparaison des meilleures architectures + sélection de DevKit ESP32 recommandés.
