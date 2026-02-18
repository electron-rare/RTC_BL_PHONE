# Plan de développement web — Agent Web

## Objectif
Structurer l’implémentation, les tests, l’audit et la CI du serveur HTTP embarqué.

---

### 1. Endpoints à implémenter
- `/` : page d’accueil (texte)
- `/status` : état du système (JSON)
- `/config` : configuration (JSON)
- `/logs` : logs système (texte)
- `/audio` : état audio (JSON)
- `/battery` : état batterie (JSON)
- `/rtos` : état multitâche (JSON)
- `/bluetooth` : état Bluetooth (JSON)
- `/wifi` : état WiFi (JSON)

### 2. Sécurité
- Validation des entrées (GET/POST)
- Protection contre injection, buffer overflow
- Authentification simple (optionnelle)

### 3. Tests fonctionnels
- Vérification réponse HTTP (code, contenu)
- Tests endpoints critiques (status, logs, config)
- Tests de charge (plusieurs requêtes)

### 4. CI
- Tests automatisés endpoints
- Rapport de couverture
- Logs CI

### 5. Documentation
- Décrire chaque endpoint, usage, format
- Exemple de requête/réponse

---

**Version :** 2026-02-17
