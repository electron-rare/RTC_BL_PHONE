# Fiche agent — Test hardware RTC_BL_PHONE

## Objectif
Décrire le rôle, la méthodologie et la validation de chaque agent de test hardware pour la livraison RTC_BL_PHONE.

---

## 1. Agent Téléphonie
- Valide l’émission/réception d’appels, la détection hook/ring, la gestion DTMF.
- Vérifie la robustesse des transitions d’état (idle, appel, raccroché, décroché).
- Documente les logs et les scénarios d’erreur.

## 2. Agent Audio
- Valide la lecture MP3/WAV, le routage audio, le volume, le mute.
- Vérifie la qualité audio sur haut-parleur et combiné.
- Teste la gestion des erreurs audio (fichier absent, SD non montée).

## 3. Agent Web
- Valide l’accessibilité de l’interface web embarquée.
- Vérifie les endpoints (`/`, `/status`, `/config`, `/logs`).
- Teste la réactivité et la robustesse sous charge.

## 4. Agent MQTT
- Valide la réception/émission de commandes et d’événements via MQTT.
- Vérifie la conformité du schéma JSON, la gestion des erreurs réseau.
- Teste la reconnexion automatique et la persistance des états.

## 5. Agent ESP-NOW
- Valide la réception/émission de commandes et d’événements via ESP-NOW.
- Vérifie le broadcast, la robustesse en cas de perte de pair.

## 6. Agent WiFi
- Valide la connexion/déconnexion, la gestion des coupures, le fallback config.
- Teste la robustesse en cas de reboot ou de perte de réseau.

## 7. Agent SLIC
- Valide la gestion hardware SLIC, la détection de ligne, la signalisation.
- Vérifie la fiabilité sur plusieurs cycles d’appel.

## 8. Agent Energie
- Valide la gestion batterie, le deep sleep, le wakeup.
- Vérifie la robustesse en cas de coupure d’alimentation.

## 9. Agent Logs
- Surveille et collecte tous les logs série, web, MQTT, ESP-NOW.
- Documente les erreurs, warnings, et comportements inattendus.

---

**Synergie agents** :
- Les agents collaborent pour valider les scénarios croisés (ex : appel via web, événement MQTT, coupure WiFi, etc.).
- Chaque agent documente ses résultats dans le rapport de synthèse.

**Version :** 2026-02-18
