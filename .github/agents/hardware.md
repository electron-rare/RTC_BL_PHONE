> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Hardware – RTC_BL_PHONE

## Périmètre
Documentation matérielle : câblage ESP32, SLIC K50835F, ES8388, schémas, choix de DevKit, sécurité et tests hardware.

## À faire
- Documenter précisément le schéma de câblage et les variantes supportées (voir docs/solutions_rtc_phone_esp32.md).
- Vérifier la cohérence entre le schéma hardware et la configuration firmware (pins, I2S, ADC/DAC, GPIO).
- Mettre à jour la documentation à chaque évolution matérielle ou changement de routage.
- Tester le hardware (hook, ring, audio, sécurité) sur chaque plateforme supportée.

## À ne pas faire
- Ne pas committer de schémas ou photos non validés.
- Ne pas référencer de scripts ou chemins absents du projet.

## Références
- docs/solutions_rtc_phone_esp32.md
- README.md

## Plan d’action
1. Mettre à jour le schéma de câblage à chaque modification hardware.
2. Vérifier la correspondance entre hardware et firmware (pins, signaux, sécurité).
3. Documenter les tests et les problèmes rencontrés dans la doc projet.

