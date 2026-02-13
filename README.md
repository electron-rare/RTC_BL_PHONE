# RTC_BL_PHONE

Projet PlatformIO ESP32 pour recycler un téléphone RTC ancien (combiné, clavier, hook).

## Démarrage rapide
1. Ouvrir le dossier dans PlatformIO.
2. Compiler et flasher l'environnement `esp32-s3-devkitc-1` (par défaut).
3. Ouvrir le moniteur série à 115200 bauds.
4. Utiliser les commandes série (`h`, `r`, `o`, `d`, `c`) pour piloter la machine d'états.

## Choix de cartes ESP32
Voir `docs/solutions_rtc_phone_esp32.md` pour la shortlist des DevKit utilisables (ESP32-DevKitC, ESP32-S3-DevKitC-1, NodeMCU-32S, LOLIN32), les liens de référence web, et les solutions d’interface (direct combiné/clavier, SLIC/FXS, ATA externe), dont une variante AG1171S (Silvertel).

## Plan projet (chef de projet)
Voir `docs/plan_chef_projet_esp32s3_ag1171s.md` pour le planning en phases, les risques, les critères d'acceptation et les livrables de la version ESP32-S3 + AG1171S.

## Contenu
- `platformio.ini`: configuration multi-env (ESP32-S3 par défaut + ESP32 legacy).
- `src/main.cpp`: squelette firmware machine d'états pour intégration AG1171S.
- `docs/solutions_rtc_phone_esp32.md`: comparaison des architectures et recommandations.
- `docs/plan_chef_projet_esp32s3_ag1171s.md`: plan d'exécution projet version V0.1.
