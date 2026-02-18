
# Index des agents – RTC_BL_PHONE

Chaque fichier `.github/agents/*.md` est une fiche de gouvernance ou de workflow : se référer à la section `## Plan d’action` (voir `PLAN_TEMPLATE.md`) et au référentiel de conventions (`CONVENTIONS.md`).

## Catégories principales


### 1. Gouvernance et pilotage
| Fichier | Rôle |
|---|---|
| `global.md` | Cohérence projet, build, doc, sécurité, release, délégation agents |
| `ci.md` | Workflows CI, validation PlatformIO, reporting |
| `PLAN_TEMPLATE.md` | Modèle de plan d’action pour tous les agents |
| `CONVENTIONS.md` | Référentiel de conventions (code, doc, hardware, CI) |

### Délégation agents
- Agent Firmware : modularisation, tests, intégration hardware/audio, CI.
- Agent Hardware : câblage, sécurité, compatibilité ESP32/ESP32-S3.
- Agent Audio : abstraction AudioCodec, routage, documentation audio.
- Agent Documentation : rédaction, mise à jour, cohérence documentation.
- Agent CI/QA : validation builds, tests automatisés, traçabilité.
- Agent Global/Conventions : audit, amélioration, cohérence.

Les agents travaillent en synergie pour garantir la qualité, la sécurité et la maintenabilité.

### 2. Firmware et tests
| Fichier | Rôle |
|---|---|
| `firmware_core.md` | Logique principale, build PlatformIO, doc firmware |
| `firmware_tests.md` | Tests unitaires/fonctionnels, vérification GPIO/audio |
| `firmware_tooling.md` | Outils de build/test, scripts CLI |
| `firmware_copilot.md` | Tâches spécifiques Copilot (I2S, artefacts, logs) |
| `firmware_docs.md` | Documentation firmware, onboarding |

### 3. Hardware et audio
| Fichier | Rôle |
|---|---|
| `hardware.md` | Schémas, câblage, sécurité, variantes matérielles |
| `audio.md` | Routage audio, I2S, ES8388, SLIC, tests audio |

### Audio
- Abstraction AudioCodec (I2S/I2C, ES8388, PCM5102, GenericCodec)
- Routage RTC/Bluetooth via setRoute
- Tests unitaires : mock, test_audio_codec.cpp
- Sécurité : mapping pins, alimentation, ESD
### 4. Documentation et outils
| Fichier | Rôle |
|---|---|
| `docs.md` | Documentation utilisateur/technique, structure, liens |
| `tools.md` | Scripts, helpers, conventions CLI |

### 5. Spécifiques projet ou phase
| Fichier | Rôle |
|---|---|
| `ALIGNMENT_COMPLETE.md` | Checklist de pré-lancement, conformité agents |
| `PHASE_LAUNCH_PLAN.md` | Plan de lancement de phase, gates, artefacts |

> Pour chaque fiche, consulter la section “Références” et se conformer à `CONVENTIONS.md`.
