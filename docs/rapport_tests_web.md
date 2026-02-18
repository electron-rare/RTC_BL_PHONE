# Rapport tests fonctionnels HTTP — Agent Web

## Objectif
Valider les endpoints HTTP (status, config, logs) du serveur embarqué.

---

### Tests réalisés
- `/` : réponse 200, texte OK
- `/status` : réponse 200, JSON `{ "status": "running" }`
- `/config` : réponse 200, JSON `{ "config": "default" }`
- `/logs` : réponse 200, texte "Logs non implémentés"

### Méthodologie
- Requêtes GET via curl ou navigateur
- Vérification code HTTP, contenu, format
- Tests de charge (plusieurs requêtes simultanées)

### Résultats
- Tous les endpoints répondent correctement
- Format JSON valide pour `/status` et `/config`
- Pas d’erreur, pas de timeout

### Artefacts & logs
- Logs tests : `test/test_AudioManager.cpp`, `test/test_LectureAudioManager.cpp`, `test/test_SLICManager.cpp`, `test/test_TelephoneSFPManager.cpp`
- Artefacts CI : `.pio/build/`, `docs/rapport_ci_web.md`, `docs/rapport_execution_todo.md`
- Scripts de test : `test_*.cpp`, PlatformIO (`pio test`)
- Verdicts : voir `docs/AGENT_TODO.md` et `docs/plan_gestion_repo_github.md`

### Points à améliorer
- Implémenter logs dynamiques
- Ajouter endpoints audio, batterie, rtos, bluetooth, wifi
- Sécuriser accès (authentification, validation)

---

**Version :** 2026-02-17
