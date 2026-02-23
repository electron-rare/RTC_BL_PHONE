# ESP-NOW API v1

Date: 2026-02-23

Ce document est synchronisé avec le contrat opérationnel: `docs/espnow_contract.md`.

## Source de vérité

- Canonique: `docs/espnow_contract.md`
- Ce fichier doit rester cohérent avec ce contrat.

## Reprise du contrat (v1)

- Trame recommandée: `type: "command"`, `ack: true`.
- Corrélation: `msg_id` + `seq`.
- Corps: objet JSON dans `payload`, ou texte compatible selon parser legacy.

Exemple de demande:

```json
{
  "msg_id": "req-001",
  "seq": 7,
  "type": "command",
  "ack": true,
  "payload": {
    "cmd": "WIFI_STATUS"
  }
}
```

Réponse attendue:

```json
{
  "msg_id": "req-001",
  "seq": 7,
  "type": "ack",
  "ack": true,
  "payload": {
    "ok": true,
    "code": "WIFI_STATUS",
    "error": "",
    "data": {}
  }
}
```

## Compatibilité parser (legacy)

- `cmd`, `command`, `action` (root ou `payload`).
- format texte: `"CMD arg"`.
- ancien champ `id` au lieu de `msg_id` côté trames historiques.

## Commandes supportées

- `STATUS`
- `WIFI_STATUS`
- `ESPNOW_STATUS`
- `UNLOCK`
- `NEXT`
- `WIFI_DISCONNECT`
- `WIFI_RECONNECT`
- `ESPNOW_ON`
- `ESPNOW_OFF`
- `STORY_REFRESH_SD`
- `SC_EVENT`
- `RING`
- `SCENE <id>`

## Erreurs connues

- `unsupported_command`
- `missing_scene_id`
- `scene_not_found`
- erreurs réseau: `peer`, `payload` vide, trame > 240

## Limites runtime

- Trame brute max: `240`
- Peers: `16`
- RX queue: `6`

## Réception firmware

- `type=command` -> `executeEspNowCommandPayload`
- `type` non-`command` ignoré pour dispatch de commande
- `type=ack` ignoré côté dispatch commande
