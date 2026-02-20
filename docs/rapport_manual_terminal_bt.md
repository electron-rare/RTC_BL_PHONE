# Rapport terminal manuel (audio/SLIC/HFP/BLE)

- Date UTC: 2026-02-20T20:32:52.137481+00:00
- Port serie: `/dev/cu.usbserial-0001`
- Mac Bluetooth local: `78:4F:43:8A:0F:3E`
- MAC cible HFP testee: `B4:1B:B0:84:73:06`

| Scenario | Resultat | Evidence |
|---|---|---|
| SLIC ring trigger | PASS (logique) | `CALL` -> `OK CALL`, puis `STATUS.telephony.state=RINGING` |
| Hook transition reel | MANUAL_REQUIRED | Necessite decroche/raccroche physique pour observer `ON_HOOK/OFF_HOOK` |
| Audio play path | PASS (commande) | `PLAY /welcome.wav` -> `OK PLAY` |
| Audio capture runtime | PASS (commande) | `CAPTURE_START`/`CAPTURE_STOP` -> `OK`; metriques non evolutives sans signal audio reel |
| BLE control command | PASS (commande) | `BT_BLE_START` -> `ble_active=true`, `BT_BLE_STOP` -> `ble_active=false` |
| BLE advertising reel | FAIL (non prouve) | Le firmware actuel ne publie pas de service BLE reel; etat interne uniquement |
| HFP connect command | PASS (commande) | `BT_HFP_CONNECT B4:1B:B0:84:73:06` -> `connected=true,hfp_active=true` |
| HFP liaison radio reelle | FAIL (non prouve) | Cote macOS, l'appareil reste dans `Not Connected`; pas de session HFP etablie |

## Conclusion

- Les scenarios restants peuvent etre pilotes depuis le terminal, mais seulement en validation **commande/etat firmware**.
- La validation **materielle reelle** (audio analogique, hook physique, HFP/BLE radio reel) n'est pas totalement atteinte avec l'implementation BT actuelle.
- `BluetoothManager` reste un stub logiciel (flags internes), sans pile HFP/BLE de production.

