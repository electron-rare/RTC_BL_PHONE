# ESP-NOW API v1 (`rtcbl/1`)

Date: 2026-02-21  
Scope: contrat d'échange entre `RTC_BL_PHONE` (A252) et la seconde carte.

## 1. Objectif

Normaliser les trames ESP-NOW pour:
- corréler requête/réponse,
- conserver compatibilité legacy,
- simplifier l'intégration du second repo.

## 2. Requête v1

```json
{
  "proto": "rtcbl/1",
  "id": "req-001",
  "ts": 1730000000,
  "source": "a252",
  "target": "peer|broadcast",
  "cmd": "STATUS|CALL|WIFI_STATUS|BT_STATUS|...",
  "args": {}
}
```

Règles:
- `proto` obligatoire pour activer le mode v1.
- `id` recommandé (corrélation réponse).
- `cmd` obligatoire.
- `args` optionnel. S'il est présent, il est sérialisé et transmis au dispatcher.

## 3. Réponse v1

```json
{
  "proto": "rtcbl/1",
  "id": "req-001",
  "ok": true,
  "code": "STATUS",
  "data": {},
  "error": ""
}
```

Règles:
- `id` reprend la requête.
- `ok=false` => `error` non vide.
- `data` contient le JSON de la commande si disponible.
- fallback possible `data_raw` si la réponse n'est pas JSON.

## 4. Compatibilité legacy

Le firmware continue d'accepter les formats existants:
- `{"cmd":"..."}`
- `{"raw":"..."}`
- `{"command":"..."}`
- `{"action":"..."}`
- variantes imbriquées via `event`, `message`, `payload`

## 5. Commandes recommandées v1

- `STATUS`
- `CALL`
- `WIFI_STATUS`
- `MQTT_STATUS`
- `ESPNOW_STATUS`
- `BT_STATUS`

## 6. Intégration second repo

Checklist minimum côté seconde carte:
1. Émettre `proto=rtcbl/1` + `id` unique par requête.
2. Implémenter timeout de réponse (2-5s).
3. Corréler sur `id`.
4. Prévoir fallback legacy si `proto` absent en réponse.
