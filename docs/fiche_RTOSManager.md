# Fiche technique RTOSManager

## Interface
- Méthodes principales : start(), stop(), createTask(), audit(), getStatus()
- Gestion multitâche, watchdog

## Flux de données
- Entrée : tâches, signaux
- Sortie : logs, états

## Scénarios d’utilisation
- Création de tâches
- Audit du système
- Gestion du watchdog

## Exemple d’intégration
```cpp
RTOSManager rtos;
rtos.start();
rtos.createTask(myTask);
```
