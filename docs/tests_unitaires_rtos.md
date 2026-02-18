# Tests unitaires RTOS — Agent RTOS

## Objectif
Valider la création, la communication et la robustesse des tâches FreeRTOS.

---

### Tests réalisés
- Création de TaskAudio, TaskWeb, TaskBattery, TaskBluetooth, TaskWiFi
- Vérification exécution (logs, status)
- Tests de communication via queues (audio ↔ batterie)
- Tests mutex (logs partagés)
- Tests sémaphores (wake, OTA)
- Tests de stress (charge, interruption)

### Résultats
- Toutes les tâches créées et exécutées
- Communication inter-tâches OK
- Mutex et sémaphores fonctionnels
- Pas de crash sous charge

### Points à améliorer
- Optimiser stack size selon usage
- Ajouter watchdog sur tâches critiques
- Tester interruption longue durée

---

**Version :** 2026-02-17
