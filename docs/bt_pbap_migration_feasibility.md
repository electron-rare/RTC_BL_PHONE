# Faisabilité migration Bluetooth PBAP

Date: 2026-02-21  
Repo: `RTC_BL_PHONE`

## Résumé exécutif

- Constats actuels:
  - HFP est opérable côté commandes firmware.
  - PBAP n'est pas exposé par la stack Arduino-ESP32/Bluedroid utilisée actuellement.
- Décision court terme:
  - conserver Track A livrable (HFP + WebUI + WiFi + ESP-NOW),
  - lancer Track B migration stack pour PBAP.

## 1) État technique actuel

- Stack firmware active: Arduino ESP32 + Bluedroid (HFP client).
- PBAP: API indisponible dans ce périmètre actuel.
- Symptôme attendu: `BT_PBAP_SYNC` retourne `unsupported` avec cause explicite.

## 2) Cibles étudiées

1. Arduino-ESP32 actuel (baseline):
   - Avantage: stable, faible risque régression.
   - Limite: PBAP absent.
2. Mode mixte Arduino + ESP-IDF (backend BT dédié):
   - Avantage: accès plus bas niveau profils BT.
   - Risque: complexité build/intégration.
3. Backend BT alternatif (abstraction `IBluetoothBackend`):
   - Avantage: migration progressive sans casser Track A.
   - Risque: surcharge maintenance temporaire.

## 3) Critère GO / NOGO Track B

GO si:
- PBAP sync fonctionne réellement (au moins 1 contact avec `display_name`, `phone_number`).
- Build reproductible via `platformio.ini` (`env:esp32dev-bt-migrate`).
- Pas de régression des commandes HFP existantes.

NOGO si:
- PBAP non atteignable en 2 itérations de migration contrôlée.
- Régressions HFP non résolues.

## 4) Plan de pivot

Si NOGO:
1. marquer PBAP `blocked by stack`,
2. conserver Track A en production,
3. ouvrir lot dédié migration profonde (estimation + risques + dépendances).

## 5) Implémentation préparatoire déjà posée

- Environnement de build migration ajouté:
  - `env:esp32dev-bt-migrate` (`-DBT_BACKEND_EXPERIMENTAL=1`)
- Interface d'abstraction introduite:
  - `src/bluetooth/IBluetoothBackend.h`

## 6) Risques

- Risque principal: coût de migration stack BT supérieur au bénéfice court terme.
- Risque secondaire: divergence comportements HFP entre backend baseline et backend migré.
