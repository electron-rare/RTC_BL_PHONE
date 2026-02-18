# Structuration des tâches FreeRTOS — Agent RTOS

## Objectif
Organiser les tâches principales du firmware pour audio, web, batterie.

---

### Tâches à créer
- TaskAudio : gestion flux audio, lecture MP3, routage
- TaskWeb : gestion serveur HTTP, endpoints, logs
- TaskBattery : surveillance batterie, gestion deep sleep
- TaskBluetooth : gestion HFP, BLE, pairing
- TaskWiFi : gestion connexion, OTA, logs

### Priorités
- TaskAudio : haute priorité (temps réel)
- TaskBattery : priorité moyenne (surveillance périodique)
- TaskWeb, TaskBluetooth, TaskWiFi : priorité basse (événementiel)

### Synchronisation
- Utilisation de queues pour communication inter-tâches
- Mutex pour accès partagé (logs, config)

### Plan d’implémentation
- Créer chaque tâche via xTaskCreate
- Définir stack size, priorité, fonction
- Tester la robustesse (stress, interruption)

---

**Version :** 2026-02-17
