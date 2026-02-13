# RTC_BL_PHONE

Projet PlatformIO ESP32 pour recycler un téléphone RTC ancien (combiné, clavier, hook), avec intégration Bluetooth HFP pour les appels (émission/réception).

## Démarrage rapide
1. Ouvrir le dossier dans PlatformIO.
2. Option A: renseigner l'adresse MAC dans `src/main.cpp` (`DEFAULT_PEER_ADDR`).
3. Option B: la définir au runtime avec la commande série `p <mac>`.
4. Compiler et flasher l'environnement `esp32dev` (par défaut).
5. Ouvrir le moniteur série à 115200 bauds.
6. Connecter puis piloter les appels via commandes série.

## Commandes série
- `h` : aide
- `s` : statut runtime (hook, HFP, audio, call)
- `p <mac>` : configure la MAC du téléphone (`AA:BB:CC:DD:EE:FF`)
- `b` : connexion HFP vers le téléphone (Audio Gateway)
- `x` : déconnexion HFP
- `m <numero>` : émission d'appel
- `a` : décrocher un appel entrant
- `e` : raccrocher / rejeter
- `v <0..15>` : volume speaker HFP

## Cibles matérielles
- **ESP32 (Classic BT)** : support HFP complet (`esp32dev`).
- **ESP32-S3** : Bluetooth Classic non supporté par le silicium, HFP indisponible (le firmware reste compilable avec messages de fallback).

## Comportement hook/ring
- Si combiné **raccroché** (`ON_HOOK`) : ligne coupée.
- Si appel entrant : `pinRingCmd` activé, sonnerie pilotable côté AG1171S.
- Si décroché pendant sonnerie : `answer` automatique.
- Si raccroché pendant appel : `end/reject` automatique.

## Choix de cartes ESP32
Voir `docs/solutions_rtc_phone_esp32.md` pour la shortlist des DevKit utilisables (ESP32-DevKitC, ESP32-S3-DevKitC-1, NodeMCU-32S, LOLIN32), les liens de référence web, et les solutions d’interface (direct combiné/clavier, SLIC/FXS, ATA externe), dont une variante AG1171S (Silvertel).

## Plan projet (chef de projet)
Voir `docs/plan_chef_projet_esp32s3_ag1171s.md` pour le planning en phases, les risques, les critères d'acceptation et les livrables de la version ESP32-S3 + AG1171S.
