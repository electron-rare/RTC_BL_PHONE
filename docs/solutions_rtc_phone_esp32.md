# Solutions recommandées: ESP32 + téléphone RTC ancien

## Objectif
Créer un projet PlatformIO sur ESP32 pour réutiliser un ancien téléphone analogique RTC (combiné, clavier, crochet) avec une architecture moderne.

## Contraintes importantes
- **Ne jamais connecter directement** l'ESP32 à une ligne téléphonique publique (PSTN) sans isolation/éléments homologués.
- Les tensions de sonnerie peuvent être élevées; prévoir isolation galvanique si interface ligne 2 fils.
- La qualité audio dépend fortement du front-end analogique (adaptation d'impédance, gain, anti-larsen).

## Option A — Réutiliser uniquement le combiné + clavier (recommandé MVP)
### Principe
- Débrancher l'électronique ligne d'origine.
- Lire en hard:
  - hook switch (décroché/raccroché)
  - clavier (matrice)
- Audio:
  - micro du combiné vers préampli + ADC/codec I2S
  - écouteur via DAC/codec + ampli

### Avantages
- La plus simple pour un premier prototype.
- Sécurité électrique plus facile à maîtriser.
- Excellente maîtrise logicielle côté ESP32 (Bluetooth, Wi-Fi, SIP, intercom local).

### Inconvénients
- Nécessite un peu d'analogique audio (préamp, filtre, gain).
- Le clavier ancien peut demander reverse engineering.

## Option B — Interface 2 fils complète type "ligne RTC privée" (SLIC/DAA)
### Principe
- Conserver le téléphone quasiment tel quel (port RJ11/2 fils).
- Générer alimentation de boucle, tonalité, éventuellement sonnerie via circuit spécialisé (SLIC).

### Avantages
- Expérience "téléphone d'origine" la plus fidèle.
- Compatible avec plusieurs postes analogiques.

### Inconvénients
- Plus complexe et coûteux.
- Design analogique + sécurité plus exigeants.
- Debug plus long.

## Option C — Passerelle hybride (ATA/FXS externe + ESP32 contrôle)
### Principe
- L'audio et la boucle téléphonique sont gérés par un matériel externe (ATA/box FXS).
- ESP32 gère logique applicative (boutons, scénario d'appel, domotique, BLE/Wi-Fi).

### Avantages
- Déploiement rapide avec qualité voix stable.
- Risque analogique réduit.

### Inconvénients
- Dépendance à un boîtier externe.
- Moins "DIY full stack".

## Dev kits ESP32 utilisables (vérifiés web)
> Recherche croisée faite sur la doc officielle Espressif (`esp-dev-kits`) et la liste des cartes PlatformIO (`espressif32`).

### Recommandés pour TON projet (RTC + combiné + clavier + audio)
1. **ESP32-DevKitC (WROOM-32)**
   - Très compatible avec exemples existants.
   - Wi-Fi + Bluetooth Classic/BLE utile si tu vises audio BT/HFP plus tard.
   - Mapping PlatformIO simple: `board = esp32dev`.

2. **ESP32-S3-DevKitC-1**
   - Plus récent, bon support I/O et USB natif.
   - Très bon choix pour version long terme (UI, USB CDC, plus de marge CPU/RAM selon variantes).
   - Mapping PlatformIO: `board = esp32-s3-devkitc-1`.

3. **NodeMCU-32S / LOLIN32**
   - Faciles à trouver et souvent moins chers.
   - Bons pour prototypage hook + clavier + logique de numérotation.
   - Mapping PlatformIO: `board = nodemcu-32s` ou `board = lolin32`.

### Utilisables mais moins adaptés à ton besoin immédiat
- **ESP32-C3-DevKitC-02**
  - Très bien pour projets IoT BLE/Wi-Fi.
  - Mais pas de Bluetooth Classic, donc moins idéal si objectif audio BT "téléphone" avancé.

## Recommandation pratique
1. **Phase 1 (MVP): Option A + ESP32-DevKitC**
   - Hook + clavier + audio local simple.
   - Validation UX (décroché, composition, feedback audio).
2. **Phase 2:** ajout Bluetooth/Wi-Fi (SIP/WebRTC passerelle).
3. **Phase 3:** si besoin d'un vrai port 2 fils: migration vers Option B.

## Matériel conseillé (MVP)
- ESP32-WROOM / ESP32-S3.
- Codec audio I2S simple (ou carte audio ESP32 dédiée).
- Petit ampli casque/écouteur + préampli micro électret/carbone selon combiné.
- Optocoupleur ou transistors pour lecture fiable hook/clavier si tensions atypiques.
- Alimentation propre 5V + 3.3V, masse analogique soignée.

## Notes logiciel PlatformIO
- Démarrer avec un squelette Arduino + machine d'états:
  - `IDLE` (raccroché)
  - `OFF_HOOK`
  - `DIALING`
  - `IN_CALL`
- Ajouter journal série détaillé dès le début.
- Écrire des tests de mapping clavier (table de correspondance).

## Sources web utiles
- Espressif DevKitC: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/index.html
- Espressif S3 DevKitC-1: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html
- PlatformIO `esp32dev`: https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html
- PlatformIO `esp32-s3-devkitc-1`: https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html
- PlatformIO `nodemcu-32s`: https://docs.platformio.org/en/latest/boards/espressif32/nodemcu-32s.html
- PlatformIO `lolin32`: https://docs.platformio.org/en/latest/boards/espressif32/lolin32.html
- PlatformIO `esp32-c3-devkitc-02`: https://docs.platformio.org/en/latest/boards/espressif32/esp32-c3-devkitc-02.html

## Décision proposée
Pour ton besoin immédiat (PLIP / combiné + clavier en hard), la **meilleure balance risque/temps** est:
- **Option A en premier** avec **ESP32-DevKitC**.
- Puis extension progressive vers ESP32-S3 ou interface ligne complète si nécessaire.
