# Spécification Bluetooth Téléphonie — HFP + PBAP + Numérotation (v1)

Date: 2026-02-21  
Repo: `electron-rare/RTC_BL_PHONE`

## 1. Contexte

Le firmware expose des commandes Bluetooth (`BT_HFP_CONNECT`, `BT_STATUS`, etc.) mais la liaison HFP réelle n'est pas encore validée de bout en bout avec un téléphone.
Le besoin produit inclut explicitement:
- audio d'appel + états d'appel (HFP),
- synchronisation des contacts (PBAP),
- numérotation depuis le firmware (AT/HFP) avec session HFP active.

## 2. Objectif

Livrer une stack Bluetooth téléphonie réellement opérationnelle sur ESP32 Audio Kit:
1. HFP réel fonctionnel (pas seulement commandes firmware),
2. synchronisation contacts via PBAP,
3. numérotation sortante via commandes HFP/AT.

## 2-bis. État d’implémentation (snapshot 2026-02-21)

- HFP commandes firmware: implémenté (`BT_HFP_CONNECT`, `BT_HFP_DISCONNECT`, `BT_DISCOVERABLE_ON/OFF`, `BT_STATUS`).
- Politique de reconnexion: si un peer bondé est trouvé au boot, le firmware planifie la reconnexion HFP automatiquement.
- Politique runtime: `BT_AUTO_RECONNECT_ON/OFF` permet de forcer le mode autoradio en live.
- Numérotation/contrôle appel via HFP AT: implémenté côté firmware:
- `BT_DIAL <number>` / alias `DIAL <number>`
- `BT_REDIAL`
- `BT_ANSWER`
- `BT_HANGUP`
- `BT_CALLS` (query `AT+CLCC`)
- `BT_STATUS` expose maintenant `slc_connected`, `call_state`, `last_dialed_number`.
- PBAP (contacts): non disponible sur la stack actuelle `arduino-esp32` (Bluedroid exposé sans API PBAP côté firmware). La commande `BT_PBAP_SYNC` retourne explicitement `unsupported`.
- Audio local RTC: tonalité de numérotation calée à `425 Hz` (couleur France/Europe), niveau renforcé.
- Wiring bench verrouillé:
  - `RM=GPIO18`, `FR=GPIO5`, `SHK=GPIO23`, `PD=GPIO19`
  - `AMP_EN=GPIO21` actif bas (`LOW=ON`)
  - `LINE` retiré du runtime (non utilisé).

## 3. Périmètre

In-scope:
- Profil HFP (connexion RFCOMM/SLC + audio link),
- Profil PBAP (récupération carnet/adresses utiles),
- Commandes de numérotation et contrôle d'appel via HFP/AT,
- Validation réelle sur téléphone (iOS/macOS/Android selon disponibilité banc).

Out-of-scope v1:
- streaming musique A2DP,
- UI avancée carnet (recherche/favoris), hors MVP téléphonie.

## 4. Exigences fonctionnelles

### EF-01 — HFP réel
- Le firmware DOIT établir une session HFP réelle avec un téléphone compatible.
- `BT_STATUS` DOIT refléter:
- `connected=true` après connexion RFCOMM/SLC,
- `hfp_active=true` lorsque l'audio HFP est actif.
- Les commandes `BT_HFP_CONNECT` et `BT_HFP_DISCONNECT` DOIVENT piloter la session réelle.
- Le firmware DOIT exposer un contrôle runtime du mode auto reconnect (`BT_AUTO_RECONNECT_ON/OFF` et statut via `BT_STATUS`).

### EF-02 — PBAP contacts
- Le firmware DOIT récupérer un sous-ensemble de contacts via PBAP.
- Les métadonnées minimales DOIVENT inclure: `display_name`, `phone_number`.
- La synchro DOIT gérer:
- premier chargement,
- rafraîchissement,
- erreur d'accès/pairing (message explicite).

### EF-03 — Numérotation
- Le firmware DOIT exposer une commande de numérotation sortante (ex: `DIAL <number>`).
- La numérotation DOIT accepter la demande même si SLC n'est pas encore monté:
  - mettre le numéro en file d'attente,
  - déclencher/reprendre la connexion HFP,
  - émettre automatiquement le `dial` dès `SLC_CONNECTED`.
- Les transitions d'appel DOIVENT être visibles dans `STATUS`/`BT_STATUS` (idle, dialing, ringing, active, ended).

### EF-04 — Compatibilité pairing
- L'appareil DOIT être détectable quand requis pour jumelage.
- Le flux de jumelage DOIT être documenté et testable depuis iPhone/Mac.
- Les erreurs de profil non supporté DOIVENT être tracées avec cause exploitable.

### EF-05 — Appel entrant GSM vers sonnerie RTC
- En cas d'appel entrant détecté côté HFP (`call_state=ringing`), le téléphone RTC DOIT sonner.
- Au décroché RTC, le firmware DOIT déclencher `BT_ANSWER`.
- Précondition explicite: téléphone GSM appairé et liaison HFP active (ou reconnexion auto en cours).
- Si aucun peer n'est actif, le firmware DOIT rester découvrable pour restauration du lien.

## 5. Exigences non fonctionnelles

- Temps de connexion HFP cible: < 15s dans des conditions normales.
- Aucun crash du firmware en cas d'échec pairing/profil.
- Logs série actionnables pour chaque étape: connect, slc, audio, pbap sync, dial.

## 6. Critères d'acceptation (DoD)

- [ ] HFP réel validé sur hardware (preuve `connected=true` et `hfp_active=true`).
- [ ] PBAP contacts synchronisés avec au moins un contact lisible (bloqué stack actuelle, nécessite changement de stack BT).
- [ ] Numérotation sortante validée sur téléphone réel depuis commande firmware (`BT_DIAL`).
- [ ] Numérotation sortante validée en mode queue: `DIAL`/`BT_DIAL` avant SLC, puis émission auto après `SLC_CONNECTED`.
- [ ] Appel entrant GSM validé: sonnerie RTC, décroché RTC => `BT_ANSWER`.
- [ ] Rapport de test hardware mis à jour avec preuves (logs + JSON).
- [ ] Documentation opératoire de pairing/mise en service publiée.
- [ ] Stabilité appel: maintien HFP/SCO sans décrochage sur scénario appel sortant réel (preuve logs `hfp_*` + `sco_*`).

## 7. Preuves attendues

- Rapport JSON/Markdown de tests hardware Bluetooth téléphonie.
- Captures ou logs montrant la session HFP active.
- Extrait de contacts PBAP récupérés.
- Trace de numérotation et état d'appel.
