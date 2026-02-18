# Audit sécurité endpoints web — Agent Web

## Objectif
Analyser les risques et proposer des mesures pour sécuriser les endpoints HTTP.

---

### Points vérifiés
- Validation des entrées (pas de POST, GET simple)
- Pas de buffer overflow possible (ESPAsyncWebServer gère la taille)
- Pas d’injection (pas de paramètre dynamique pour l’instant)
- Pas d’authentification (à ajouter si besoin)
- Pas de données sensibles exposées
- Logs non dynamiques (pas de fuite)

### Recommandations
- Ajouter validation stricte des paramètres pour endpoints dynamiques
- Implémenter authentification (token, basic auth) pour `/config`, `/logs`
- Limiter accès à `/logs` (IP, token)
- Ajouter rate limiting (anti-DOS)
- Journaliser les accès critiques

### Priorité
- Authentification endpoints critiques
- Validation entrées POST/GET
- Rate limiting

---

**Version :** 2026-02-17
