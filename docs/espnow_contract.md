# ESP-NOW Contract — Freenove (Quick)

Cible: intégration seconde carte (app/RTC).

## Recommandation trame
- `type=command`
- `msg_id` et `seq` pour corrélation
- `ack=true` pour obtenir un retour
- `payload` en JSON ou texte

Exemple recommandé:

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

## Réponse ACK attendue

```json
{
  "msg_id": "req-001",
  "seq": 7,
  "type": "ack",
  "ack": true,
  "payload": { "ok": true, "code": "WIFI_STATUS", "error": "", "data": {} }
}
```

## Compatibilité entrées
Le parseur accepte aussi:

- `cmd`, `command`, `action` (root ou `payload`)
- format texte "`CMD arg`"

## Commandes supportées (ESP-NOW)

- `STATUS`
- `WIFI_STATUS`
- `ESPNOW_STATUS`
- `UNLOCK`, `NEXT`
- `WIFI_DISCONNECT`, `WIFI_RECONNECT`
- `ESPNOW_ON`, `ESPNOW_OFF`
- `STORY_REFRESH_SD`
- `SC_EVENT`
- `RING`
- `SCENE <id>`
  - exemples: `SCENE SCENE_WIN_ETAPE`
  - JSON: `{"cmd":"SCENE","args":{"id":"SCENE_WIN_ETAPE"}}`
  - `SCENE` retourne les erreurs:
    - `missing_scene_id` si `id` absent
    - `scene_not_found` si aucune scène active pour un `NEXT`
- Actions de contrôle (générique): `HW_*`, `AUDIO_*`, `MEDIA_*`, etc.

## Erreurs fréquentes
- `unsupported_command`
- `missing_scene_id`
- `scene_not_found`
- `WIFI_RECONNECT no_credentials` (pas de SSID/pw stocké)
- erreurs réseau/connexion habituelles (`peer`, `payload` vide, trame > 240)

## Limites runtime
- Trame brute max: `240`
- Peers: `16`
- RX queue: `6`

## Réception côté firmware
- `type=command` -> `executeEspNowCommandPayload`
- autre `type` -> ignoré en commande (bridge story seulement si activé)
- les frames `type=ack` reçues sont ignorées côté dispatch
