# Plan Annuaire Téléphone SFP

## Objectif
Permettre la gestion complète des contacts (nom, numéro, type), CRUD via API et webUI, recherche, logs, sécurité, et synchronisation BLE avec smartphone.

---

### 1. Architecture
- Stockage contacts (RAM, extension possible EEPROM/Flash)
- Endpoints API : /api/contacts (GET, POST, PUT, DELETE), /api/contacts/sync_ble
- Logs des modifications (ajout, modif, suppression)
- Sécurité : validation, contrôle accès, audit
- Intégration webUI : liste, recherche, formulaire, actions

### 2. API
- GET /api/contacts : liste contacts (JSON)
- POST /api/contacts : ajout contact (JSON {nom, numero, type})
- PUT /api/contacts : modification contact (JSON {idx, nom, numero, type})
- DELETE /api/contacts : suppression contact (JSON {idx})
- POST /api/contacts/sync_ble : synchronisation BLE (à compléter)

### 3. Sécurité
- Validation des données (nom, numéro, type)
- Contrôle d’accès (authentification future)
- Logs des modifications (Serial, extension fichier)
- Protection contre injections et erreurs

### 4. Logs
- Ajout, modification, suppression : log Serial (format : action, contact, timestamp)
- Extension possible : logs consultables via /api/logs

### 5. Synchronisation BLE
- Endpoint /api/contacts/sync_ble
- Structure d’échange : JSON contacts
- Plan :
    - Scan BLE smartphone
    - Appairage
    - Envoi/réception contacts
    - Validation et intégration
- Extension : synchronisation bidirectionnelle, logs, sécurité

### 6. Usage webUI
- Section Annuaire : liste stylée, recherche, formulaire CRUD, actions (appel, modif, suppression)
- Feedback utilisateur, logs, sécurité
- Préparation pour synchronisation BLE (bouton sync, feedback)

---

## Version : 2026-02-17
