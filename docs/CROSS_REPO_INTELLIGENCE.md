# Cross-Repo Intelligence Base

Date: 2026-02-20

## Repos de référence

- `electron-rare/RTC_BL_PHONE`:
  - firmware téléphonie RTC + Bluetooth + WiFi + MQTT + ESP-NOW.
  - repo d’exécution pour la stack A252.
- `electron-rare/le-mystere-professeur-zacus`:
  - repo scénario/UI avec bridge WiFi/ESP-NOW orienté événements story.
  - référence pour formats de payload entrants hétérogènes.
- `electron-rare/Kill_LIFE`:
  - repo base méthodologique (agents, gates, evidence pack, standards firmware).
  - référence process qualité/traçabilité.

## Standards repris depuis Kill_LIFE

- Gates minimales obligatoires sur changements firmware:
  - `Spec` -> `Build` -> `Test` -> `Release`.
- Evidence minimum à conserver:
  - logs build/test,
  - résultat CI,
  - artefacts de validation.
- Standards firmware:
  - PlatformIO + Unity,
  - wrappers autour des drivers,
  - timeouts/watchdogs pour IO bloquants.

## Contrat d’interop RTC <-> Zacus

- Commandes série ESP-NOW alignées:
  - `ESPNOW_ON`
  - `ESPNOW_OFF`
  - `ESPNOW_STATUS`
  - `ESPNOW_PEER_ADD <mac>`
  - `ESPNOW_PEER_DEL <mac>`
  - `ESPNOW_PEER_LIST`
  - `ESPNOW_SEND <mac|broadcast> <payload>`
- Payload entrant bridge accepté par `RTC_BL_PHONE`:
  - direct: `cmd`, `raw`, `command`, `action`
  - imbriqué: `event.*`, `message.*`, `payload.*`

## Règle opérationnelle

- Toute évolution protocolaire ESP-NOW doit:
  - être documentée ici + `docs/props.md`,
  - être testée au minimum par build terminal + smoke host,
  - être reflétée dans une PR liée côté repo partenaire si impact croisé.
