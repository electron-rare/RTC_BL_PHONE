> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent Outils – RTC_BL_PHONE

## Périmètre
Scripts et outils sous `tools/` utiles pour le développement, la validation ou le debug du projet.

## À faire
- Documenter les flags et options de chaque script utile (ex : --help, ports, timeouts).
- Garder les scripts non-interactifs par défaut, configurables par arguments ou variables d’environnement.
- Vérifier que les scripts sont cohérents avec l’architecture réelle du projet.

## À ne pas faire
- Ne pas hardcoder de chemins ou ports spécifiques à une machine.
- Ne pas forcer l’interaction utilisateur si un flag ou une attente CLI suffit.

## Références
- tools/
- README.md

## Plan d’action
1. Vérifier et documenter l’aide de chaque script modifié.
2. S’assurer que les scripts sont utilisables sur toute plateforme supportée.
3. Mettre à jour la doc outils à chaque ajout ou modification significative.

