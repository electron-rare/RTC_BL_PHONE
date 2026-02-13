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

### Variante concrète: module **Silvertel AG1171S**
Si tu veux garder un ancien téléphone tel quel (2 fils TIP/RING) avec une carte ESP32, l'AG1171S est une piste concrète côté **FXS/SLIC**.

#### Ce que ça apporte
- Génération de la boucle analogique pour alimenter un poste RTC ancien.
- Interface pensée pour créer une "ligne privée" locale (pas une connexion directe PSTN brute).
- Réduction de la complexité analogique par rapport à un design SLIC discret from-scratch.

#### Comment l'intégrer proprement avec ESP32
1. **AG1171S = étage ligne analogique** (alimentation de boucle, interface 2 fils).
2. **ESP32 = logique et signalisation** (états off-hook/on-hook, numérotation, scénarios).
3. **Chemin audio**:
   - soit via codec audio externe (I2S) + adaptation vers l'étage ligne,
   - soit via une architecture où l'audio analogique reste majoritairement côté téléphonie, et l'ESP32 pilote surtout la logique.
4. **Détection d'événements**: exposer vers GPIO les états utiles (hook, ring detect selon schéma d'application).

#### Points d'attention (importants)
- Respecter strictement les notes d'application Silvertel (DC feed, protection, découplage, routage).
- Ne pas connecter à une ligne publique sans conformité réglementaire et schéma adapté.
- Prévoir protections (surintensité/surtension) et isolation selon l'usage final.
- Valider impédance et niveau audio pour éviter faible volume, saturation, écho/larsen.

#### Recommandation projet
- **MVP rapide**: continuer Option A (combiné + clavier en hard) pour avancer sur firmware.
- **Version "vraie ligne 2 fils"**: passer en Option B avec AG1171S quand la logique applicative est stable.


### Proposition de mapping initial (ESP32-S3-DevKitC-1)
- `GPIO4` : Hook sense (entrée, `INPUT_PULLUP`, actif bas selon câblage).
- `GPIO5` : Ring command (sortie logique vers étage de commande sonnerie).
- `GPIO6` : Line enable (sortie logique vers activation ligne AG1171S).
- `GPIO48`: LED debug état firmware.

> Ce mapping est un **point de départ firmware** et doit être validé/ajusté selon le schéma AG1171S final.

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


## Solutions existantes pour interfacer un ancien téléphone à un microcontrôleur

Oui, il existe plusieurs familles de solutions déjà utilisées en pratique:

1. **Interface combiné “en direct” (hook + clavier + audio séparé)**
   - Tu n’utilises pas la ligne RJ11 d’origine: tu récupères seulement le crochet, le clavier, le micro et l’écouteur.
   - C’est la méthode la plus simple pour ESP32 et la plus sûre pour un prototype.
   - Très bien pour interphone, SIP local, Bluetooth, domotique vocale.

2. **Interface FXS/SLIC (émuler une vraie ligne analogique privée)**
   - Le microcontrôleur pilote un front-end téléphonique spécialisé qui fournit tension de ligne, courant de boucle et sonnerie.
   - C’est la solution “propre téléphonie” si tu veux brancher un poste analogique sur 2 fils comme à l’époque.
   - Plus complexe en électronique et en sécurité CEM/isolement.

3. **Interface FXO/DAA (recevoir ou analyser une ligne analogique existante)**
   - Utilisée quand on veut interfacer une ligne analogique côté “réseau”.
   - Demande encore plus de prudence réglementaire/sécurité selon le pays.

4. **Passerelle ATA externe (FXS déjà intégrée)**
   - Exemple courant: un ATA SIP/FXS fait tout le travail téléphonique analogique.
   - L’ESP32 ne gère que la logique (capteurs, scénarios, UI, automation).
   - Super choix pour aller vite avec une bonne qualité audio.

5. **Décodage DTMF / cadran impulsionnel dédié**
   - Si ton téléphone est à cadran ou clavier ancien, tu peux ajouter un étage dédié:
     - décodage DTMF matériel,
     - ou capture d’impulsions du cadran via GPIO + anti-rebond/filtrage.
   - Souvent combiné avec l’Option A.

### Ce qui marche le mieux pour ton cas
- Vu ton besoin ("combiné + clavier en hard"), la meilleure approche reste:
  - **MVP: interface directe (Option A)**,
  - puis migration vers **ATA externe** ou **SLIC/FXS** si tu veux une expérience “ligne RTC” complète.

## Sources web utiles
- Espressif DevKitC: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32/esp32-devkitc/index.html
- Espressif S3 DevKitC-1: https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32s3/esp32-s3-devkitc-1/index.html
- PlatformIO `esp32dev`: https://docs.platformio.org/en/latest/boards/espressif32/esp32dev.html
- PlatformIO `esp32-s3-devkitc-1`: https://docs.platformio.org/en/latest/boards/espressif32/esp32-s3-devkitc-1.html
- PlatformIO `nodemcu-32s`: https://docs.platformio.org/en/latest/boards/espressif32/nodemcu-32s.html
- PlatformIO `lolin32`: https://docs.platformio.org/en/latest/boards/espressif32/lolin32.html
- PlatformIO `esp32-c3-devkitc-02`: https://docs.platformio.org/en/latest/boards/espressif32/esp32-c3-devkitc-02.html
- Asterisk documentation (PBX/VoIP): https://docs.asterisk.org/
- Exemple ATA FXS (Grandstream HT801): https://www.grandstream.com/products/gateways-and-atas/analog-telephone-adaptors/product/ht801
- Silvertel (catalogue modules téléphonie): https://www.silvertel.com/

## Décision proposée
Pour ton besoin immédiat (PLIP / combiné + clavier en hard), la **meilleure balance risque/temps** est:
- **Option A en premier** avec **ESP32-DevKitC**.
- Puis extension progressive vers ESP32-S3 ou interface ligne complète si nécessaire.
