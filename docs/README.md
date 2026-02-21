# Documentation technique & intelligence agents — RTC_BL_PHONE

Ce dossier centralise toute l'intelligence documentaire et la synergie agents du projet :

## Fiches agents
| Agent                | Fiche                          |
|----------------------|-------------------------------|
| AudioManager         | fiche_AudioManager.md          |
| BluetoothManager     | fiche_BluetoothManager.md      |
| RTOSManager          | fiche_RTOSManager.md           |
| SLICManager          | fiche_agent_embarque_stack.md  |
| WifiManager          | roles_agents.md                |
| Web                  | plan_webui.md                  |
| Energie              | plan_stack_audio.md            |
| Lecture Audio        | fiche_agent_audio_tools.md     |
| Téléphone SFP        | plan_stack_telephone_sfp.md    |
| Repo & GitHub        | plan_gestion_repo_github.md    |
| Tests Hardware       | fiche_agents_test_hardware.md  |
| QA                   | procedure_tests_hardware.md    |

Chaque fiche détaille : rôle, API, notifications, points de validation, synergie avec les autres agents.

## Plans d'architecture et de test
| Plan                        | Fichier                       |
|-----------------------------|-------------------------------|
| Annuaire                    | plan_annuaire.md              |
| Chef projet ESP32S3         | plan_chef_projet_esp32s3_ag1171s.md |
| Délégation agents           | plan_delegation_agents.md     |
| Dev web                     | plan_dev_web.md               |
| Gestion repo GitHub         | plan_gestion_repo_github.md   |
| Multitâche RTOS             | plan_multitache_rtos.md       |
| RTOS                        | plan_rtos.md                  |
| Stack audio                 | plan_stack_audio.md           |
| Stack lecture audio         | plan_stack_lecture_audio.md   |
| Stack SLIC                  | plan_stack_slic.md            |
| Stack téléphone SFP         | plan_stack_telephone_sfp.md   |
| Tests livraison             | plan_tests_livraison.md       |
| WebUI                       | plan_webui.md                 |
| Spec WebUI route parity     | spec_webui_route_parity_and_coverage_v1.md |

## Procédures QA & hardware
| Procédure                   | Fichier                       |
|-----------------------------|-------------------------------|
| Tests hardware              | procedure_tests_hardware.md   |
| QA moniteur série           | protocole_test_qa_moniteur_serie.md |
| Robustesse RTOS             | audit_robustesse_rtos.md      |
| Sécurité web                | audit_securite_web.md         |

## Rapports
| Rapport                     | Fichier                       |
|-----------------------------|-------------------------------|
| CI Web                      | rapport_ci_web.md             |
| Exécution TODO              | rapport_execution_todo.md     |
| Validation hardware         | rapport_validation_hardware.md|
| Synthèse validation         | rapport_synthese_validation.md|
| Tests fonctionnels          | rapport_tests_fonctionnels.md |
| Tests web                   | rapport_tests_web.md          |
| HW JSON                     | rapport_hw.json               |

### Archives
Les rapports d’audit, de validation et de CI anciens sont archivés dans [docs/archives/](docs/archives).

## Changelog
Historique des évolutions : [CHANGELOG.md](CHANGELOG.md)

---

## Synergie agents
- Les agents collaborent pour garantir la robustesse, la qualité et la traçabilité du firmware et du hardware.
- Exemples :
	- Audio ↔ RTOS ↔ Web ↔ WiFi ↔ SLIC ↔ Téléphonie ↔ Energie ↔ Bluetooth
	- Repo & GitHub ↔ CI/CD ↔ Documentation

## Règles
- Toute évolution majeure doit être documentée ici
- Les fiches agents servent de référence pour la maintenance, la QA et la validation terrain
- L'index (ce fichier) doit pointer vers tous les documents clés

## Pas de prompts story
Ce projet n'utilise pas de prompts narratifs ou de story specs, mais une documentation technique et QA exhaustive.
