# RTC_BL_PHONE

Projet ESP32 : téléphone RTC, SLIC, audio, Bluetooth, WiFi, agentic.

## CI/CD automatisé
Le pipeline CI/CD est géré par GitHub Actions + PlatformIO.

- **Déclenchement** : à chaque push ou pull request sur `main` ou `release/stable`.
- **Gate de validation** : exécution du script `scripts/branch_gate.sh` (ordre de checks unifié).
- **Vérifications** : tests Python/unit, tests hôte DTMF, parity WebUI/commandes, puis builds des cibles actives.
- **Artefact** : `artifacts/route_parity_report.json` généré par la gate.

### Gate de branche (qualité)
Le script de référence est `scripts/branch_gate.sh` (définition dans [docs/branch_quality_gate.md](docs/branch_quality_gate.md)).
Commande locale recommandée :

```bash
bash scripts/branch_gate.sh
```

Exécuter uniquement les checks sans build (ex. pour un contrôle rapide local) :

```bash
bash scripts/test_terminal.sh
```

## Intégration A252 (réintégration firmware)

Repérage du port série (A252: USB-Serial prioritaire) :

```bash
python3 scripts/diagnostic_esp_ports.py
```

Commande recommandée de réintégration sur la carte A252 (USB-Serial) :

Si le port USB‑Serial est bien branché, `hw_validation` le détecte automatiquement:

```bash
python3 scripts/hw_validation.py \
  --flash \
  --wifi-ssid "<SSID>" \
  --wifi-password "<WIFIPASS>" \
  --report-json artifacts/hw_validation_a252_report.json \
  --report-md docs/hw_validation_a252_report.md
```

Ou en mode explicite:

```bash
python3 scripts/hw_validation.py \
  --port-a252 /dev/cu.usbserial-0001 \
  --flash \
  --wifi-ssid "<SSID>" \
  --wifi-password "<WIFIPASS>" \
  --report-json artifacts/hw_validation_a252_report.json \
  --report-md docs/hw_validation_a252_report.md
```

Version minimal sans essais Wi‑Fi (si pas de réseau test) :

```bash
python3 scripts/hw_validation.py \
  --port-a252 /dev/cu.usbserial-0001 \
  --flash \
  --report-json artifacts/hw_validation_a252_report.json \
  --report-md docs/hw_validation_a252_report.md
```

Si le firmware expose une IP après connexion Wi-Fi, les endpoints HTTP sont testés automatiquement. Sinon le scénario HTTP est marqué `MANUAL_SKIP`.

## Livraison
Après chaque build, les binaires sont disponibles en téléchargement dans les artefacts du workflow.

### Tests
Les tests sont lancés automatiquement à chaque commit dans `.github/workflows/ci.yml`.

