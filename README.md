# RTC_BL_PHONE

Téléphone RTC expérimental sur ESP32 A252, orienté terrain: numérotation à impulsion, hotline audio, bridge ESP-NOW, validation série stricte.

## Esprit Zacus

Ce repo se pilote comme une session de terrain:

- `Opérateur` : décroche, compose, déclenche les scénarios.
- `Analyste` : surveille `STATUS` / `HOTLINE_STATUS` / `ESPNOW_STATUS`.
- `Archiviste` : garde les logs et rapports dans `artifacts/`.

Référence rôles: [docs/roles_agents.md](docs/roles_agents.md)

## Cible active

- Board focus: `ESP32_A252`
- Port série usuel: `/dev/cu.usbserial-0001`
- Contrat firmware courant: `A252_AUDIO_CHAIN_V4`

## Comportement hotline (actuel)

- Preset forcé au boot:
  - `1 -> /welcome.wav`
  - `2 -> /souffle.wav`
  - `3 -> /radio.wav`
- Numérotation impulsion active combiné décroché.
- Après sélection d’un numéro valide:
  - lecture WAV,
  - pause 3s,
  - boucle jusqu’au raccroché.
- Raccroché détecté rapidement (~300 ms).
- Pas de sonnerie automatique au boot (ring déclenché par événement runtime uniquement).

## ESP-NOW (actuel)

- Identité device persistante: `HOTLINE_PHONE`
- Commandes dédiées:
  - `ESPNOW_DEVICE_NAME_GET`
  - `ESPNOW_DEVICE_NAME_SET <NAME>`
- Runtime auto-discovery peers:
  - broadcast `ESPNOW_DEVICE_NAME_GET` toutes les 60s,
  - auto-ajout des MAC qui répondent,
  - télémétrie visible dans `STATUS.espnow.peer_discovery_*`.

## Démarrage rapide A252

1. Build:

```bash
pio run -e esp32dev
```

2. Flash:

```bash
pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001
```

3. Validation minimale série (via terminal série 115200):

```text
PING
STATUS
ESPNOW_DEVICE_NAME_GET
DIAL_MEDIA_MAP_GET
HOTLINE_STATUS
```

## Script contrôleur ESP-NOW (terrain)

Script: `scripts/espnow_hotline_control.py`

Exemples:

```bash
python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target broadcast+discovery --target-name HOTLINE_PHONE --action ring
python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --action discover --target-name HOTLINE_PHONE --discover-rounds 3
python3 scripts/espnow_hotline_control.py --port /dev/cu.usbserial-0001 --target AA:BB:CC:DD:EE:FF --ensure-peer --action hotline1
```

## Monitoring hotline live

```bash
python3 scripts/hotline_live_monitor.py --port /dev/cu.usbserial-0001 --expect 1,2,3
```

## Gate de validation

- Contrats/tests Python:

```bash
python3 -m pytest -q scripts/test_hw_validation_contracts.py scripts/test_runtime_contracts.py
```

- Validation hardware A252:

```bash
python3 scripts/hw_validation.py \
  --port-a252 /dev/cu.usbserial-0001 \
  --no-require-hook-toggle \
  --strict-serial-smoke \
  --allow-capture-fail-when-disabled \
  --audio-probe-path /welcome.wav \
  --require-contract-version A252_AUDIO_CHAIN_V4
```

## Docs clés

- Contrat ESP-NOW: [docs/espnow_contract.md](docs/espnow_contract.md)
- API ESP-NOW: [docs/espnow_api_v1.md](docs/espnow_api_v1.md)
- Plan tonal/audio: [docs/audio_tone_plan.md](docs/audio_tone_plan.md)
- Gate qualité: [docs/branch_quality_gate.md](docs/branch_quality_gate.md)
- Orchestration dual-repo RTC/Zacus: [docs/CROSS_REPO_INTELLIGENCE.md](docs/CROSS_REPO_INTELLIGENCE.md)























<!-- CHANTIER:AUDIT START -->
## Audit & Execution Plan (2026-03-10)

### Snapshot
- Priority: `P2`
- Tech profile: `embedded+cpp/cmake`
- Workflows: `yes`
- Tests: `yes`
- Debt markers: `1`
- Source files: `84`

### Corrections Prioritaires
- [ ] Vérifier target PlatformIO et budget mémoire
- [ ] Ajouter/fiabiliser les commandes de vérification automatiques.
- [ ] Clore les points bloquants avant optimisation avancée.

### Optimisation
- [ ] Identifier le hotspot principal et mesurer avant/après.
- [ ] Réduire la complexité des modules les plus touchés.

### Mémoire chantier
- Control plane: `/Users/electron/.codex/memories/electron_rare_chantier`
- Repo card: `/Users/electron/.codex/memories/electron_rare_chantier/REPOS/RTC_BL_PHONE.md`

<!-- CHANTIER:AUDIT END -->
