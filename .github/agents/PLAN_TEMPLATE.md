
# Modèle de plan d’action – RTC_BL_PHONE

Chaque fiche `.github/agents/<agent>.md` doit inclure une section `## Plan d’action` structurée ainsi :

1. **Contexte et validation**
   - Expliquer le contexte de l’action (matériel, firmware, doc, CI, etc.).
   - run: <commande de vérification initiale, ex : `git status -sb`>

2. **Étapes principales**
   - Détailler chaque étape avec une phrase claire.
   - run: <commande 1>
   - run: <commande 2>

3. **Reporting et documentation**
   - Préciser où consigner les résultats (README, docs, changelog, etc.).
   - run: <commande pour générer ou archiver les artefacts, ex : `python3 tools/dev/gen_cockpit_docs.py`>

> **Référence** : Ce plan doit respecter le référentiel de conventions du projet (voir `CONVENTIONS.md`).