### Références
- [PlatformIO CI Docs](https://docs.platformio.org/en/latest/ci/index.html)
- [GitHub Actions Docs](https://docs.github.com/en/actions)

## Notifications CI/CD
- Le pipeline CI/CD envoie des notifications sur les statuts (succès, échec) via GitHub Actions.
- Possibilité d’ajouter des notifications Slack ou email (voir .github/workflows/ci.yml).

---

_Agent Repo & GitHub – README généré automatiquement._

## Démarrage rapide
1. Ouvrir le dossier dans PlatformIO.
2. Option A: renseigner l'adresse MAC dans `src/main.cpp` (`DEFAULT_PEER_ADDR`).
3. Option B: la définir au runtime avec la commande série `p <mac>`.
4. Compiler et flasher l'environnement `esp32dev` (par défaut).
5. Ouvrir le moniteur série à 115200 bauds.
6. Connecter puis piloter les appels via commandes série.

## Orchestration ZeroClaw (préflight + agent)

- Guide: `docs/zeroclaw_orchestration.md`
- Préflight hardware avant upload:
  - `python3 scripts/zeroclaw_hw_preflight.py --require-port`
- Conversation agent ciblée RTC (depuis `Kill_LIFE`):
  - `tools/ai/zeroclaw_dual_chat.sh rtc -m "fais un état hardware et propose 3 actions"`

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

## Wiring A252 validé (bench courant)
- `SLIC RM` -> `GPIO18`
- `SLIC FR` -> `GPIO5`
- `SLIC SHK` -> `GPIO23` (`INPUT_PULLUP`, hook actif haut)
- `SLIC PD` -> `GPIO19`
- `SLIC LINE` -> non utilisé (`-1`, logique retirée du runtime)
- `AMP_EN` carte audio -> `GPIO21`, polarité active bas (`LOW=ON`, `HIGH=OFF`)
- tonalité locale: `425 Hz` (couleur France/Europe)

## Choix de cartes ESP32
Voir `docs/solutions_rtc_phone_esp32.md` pour la shortlist des DevKit utilisables (ESP32-DevKitC, ESP32-S3-DevKitC-1, NodeMCU-32S, LOLIN32), les liens de référence web, et les solutions d’interface (direct combiné/clavier, SLIC/FXS, ATA externe), dont une variante AG1171S (Silvertel).

## Plan projet (chef de projet)
Voir `docs/plan_chef_projet_esp32s3_ag1171s.md` pour le planning en phases, les risques, les critères d'acceptation et les livrables de la version ESP32-S3 + AG1171S.

## Audio embarqué et lecture MP3

### Librairie Audio Tools
Le projet intègre la librairie [Audio Tools](https://github.com/pschatzmann/arduino-audio-tools) pour la lecture MP3/WAV sur ESP32 via I2S (PCM5102, ES8388, DAC interne).

### Exemple d'utilisation
Lecture automatique d'un fichier MP3 sur carte SD (voir `src/AudioFilePlayer.h/.cpp` et intégration dans `main.cpp`) :

```cpp
#include <AudioFilePlayer.h>

AudioFilePlayer audioFilePlayer;

void setup() {
	Serial.begin(115200);
	if (audioFilePlayer.begin()) {
		audioFilePlayer.play("/test.mp3");
	}
}

void loop() {
	audioFilePlayer.loop();
}
```

### Validation
- Test lecture MP3 sur hardware ESP32 (SD, I2S, codec)
- Routage audio, volume, mute
- Logs série pour débogage

Voir aussi la fiche agent : `docs/fiche_agent_audio_tools.md`

## Arborescence du projet (2026)

```
src/
	main.cpp
	AudioCodec.cpp/h
	AudioFilePlayer.cpp/h
	bluetooth/
		BluetoothManager.cpp/h
	wifi/
		WifiManager.cpp/h
	web/
		WebServerManager.cpp/h
	rtos/
		RTOSManager.cpp/h
	power/
		PowerManager.cpp/h
```

## Stacks embarquées

---

## Documentation technique des modules principaux

### 1. AudioManager
**Fichiers** : src/audio/AudioManager.cpp, src/audio/AudioManager.h

#### Interfaces
- `AudioManager` expose des méthodes pour l'initialisation, la gestion des flux audio, le contrôle du volume, et la sélection des sources.
- Interface principale :
	- `init()` : initialise le module audio
	- `start()` / `stop()` : démarre ou arrête le flux audio
	- `setVolume(int level)` : ajuste le volume
	- `selectSource(AudioSource src)` : sélectionne la source (micro, fichier, etc.)

#### Flux de données
- Entrées : sources audio (microphone, fichiers, Bluetooth)
- Traitement : conversion, mixage, contrôle du volume
- Sorties : haut-parleur, enregistrement, transmission (Bluetooth, Web)

#### Scénarios d’utilisation
- Lecture audio locale
- Streaming Bluetooth
- Enregistrement et restitution

#### Exemple d’intégration
```cpp
#include "audio/AudioManager.h"
AudioManager audio;
audio.init();
audio.selectSource(AudioSource::MIC);
audio.setVolume(80);
audio.start();
```

---

### 2. RTOSManager
**Fichiers** : src/rtos/RTOSManager.cpp, src/rtos/RTOSManager.h

#### Interfaces
- Gestion des tâches, synchronisation, timers.
- Interface principale :
	- `createTask(void (*taskFunc)(void*), const char* name)` : création de tâche
	- `startScheduler()` : démarrage du scheduler
	- `delay(uint32_t ms)` : temporisation

#### Flux de données
- Entrées : fonctions de tâches, signaux d’événements
- Traitement : planification, synchronisation, gestion des priorités
- Sorties : exécution des tâches, notifications

#### Scénarios d’utilisation
- Multitâche (audio, Bluetooth, web, etc.)
- Synchronisation entre modules
- Gestion des timers pour actions périodiques

#### Exemple d’intégration
```cpp
#include "rtos/RTOSManager.h"
RTOSManager rtos;
rtos.createTask(audioTask, "AudioTask");
rtos.startScheduler();
```

---

### 3. BluetoothManager
**Fichiers** : src/bluetooth/BluetoothManager.cpp, src/bluetooth/BluetoothManager.h

#### Interfaces
- Gestion du Bluetooth (connexion, transmission, réception)
- Interface principale :
	- `init()` : initialise le module Bluetooth
	- `connect(const char* device)` : connexion à un périphérique
	- `sendData(const uint8_t* data, size_t len)` : envoi de données
	- `onReceive(void (*callback)(const uint8_t*, size_t))` : callback de réception

#### Flux de données
- Entrées : commandes de connexion, données à transmettre
- Traitement : gestion du protocole, encodage, sécurité
- Sorties : données reçues, notifications d’état

#### Scénarios d’utilisation
- Streaming audio via Bluetooth
- Commandes distantes
- Synchronisation avec smartphone ou périphérique externe

#### Exemple d’intégration
```cpp
#include "bluetooth/BluetoothManager.h"
BluetoothManager bt;
bt.init();
bt.connect("DeviceName");
bt.sendData(buffer, length);
```

---

## Contrôle MQTT, ESP-NOW et DTMF logiciel

- Contrôle distant via MQTT (ArduinoProps) : topics `rtc_bl_phone/<device_id>/in` (commandes), `rtc_bl_phone/<device_id>/out` (événements).
- Contrôle local via ESP-NOW (même schéma JSON).
- Détection DTMF logicielle (Goertzel) : les chiffres détectés sont publiés dans les événements.
- Limitations ESP32-S3 : pas de Bluetooth Classic, uniquement BLE (les fonctions BT Classic sont désactivées sur S3).

**Exemples** :
- Publier une commande MQTT :
  `mosquitto_pub -t rtc_bl_phone/mondevice/in -m '{"cmd":"CALL"}'`
- Écouter les événements :
  `mosquitto_sub -t rtc_bl_phone/mondevice/out`

Voir aussi `docs/props.md` pour le schéma détaillé.

## Résumé des fichiers modifiés/créés
- README.md : ajout de la documentation technique détaillée des modules AudioManager, RTOSManager, BluetoothManager.
  
Pour une documentation approfondie, voir aussi les fichiers dans `docs/` (fiche_agent_audio_tools.md, fiche_agent_embarque_stack.md).

Voir la fiche agent : `docs/fiche_agent_embarque_stack.md`

## Tests unitaires et robustesse RTC_BL_PHONE

### Couverture de code

Pour générer le rapport de couverture :

```bash
bash scripts/gen_coverage.sh
```

Le rapport HTML sera disponible dans `coverage/html`.

### Types de tests ajoutés
- Tests de stress (boucles intensives)
- Edge cases (cas limites)
- Tests de gestion mémoire (allocation/libération)
- Tests de thread safety (multithreading)
- Tests d’interaction entre modules (ex : AudioManager ↔ BluetoothManager)

### Fichiers de tests modifiés
- test/test_audio_codec.cpp
- test/test_audio_file_player.cpp
- test/test_AudioManager.cpp
- test/test_LectureAudioManager.cpp
- test/test_SLICManager.cpp
- test/test_TelephoneSFPManager.cpp

### Script de couverture
- scripts/gen_coverage.sh

### Exécution
Lancez les tests avec PlatformIO :

```bash
bash scripts/test_terminal.sh
```

Ce script enchaîne:
- Build des tests embarqués sans upload matériel.
- Tests hôte DTMF (génération de tonalités synthétiques) en terminal.

Puis, si besoin, générez le rapport de couverture.

### Objectif
Ces ajouts permettent de valider la robustesse, la gestion mémoire, la sécurité multithread et les interactions entre modules, tout en mesurant la couverture des tests.
