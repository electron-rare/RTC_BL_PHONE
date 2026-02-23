
## Exemple de pinout ESP32-DevKitC (WROOM-32) + SLIC K50835F + PCM5102

| Fonction           | ESP32 Pin | Direction | Remarque                |
|--------------------|-----------|-----------|-------------------------|
| hookSense          | GPIO27    | IN        | Crochet (INPUT_PULLUP)  |
| ringCmd            | GPIO26    | OUT       | Commande sonnerie       |
| lineEnable         | GPIO25    | OUT       | Activation ligne        |
| led                | GPIO2     | OUT       | Debug LED               |
| I2S BCK (PCM5102)  | GPIO14    | OUT       | I2S0_BCK_OUT            |
| I2S WS  (PCM5102)  | GPIO15    | OUT       | I2S0_WS_OUT             |
| I2S DIN (PCM5102)  | GPIO22    | OUT       | I2S0_DO_OUT             |
| Audio IN (ADC)     | GPIO36    | IN        | ADC1_CH0 (VP), micro    |

- **Aucun conflit de pin détecté** avec cette configuration sur ESP32-DevKitC.
- Les pins I2S sont standards et compatibles PCM5102.
- GPIO36 (ADC1_CH0) est idéal pour l’entrée audio analogique (micro).
- GPIO25/26/27/2 sont libres et utilisés pour la logique RTC.


### Exemple d'initialisation I2S pour ESP32 Audio Kit V2.2 A252 (codec ES8388)

```cpp
#include <driver/i2s.h>

// Configuration I2S pour ES8388 (sortie casque/HP, entrée micro)
const i2s_config_t i2s_config = {
   .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_RX),
   .sample_rate = 16000, // 8k, 16k, 32k, 44.1k, 48k selon besoin
   .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
   .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
   .communication_format = I2S_COMM_FORMAT_I2S_MSB,
   .intr_alloc_flags = 0,
   .dma_buf_count = 8,
   .dma_buf_len = 64,
   .use_apll = false,
   .tx_desc_auto_clear = true,
   .fixed_mclk = 0
};

const i2s_pin_config_t pin_config = {
   .bck_io_num = 27,   // I2S BCK
   .ws_io_num = 25,    // I2S WS
   .data_out_num = 26, // I2S DIN (vers ES8388)
   .data_in_num = 35   // I2S DOUT (depuis ES8388)
};

void setup() {
   // ... autres inits ...
   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
   i2s_set_pin(I2S_NUM_0, &pin_config);
   // Initialisation du codec ES8388 via I2C (voir librairie es8388 ou esp-adf)
}
```

