# Plan d’architecture WebUI RTC_BL_PHONE

## Objectif
Créer une interface web embarquée (WebUI) pour le monitoring, la configuration et le contrôle du téléphone RTC.

## Structure
- Dossier : src/webui/
- Fichiers : index.html, style.css, script.js
- Ressources statiques servies par ESPAsyncWebServer

## Fonctionnalités
- Dashboard (état, batterie, audio, SLIC, Bluetooth, WiFi)
- Configuration (paramètres réseau, audio, RTC, firmware)
- Logs et monitoring en temps réel
- Contrôle (appels, lecture audio, reboot, sleep)

## Intégration
- Endpoints HTTP pour données dynamiques (JSON)
- Serveur de fichiers statiques (HTML/CSS/JS)
- Sécurité : authentification, audit endpoints

## Tests & CI
- Tests fonctionnels frontend (affichage, interaction)
- CI : validation build webUI, tests automatisés

---

_Agent WebUI – Plan généré automatiquement._
