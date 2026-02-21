# Orchestration ZeroClaw (RTC)

Dernière mise à jour: 2026-02-21.

## Objectif

Rendre les boucles hardware RTC plus robustes:

- préflight USB obligatoire avant upload/flash,
- session agent ZeroClaw ciblée sur le repo RTC,
- exécution coordonnée avec `Kill_LIFE` (orchestrateur dual-repo).

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

Commande de reference:

```bash
cd /Users/cils/Documents/Lelectron_rare/RTC_BL_PHONE
python3 scripts/zeroclaw_hw_preflight.py --require-port --zeroclaw-bin /Users/cils/Documents/Lelectron_rare/Kill_LIFE/zeroclaw/target/release/zeroclaw
pio run -e esp32dev
pio run -e esp32dev -t upload --upload-port /dev/cu.SLAB_USBtoUART
```

Smoke terminal attendu:

- `PING` => `PONG`
- `STATUS` => JSON avec `board_profile`, `telephony`, `hook`, `full_duplex`, metriques audio.

Note technique:

- Sur `esp32dev`, il faut initialiser le stack reseau avant `AsyncWebServer::begin()`
  pour eviter le crash `lwIP Invalid mbox`.
