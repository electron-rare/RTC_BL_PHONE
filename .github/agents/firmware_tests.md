> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Firmware Tests – RTC_BL_PHONE

## Périmètre
Tests unitaires et fonctionnels du firmware (src/), vérification des GPIO, de la logique RTC, et de l’audio (I2S, ES8388, SLIC).

## À faire
- Écrire et exécuter des tests unitaires pour la machine d’états (PhoneState), la gestion du hook, ring, line enable.
- Vérifier le comportement des GPIO sur hardware réel (décroché, sonnerie, activation ligne).
- Tester la capture et la lecture audio (I2S/codec/ADC/DAC) sur chaque plateforme supportée.
- Documenter les résultats de test dans README.md ou docs/solutions_rtc_phone_esp32.md.

## À ne pas faire
- Ne pas référencer de scripts ou chemins absents du projet.
- Ne pas committer d’artefacts de test ou de logs binaires.

## Références
- src/main.cpp
- README.md
- docs/solutions_rtc_phone_esp32.md

## Plan d’action
1. Écrire des tests unitaires pour chaque composant critique (états, GPIO, audio).
2. Exécuter les tests sur hardware réel à chaque évolution majeure.
3. Reporter les résultats et bugs dans la documentation projet.
