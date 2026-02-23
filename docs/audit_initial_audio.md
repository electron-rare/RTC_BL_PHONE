# Audit Audio / BT / RTOS — état et plan d'enchaînement

## État validé (run local USB `/dev/cu.usbserial-0001`)
- Boot stable: audio + codec + web server OK.
- Capture audio: arbitrage multi-clients en place (`TELEPHONY`, `BLUETOOTH`, `GENERIC`).
- Bluetooth HFP:
- plus de crash immédiat au boot.
- plus d'auto-connect agressif au boot (pair bond restauré, connexion sur demande).
- dial peut rester en queue (`pending_dial_number`) si SLC non prêt.
- Coexistence WiFi/BT:
- erreurs fatales/abort au boot réduites après déport d'actions callback vers `tick()`.

## Correctifs clés appliqués
- `AudioEngine`:
- `requestCapture/releaseCapture` thread-safe.
- task audio avec backoff idle (charge RTOS réduite).
- `TelephonyService` / `BluetoothManager`:
- plus de `startCapture/stopCapture` concurrents.
- utilisation des clients de capture dédiés.
- `BluetoothManager`:
- déport de `applyDiscoverablePolicy/publishBleStatus` hors callbacks.
- suppression d’auto-reconnect HFP au boot.
- timeout SLC en mode backoff (sans disconnect agressif).
- désactivation `AgentSupervisor` côté BT pour retirer allocations C++ en callback.

## Plan exécutable (skills coordonnés)

### Track 1 — `esp32-rtos-coex-triage` (en cours)
- Objectif: zéro reboot/assert sur séquence boot + WiFi STA + BT activé.
- Actions:
- garder le BT callback-safe (déjà fait).
- surveiller `wifi: fail to alloc timer` sur endurance.
- livrer un rapport de stabilité sur 10 min.

### Track 2 — `esp32-bt-hfp-stability` (prochaine étape)
- Objectif: connexion GSM stable, SLC up, dial auto depuis buffer len10.
- Actions:
- test live: `BT_HFP_CONNECT`, `BT_STATUS`, appel sortant/entrant.
- valider transition `pending_dial_number -> dial requested` quand SLC connecté.
- verrouiller comportement discoverable auto si non connecté.

### Track 3 — `esp32-audio-runtime-gating` (prochaine étape)
- Objectif: tonalité et audio call sans glitch ni coupure.
- Actions:
- vérifier arrêt instant tonalité au 1er digit.
- vérifier arrêt tonalité immédiat au raccroché.
- valider chemin HFP audio I2S en appel actif.

## Critères de sortie
- Aucun `abort/assert` pendant test run.
- Aucun `vQueueDelete queue.c` observé.
- Numérotation 10 chiffres pulse/DTMF déclenche `BT_DIAL` avec succès quand SLC monte.
- Appel entrant GSM: ring, décroché, audio duplex.

## Plan autonome coordonné (skills + agents)

- Skill utilisé pour cadrage spec->implémentation: `notion-spec-to-implementation` (adapté en mode repo local).
- Agent `bt-hfp-stability`:
  - garantir `DIAL => queue + connect HFP + auto-dial post-SLC`.
  - garder `discoverable=true` si aucun client BT connecté.
- Agent `rtos-coex-wifi-bt`:
  - forcer `WIFI_PS_MIN_MODEM` en STA/APSTA pour coexistence WiFi+BT.
  - supprimer les reboots `Should enable WiFi modem sleep ...`.
- Agent `telephony-call-routing`:
  - appel entrant HFP (`ringing`) => sonnerie RTC.
  - décroché RTC en sonnerie => `BT_ANSWER`.
- Gate de validation utilisateur (bench USB):
  - confirmer logs `dial_trigger ... ok=true` puis `hfp_slc_connected`/`hfp_dial_requested`.
  - confirmer appel entrant GSM => sonnerie RTC immédiate.

## Exécution autonome (batch courant)
- `main.cpp`:
  - ordre de boot ajusté pour réduire contention WiFi/BT (`WiFi/ESP-NOW` avant `BT begin`).
  - commandes runtime ajoutées: `BT_AUTO_RECONNECT_ON`, `BT_AUTO_RECONNECT_OFF`.
- `BluetoothManager`:
  - flag runtime `auto_reconnect_enabled` exposé dans `BT_STATUS`.
  - reconnexion auto conditionnée par policy + queue dial.
- `WebServerManager` + WebUI:
  - SSE réactivé (`/api/events`) + fallback polling navigateur.
  - endpoint ajouté: `/api/bluetooth/hfp/auto`.
- `EspNowBridge`:
  - init WiFi coex durci (mode préservé + modem sleep policy).
