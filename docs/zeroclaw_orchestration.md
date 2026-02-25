# Orchestration ZeroClaw (RTC)

Dernière mise à jour: 2026-02-25.

## Objectif

Rendre les boucles hardware RTC plus robustes:

- préflight USB obligatoire avant upload/flash,
- session agent ZeroClaw ciblée sur le repo RTC,
- exécution coordonnée avec `Kill_LIFE` (orchestrateur dual-repo).

## Mode retenu: Docker orchestration + hardware hôte

Stack active:

- orchestrateur: `http://127.0.0.1:8788`
- n8n: `http://127.0.0.1:5678`
- OpenWebUI: `http://127.0.0.1:3001`

Utilisation recommandée:

1. Pilotage agents/subagents via API orchestrateur (`GET /api/agents`, `GET /api/workflows`, `GET /api/status`, `POST /api/run`).
2. Build/flash/monitor RTC_A252 exécutés localement sur macOS (hors conteneur).

Contrainte validée:

- Ne pas lancer `rtc_firmware_loop` depuis l'orchestrateur Docker pour cette passe RTC:
  - conteneur sans chemin repo RTC hôte monté (`/Users/.../RTC_BL_PHONE`),
  - `pio` non installé dans l'image orchestrateur,
  - pas d'accès direct aux ports série macOS.

Actions Docker autorisées dans ce mode:

- `provider_scan`
- actions webhook (`rtc_webhook`, `zacus_webhook`, `general_prompt_fanout`)

Check health recommandé:

```bash
python3 scripts/zeroclaw_orchestrator_health.py --base-url "${ZEROCLAW_ORCH:-http://127.0.0.1:8788}"
```

## 1) Préflight hardware local

Lancer avant `autoflash.py`, `hw_validation.py` ou tout upload PlatformIO:

```bash
python3 scripts/zeroclaw_hw_preflight.py --zeroclaw-bin /Users/cils/Documents/Lelectron_rare/Kill_LIFE/zeroclaw/target/release/zeroclaw --require-port
```

Variantes:

```bash
python3 scripts/zeroclaw_hw_preflight.py --port /dev/tty.SLAB_USBtoUART
python3 scripts/zeroclaw_hw_preflight.py --port /dev/tty.usbserial-0001 --port /dev/tty.usbmodem5AB90753301
```

## 2) Conversation agent ciblée RTC

Depuis `Kill_LIFE`:

```bash
tools/ai/zeroclaw_dual_chat.sh rtc -m "fais un état hardware et propose 3 actions"
```

Si credentials manquants, choisir un provider:

- `zeroclaw auth login --provider openai-codex --device-code`
- ou `export OPENROUTER_API_KEY=... && export ZEROCLAW_PROVIDER=openrouter`

## 3) Boucle recommandée (issue -> PR)

1. Préflight USB (`zeroclaw_hw_preflight.py`).
2. Prompt court RTC.
3. Patch minimal RTC.
4. Tests locaux ciblés.
5. PR dédiée RTC avec logs/artefacts.

## 4) Run local ESP32 audio dev (A252 only)

Contexte bench actuel: carte `esp32dev` (audio dev), sans cible S3 connectee.

Commande de référence A252 (USB-Serial) :

```bash
cd /Users/cils/Documents/Lelectron_rare/RTC_BL_PHONE
python3 scripts/zeroclaw_hw_preflight.py --require-port --zeroclaw-bin /Users/cils/Documents/Lelectron_rare/Kill_LIFE/zeroclaw/target/release/zeroclaw
pio run -e esp32dev
pio run -e esp32dev -t upload --upload-port /dev/cu.usbserial-0001
```

Smoke terminal attendu:

- `PING` => `PONG`
- `STATUS` => JSON avec `board_profile`, `telephony`, `hook`, `full_duplex`, metriques audio.

Note technique:

- Sur `esp32dev`, il faut initialiser le stack reseau avant `AsyncWebServer::begin()`
  pour eviter le crash `lwIP Invalid mbox`.

## 5) Gate stricte A252 (local)

Variables attendues:

- `A252_PORT` (ex: `/dev/cu.usbserial-0001`)
- `A252_WIFI_SSID`
- `A252_WIFI_PASSWORD`
- `ZEROCLAW_ORCH` (défaut `http://127.0.0.1:8788`)

Exécution:

```bash
bash scripts/a252_strict_gate.sh
```

Sorties:

- `artifacts/zeroclaw_orchestrator_health.json`
- `artifacts/hw_validation_a252_report.json`
- `docs/hw_validation_a252_report.md`
- `docs/a252_strict_gate_summary.md`

## 6) Preuve boucle complète (2026-02-21)

Etat courant:

- `PR #20` (RTC) et `PR #22` (repo_state) sont déjà intégrées.
- `Kill_LIFE`: PR `#11` intégrée pour le watcher 1min + realtime log.
- `Zacus`: PR `#101` et `#103`/`#105` déjà intégrées.

Run proof:

- `python3 scripts/zeroclaw_hw_preflight.py --require-port --zeroclaw-bin /Users/cils/Documents/Lelectron_rare/Kill_LIFE/zeroclaw/target/release/zeroclaw` ✅
- `pio run -e esp32dev -e esp32-s3-devkitc-1` ✅
- `python3 scripts/hw_validation.py --port-a252 ... --port-s3 ... --bench-port ... --flash` ❌
  - bloqué: ports sérielles non distincts / indisponibles pour l’état bench (erreurs Resource busy + commandes inconnues).

Actions:

- Poursuivre sur l’issue RTC dédiée: https://github.com/electron-rare/RTC_BL_PHONE/issues/23
- Re-lancer la boucle dès que les ports A252, S3 et bench sont explicitement valables.
