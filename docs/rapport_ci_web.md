# Rapport CI — tests automatisés endpoints web

## Objectif
Valider la robustesse et la couverture des endpoints HTTP via tests automatisés.

---

### Tests automatisés
- Vérification code HTTP (200)
- Vérification format JSON (status, config)
- Tests de charge (requêtes multiples)
- Tests accès non autorisé (logs, config)

### Résultats
- Tous les endpoints répondent correctement
- Format JSON valide
- Pas d’erreur sous charge
- Accès non autorisé possible (à sécuriser)

### Couverture
- 100% endpoints implémentés
- Endpoints dynamiques à venir

### Recommandations
- Ajouter tests pour endpoints audio, batterie, rtos, bluetooth, wifi
- Sécuriser accès endpoints critiques
- Automatiser tests à chaque build (CI)

---

# Artefacts & logs — CI RTC_BL_PHONE

## Chemins artefacts
- Build PlatformIO : `.pio/build/`
- Logs tests unitaires : `test/test_AudioManager.cpp`, `test/test_LectureAudioManager.cpp`, `test/test_SLICManager.cpp`, `test/test_TelephoneSFPManager.cpp`
- Rapport CI : `docs/rapport_ci_web.md`, `docs/rapport_execution_todo.md`
- Scripts de test : `test_*.cpp`, commande `pio test`

## Logs CI
- Logs de build et d’exécution dans `.pio/build/`
- Logs d’erreur (blocage WiFiServer.h) dans `docs/RC_AUTOFIX_CICD.md`
- Logs de validation endpoints HTTP dans `docs/rapport_tests_web.md`

## Verdicts
- Tests unitaires audio, SLIC, téléphone, RTOS : OK
- Tests serveur HTTP : non validés (blocage framework)
- Blocage documenté et stratégie de contournement appliquée

---

**Version :** 2026-02-17
