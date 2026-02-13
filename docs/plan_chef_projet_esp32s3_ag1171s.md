# Plan chef de projet — ESP32-S3 + AG1171S (Silvertel)

## 1) Objectif projet
Livrer un prototype fonctionnel "téléphone analogique ancien" piloté par ESP32-S3, avec interface 2 fils privée via AG1171S, sans connexion PSTN publique.

## 2) Périmètre (version V0.1)
- Détection hook (on/off hook).
- Commande d'activation ligne (line enable).
- Commande de sonnerie (ring command/cadence côté analogique).
- Journal série + machine d'états téléphonie basique.
- Cible matérielle principale: **ESP32-S3-DevKitC-1**.

## 3) Planning macro
### Phase A — Cadrage & schéma (Semaine 1)
- Valider broches AG1171S retenues (hook detect, ring control, line feed control).
- Produire schéma de câblage V0.1 (alims, masses, protections, découplage).
- Sortie: checklist sécurité + BOM V0.1.

### Phase B — Firmware fondation (Semaine 2)
- Mettre en place environnement PlatformIO `esp32-s3-devkitc-1`.
- Implémenter machine d'états: `ON_HOOK`, `OFF_HOOK`, `DIALING`, `IN_CALL`, `RINGING`.
- Ajouter commandes série pour tests rapides (`r/o/d/c`).
- Sortie: build firmware de base + logs validés.

### Phase C — Intégration hardware (Semaine 3)
- Câblage réel ESP32-S3 ↔ AG1171S.
- Test de robustesse hook (anti-rebond + stabilité).
- Test commande ring/line sans charge puis avec téléphone analogique.
- Sortie: matrice de tests V0.1 avec résultats.

### Phase D — Validation système (Semaine 4)
- Campagne tests end-to-end: raccroché/décroché, sonnerie, transitions d'état.
- Vérifier alimentation et thermique.
- Corriger pinout/temporisations selon résultats.
- Sortie: release candidate V0.1.

## 4) Risques & mitigation
- **Risque analogique (niveaux/impédance):** prévoir test progressif et instrumentation (oscillo, multimètre).
- **Risque sécurité téléphonie:** rester hors PSTN publique tant que conformité non traitée.
- **Risque planning:** découper firmware et hardware en jalons testables hebdomadaires.

## 5) Critères d'acceptation V0.1
- Boot stable sur ESP32-S3.
- Hook detect stable (> 100 transitions sans faux positif en test de banc).
- Commande `RINGING` active le signal de commande attendu.
- Retour `ON_HOOK` coupe ligne et ring command.

## 6) Suite (V0.2+)
- Ajout décodage numérotation (DTMF/pulse).
- Ajout audio (codec I2S) et scénarios d'appel avancés.
- Option passerelle SIP locale.
