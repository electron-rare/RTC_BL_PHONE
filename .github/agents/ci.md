> **Référence : voir aussi `.github/agents/CONVENTIONS.md` pour les conventions à respecter.**

# Agent CI – RTC_BL_PHONE

## Périmètre
Workflows GitHub Actions sous `.github/workflows/` (firmware-ci.yml, firmware-story-v2.yml).

## À faire
- Garder les workflows simples, explicites et adaptés à PlatformIO.
- Vérifier la syntaxe YAML et la validité des chemins avant chaque commit.
- Documenter toute modification de workflow dans le README ou un changelog.

## À ne pas faire
- Ne pas supprimer ou renommer massivement des workflows sans justification.
- Ne pas modifier les licences ou les checks critiques sans validation.

## Références
- .github/workflows/firmware-ci.yml
- .github/workflows/firmware-story-v2.yml
- README.md

## Plan d’action
1. Vérifier la syntaxe et les chemins de chaque workflow modifié.
2. S’assurer que les builds PlatformIO passent sur toutes les cibles supportées.
3. Documenter toute évolution CI dans le projet.

