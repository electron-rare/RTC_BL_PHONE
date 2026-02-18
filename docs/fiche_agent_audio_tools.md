# Fiche d’ajout agent — Librairie Audio Tools (ESP32)

## Objectif
Permettre l’intégration, la gestion et la validation de la librairie Audio Tools pour lecture MP3 et audio avancé sur ESP32 via PlatformIO.

---

### 1. Ajout dans platformio.ini
- Ajouter dans la section `[env:esp32dev]` :
```
lib_deps =
    https://github.com/pschatzmann/arduino-audio-tools.git
```

### 2. Installation automatique
- Lancer `pio run` pour télécharger et compiler la librairie.
- Vérifier l’absence d’erreurs de compilation.

### 3. Exemple d’utilisation (MP3)
```cpp
#include <AudioTools.h>
#include <AudioLibs/AudioSourceMP3.h>
#include <SD.h>

AudioSourceMP3 mp3;
AudioOutputI2S i2s;
AudioPlayer player(mp3, i2s);

void setup() {
  SD.begin();
  player.begin();
  player.play("/test.mp3");
}

void loop() {
  player.loop();
}
```

### 4. Points de validation
- Test lecture MP3 sur codec I2S (PCM5102, ES8388, DAC interne).
- Vérifier routage audio, volume, mute.
- Logs série pour débogage.

### 5. Documentation
- Référencer : https://github.com/pschatzmann/arduino-audio-tools
- Ajouter fiche dans docs/ ou .github/agents/

---

**Agent Audio embarqué** :
- Surveille l’intégration, la compatibilité hardware, la validation fonctionnelle.
- Documente les tests, les limitations, les bugs.

**Agent PlatformIO** :
- Garantit la cohérence du fichier platformio.ini.
- Valide l’installation et la compilation.

**Agent Firmware** :
- Intègre l’exemple dans le firmware.
- Ajoute tests unitaires/mock si besoin.

---

**Synergie agents** :
- Audio ↔ Firmware ↔ PlatformIO ↔ Documentation
- Validation croisée hardware/logiciel.

---

**Version :** 2026-02-17
