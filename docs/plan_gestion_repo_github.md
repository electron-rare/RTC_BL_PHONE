# Plan de gestion Repo & GitHub

## Objectif
Structurer la gestion du dépôt RTC_BL_PHONE pour garantir la qualité, la traçabilité, l’automatisation CI/CD, et la documentation.

---

### 1. Branches
- main : stable, releases
- dev : développement
- feature/* : nouvelles fonctionnalités
- fix/* : corrections

### 2. Commits & PR
- Commits atomiques, messages clairs
- PR pour chaque feature/fix, validation croisée
- Templates PR (description, tests, impact)

### 3. Issues
- Création d’issues pour bugs, features, documentation
- Templates issues (type, description, reproduction, logs)

### 4. Releases & Tags
- Releases pour chaque version stable
- Tags versionnés (v1.0.0, v1.1.0...)
- Changelog détaillé

### 5. CI/CD
- Automatisation via GitHub Actions
- Build PlatformIO, tests unitaires, audit code
- Déploiement releases

### 6. Documentation
- README.md structuré (usage, architecture, agents, stacks)
- docs/fiche_agent_embarque_stack.md, docs/CHANGELOG.md
- Documentation auto-générée (doxygen, markdown)

### 7. Traçabilité & Qualité
- Historique complet (commits, PR, issues)
- Validation code (lint, tests, audit)
- Publication releases, changelog

---

_Agent Repo & GitHub – Plan généré automatiquement._
