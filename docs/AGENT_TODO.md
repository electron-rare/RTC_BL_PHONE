# Gates & objectifs — Phase suivante RTC_BL_PHONE

## [2026-02-20] Alignement multi-repos (RTC + Zacus + Kill_LIFE)
- Compatibilité ESP-NOW renforcée côté RTC:
  - commandes runtime `ESPNOW_ON` / `ESPNOW_OFF`
  - extraction de commande bridge depuis payload JSON imbriqué (`event/message/payload`)
- Correctif mesures ESP-NOW:
  - suppression du double comptage `tx_ok` (ack + callback)
- Base méthodologique importée depuis Kill_LIFE:
  - gates minimales Spec/Build/Test/Release
  - evidence pack et standards firmware de référence
  - documentée dans `docs/CROSS_REPO_INTELLIGENCE.md`

## Gates prioritaires
- Extension endpoints HTTP : audio, batterie, rtos, bluetooth, wifi
- Sécurisation endpoints : authentification, validation, gestion des droits
- Tests fonctionnels avancés : charge, robustesse, scénarios multi-utilisateurs
- Documentation endpoints et API : structure, exemples, onboarding
- CI : validation automatisée, reporting, traçabilité

## Objectifs
- Couverture complète des modules audio, SLIC, téléphone, RTOS, Bluetooth, Wifi
- Robustesse et sécurité des interfaces web
- Traçabilité des artefacts, logs et verdicts
- Onboarding et documentation utilisateur/technique

## Actions à lancer
- Développement endpoints manquants
- Implémentation sécurité et validation
- Création/extension des tests fonctionnels
- Mise à jour documentation et scripts CI

---

**Version :** 2026-02-17
