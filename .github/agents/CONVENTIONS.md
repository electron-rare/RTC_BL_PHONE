# Référentiel de Conventions – RTC_BL_PHONE

## Objectif
Fournir un socle commun de conventions pour le code, la documentation, le hardware et les workflows du projet RTC_BL_PHONE, inspiré des meilleures pratiques issues des sources web (Espressif, PlatformIO, Silvertel, Asterisk, etc.).

## 1. Conventions de code (firmware)
- Utiliser PlatformIO pour la gestion des environnements et des dépendances.
- Respecter la structure : `src/` pour le code principal, `include/` pour les headers, `lib/` pour les librairies additionnelles.
- Préférer les noms explicites pour les GPIO et les états (ex : `pinHookSense`, `PhoneState::OFF_HOOK`).
- Documenter chaque fonction critique avec un commentaire Doxygen minimal.
- Utiliser le logging série pour tout comportement anormal ou critique.
- Ne jamais committer de binaires ou de logs générés.

## 2. Conventions hardware
- Documenter tout schéma de câblage dans `docs/solutions_rtc_phone_esp32.md`.
- Préciser les variantes supportées (ESP32-DevKitC, Audio Kit, SLIC, etc.).
- Toujours vérifier la cohérence entre le schéma et la configuration firmware.
- Ajouter des remarques de sécurité (alimentation, isolation, ESD) dans la doc.

## 3. Conventions documentation
- Garder la documentation concise, structurée et à jour.
- Vérifier la validité de tous les liens relatifs.
- Archiver ou supprimer les sections obsolètes.
- Référencer explicitement ce référentiel dans chaque agent et plan projet.

## 4. Conventions CI/Workflows
- Garder les workflows GitHub Actions simples et adaptés à PlatformIO.
- Vérifier la syntaxe YAML et la validité des chemins à chaque commit.
- Documenter toute évolution CI dans le README ou un changelog.

## 5. Références web
- Espressif DevKitC : https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/index.html
- PlatformIO : https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html
- Silvertel SLIC : https://www.silvertel.com/
- Asterisk PBX : https://docs.asterisk.org/

## 6. Mise à jour
Ce référentiel doit être mis à jour à chaque évolution majeure du projet ou des pratiques de la communauté.