> Pour la gestion complète du codec ES8388 (volume, routing, etc.), utiliser une librairie dédiée comme [ESP-ADF](https://github.com/espressif/esp-adf) ou une librairie Arduino ES8388. L’I2S seul ne suffit pas à configurer le codec : il faut aussi l’initialiser via I2C.

**À ajuster selon ton routage réel et la disponibilité des broches sur ta carte.**

## Exemple de pinout ESP32 Audio Kit V2.2 A252 + SLIC K50835F

> **Remarque :** L'ESP32 Audio Kit V2.2 A252 intègre déjà un codec audio I2S ES8388 (entrée/sortie analogique, ampli casque, micro, etc.). Il n'est donc **pas nécessaire d'ajouter un PCM5102**. Utilise directement les entrées/sorties audio du kit !

| Fonction           | ESP32 Audio Kit V2.2 Pin | Direction | Remarque                                 |
|--------------------|--------------------------|-----------|-------------------------------------------|
| hookSense (SHK)    | GPIO23                   | IN        | Crochet (INPUT_PULLUP, actif haut)        |
| ringCmd (RM)       | GPIO18                   | OUT       | Commande sonnerie                         |
| ringFreq (FR)      | GPIO5                    | OUT       | Modulation sonnerie                       |
| powerDown (PD)     | GPIO19                   | OUT/OD    | Pilotage power-down SLIC                  |
| lineEnable         | non utilisé              | -         | Logique retirée du runtime (`-1`)         |
| ampEnable (AMP_EN) | GPIO21                   | OUT       | Ampli audio, **actif bas** (`LOW=ON`)     |
| I2S BCK            | GPIO27                   | OUT       | I2S0_BCK_OUT (vers ES8388, casque, HP)    |
| I2S WS             | GPIO25                   | OUT       | I2S0_WS_OUT (vers ES8388)                 |
| I2S DIN            | GPIO26                   | OUT       | I2S0_DO_OUT (vers ES8388)                 |
| I2S DOUT           | GPIO35                   | IN        | I2S0_DI_IN (entrée micro ES8388)          |
| Audio IN (ADC)     | GPIO34 (ADC1_CH6)        | IN        | Entrée micro (ADC, jack IN, optionnelle)  |

- **Aucun conflit de pin détecté** avec cette configuration sur ESP32 Audio Kit V2.2 A252.
- Les pins I2S sont câblés d'origine vers le codec ES8388 (sortie casque, HP, entrée micro, etc.).
- GPIO34/36 restent disponibles pour entrées analogiques supplémentaires.
- GPIO21 est réservé à `AMP_EN` (actif bas), ne pas l'utiliser pour `LINE`.

**À ajuster selon ton routage réel et la disponibilité des broches sur ta carte.**
## Options de câblage audio ESP32 <-> SLIC K50835F

### 1. Sortie audio numérique (I2S) via PCM5102

```
   +---------+      I2S      +----------+    Analog    +--------------+
   |  ESP32  |-------------->| PCM5102  |------------->| SLIC K50835F |
   +---------+               +----------+              +--------------+
         |                        |                          |
   (I2S_OUT: BCK, WS, DIN)   (L/R OUT)                (AUDIO IN)
```

### 2. Entrée micro analogique sur ADC ESP32

```
   +--------------+    Analog    +---------+
   | SLIC K50835F |------------>|  ESP32  |
   +--------------+             +---------+
        (AUDIO OUT)         (ADC: ex. GPIO34)
```

### 3. Option : sortie audio sur DAC interne ESP32

```
   +---------+    Analog    +--------------+
   |  ESP32  |------------>| SLIC K50835F |
   +---------+             +--------------+
   (DAC: ex. GPIO25/26)    (AUDIO IN)
```

**Remarques :**
## Structure logicielle minimale (exemple C++/Arduino)

```cpp
// Déclaration d'une classe simple pour piloter le SLIC K50835F
class SlicK50835F {
public:
   void begin();
   void setLineEnabled(bool enabled);
   void setRing(bool enabled);
   bool isHookOff() const;
   bool isLineFault() const;
   // ... autres méthodes selon besoins
};

// Exemple d’utilisation dans setup/loop
SlicK50835F slic;

void setup() {
   slic.begin();
   slic.setLineEnabled(true);
}

void loop() {
   if (slic.isHookOff()) {
      // Gérer appel en cours
   }
   if (slic.isLineFault()) {
      // Sécurité : couper la ligne, alerter
   }
}
```

**À compléter avec :**
## Points d’attention hardware et sécurité (SLIC K50835F)

- **Alimentation** :
   - Utiliser une alimentation stable et filtrée (5V ou 3.3V selon module).
   - Séparer les masses analogique et numérique si possible.
- **Protection ligne** :
   - Ajouter des diodes de protection ESD sur TIP/RING.
   - Prévoir fusible ou résistance de limitation sur l’alimentation de boucle.
- **Découplage** :
   - Condensateurs de découplage proches des broches d’alim du SLIC.
- **Isolation** :
   - Ne jamais connecter à une ligne téléphonique publique sans homologation.
   - Prévoir isolation galvanique si risque de contact avec le réseau public.
- **Routage PCB** :
   - Tracer les lignes audio loin des signaux numériques rapides.
   - Minimiser la longueur des pistes audio et de puissance.
- **Test et validation** :
   - Vérifier l’absence de surchauffe du SLIC et des composants associés.
   - Mesurer les niveaux audio et la tension de boucle avant branchement du téléphone.
- **Sécurité utilisateur** :
   - Boîtier fermé, pas d’accès direct aux parties sous tension.
   - Etiquetage clair si prototype.
## Schéma de connexion typique ESP32 <-> SLIC K50835F

```
   +-------------------+         +---------------------+
   |      ESP32        |         |   SLIC K50835F      |
   |                   |         |                     |
   |  GPIO23 <-------- |---SHK---| > Hook sense        |
   |  GPIO18 --------> |---RM----| < Ring control      |
   |  GPIO5  --------> |---FR----| < Ring modulation   |
   |  GPIO19 --------> |---PD----| < Power down ctrl   |
   |  GPIO21 --------> |--AMP_EN-| (actif bas)         |
   |                   |         |                     |
   |  I2S_OUT ------+  |         |  +-- AUDIO_IN        |
   |                |--|---------|--|                  |
   |  I2S_IN  <----+   |         |  +-- AUDIO_OUT       |
   +-------------------+         +---------------------+
```

**Explications :**
- Les signaux SHK/RM/FR/PD sont à adapter selon le schéma d’application du SLIC K50835F.
- La broche `LINE` n'est pas utilisée dans la config bench validée.
- L’audio analogique transite via un codec I2S (ex : PCM5102, ES8388) entre l’ESP32 et le SLIC K50835F.
- Prévoir adaptation d’impédance et filtrage sur les lignes audio.
- Les broches sont données à titre d’exemple, à ajuster selon le routage réel.

**À compléter avec :**
- Alimentation dédiée 5V/3.3V pour le SLIC K50835F.
- Protections ESD/surtension sur les lignes TIP/RING.
- Masse analogique séparée si possible.
## Fonctionnalités principales à développer autour du SLIC K50835F

1. **Gestion du décroché/raccroché (hook sense)**
   - Lecture de l’état du combiné via GPIO (décroché/raccroché).
   - Déclenchement d’événements firmware (prise d’appel, fin d’appel).

2. **Commande de la sonnerie**
   - Activation/désactivation de la sonnerie via GPIO ou commande dédiée.
   - Scénarios d’appel entrant simulé.

3. **Activation/désactivation de la ligne**
   - Contrôle de l’alimentation de boucle (line enable) pour simuler la présence d’une ligne RTC.

4. **Gestion des états de ligne**
   - Détection d’erreurs (line fault, surintensité, etc.)
   - Monitoring de l’état ligne pour sécurité et diagnostic.

5. **Interface audio**
   - Routage de l’audio analogique entre ESP32 (I2S/codec) et SLIC K50835F.
   - Adaptation d’impédance, filtrage, gestion du gain.

6. **Numérotation et détection DTMF/impulsions**
   - Lecture du clavier (matrice ou impulsions) pour composer un numéro.
   - Décodage DTMF matériel ou logiciel si besoin.

7. **Journalisation et diagnostic**
   - Log des événements (décroché, appel, erreurs ligne, etc.)
   - Statistiques d’utilisation, export série ou stockage local.

8. **Sécurité et protection**
   - Gestion des protections électriques (surintensité, surtension, isolation).
   - Détection et gestion des conditions anormales.
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



### Variante concrète : module **SLIC K50835F**
Si tu veux garder un ancien téléphone tel quel (2 fils TIP/RING) avec une carte ESP32, le SLIC K50835F est une solution moderne côté **FXS/SLIC**.

#### Ce que ça apporte
- Génération de la boucle analogique pour alimenter un poste RTC ancien (fonction FXS complète).
- Gestion de la sonnerie, de la détection de décroché/raccroché, et de la signalisation ligne.
- Intégration facilitée pour créer une "ligne privée" locale (pas de connexion directe PSTN).
- Réduction de la complexité analogique par rapport à un design SLIC discret from-scratch.

#### Comment l'intégrer proprement avec ESP32
1. **SLIC K50835F = étage ligne analogique** (alimentation de boucle, interface 2 fils, gestion ring/hook).
2. **ESP32 = logique et signalisation** (états off-hook/on-hook, numérotation, scénarios, gestion d'appel).
3. **Chemin audio** :
   - via codec audio externe (I2S) connecté entre l'ESP32 et le SLIC K50835F (entrée/sortie audio analogique).
   - prévoir adaptation d'impédance et filtrage si besoin.
4. **Détection d'événements** :
   - Exposer vers GPIO les états utiles (hook, ring detect, line fault, etc.) selon le schéma d'application SLIC K50835F.

#### Points d'attention (importants)
- Respecter strictement les notes d'application du SLIC K50835F (alimentation, découplage, protection, routage PCB).
- Ne jamais connecter à une ligne publique sans conformité réglementaire et schéma adapté.
- Prévoir protections (surintensité/surtension) et isolation selon l'usage final.
- Valider impédance et niveau audio pour éviter faible volume, saturation, écho/larsen.

#### Recommandation projet
- **MVP rapide** : continuer Option A (combiné + clavier en hard) pour avancer sur firmware.
- **Version "vraie ligne 2 fils"** : passer en Option B avec SLICK50835F quand la logique applicative est stable.


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
