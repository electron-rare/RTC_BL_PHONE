## Plan : Roadmap hardware/firmware priorisée par plateforme

### 1. ESP32
- **Build/Flash** : Scripts et gates déjà factorisés, structure conforme. Priorité : maintenir la reproductibilité, surveiller les évolutions PlatformIO.
- **Tests/Smoke** : Vérifier la couverture des tests hardware (smoke, reboot, panic). Ajouter des tests de drivers spécifiques si besoin (UART, I2C, SPI, GPIO, SLIC).
- **Drivers** : S’assurer que tous les capteurs/actuateurs nécessaires sont intégrés et documentés. Priorité aux drivers critiques (communication, sécurité, alimentation).
- **CI/CD** : Maintenir la compatibilité avec la CI, artefacts reproductibles, logs clairs.


### 4. Codex/Auto-fix
- **Prompts** : Centraliser et documenter tous les prompts dans codex_prompts/.
- **Intégration** : S’assurer que l’auto-fix fonctionne sur toutes les plateformes, et que les logs/artefacts sont bien générés.
- **Tests** : Ajouter des scénarios de test auto-fix pour chaque plateforme.

### 5. Commun (logs, artefacts, onboarding)
- **Logs/Artefacts** : Maintenir la centralisation, la rotation, et la clarté des logs/artefacts pour chaque plateforme.
- **Onboarding** : Adapter les instructions pour chaque cible
- **CI** : Vérifier que chaque plateforme est bien couverte par la CI (build, smoke, artefacts, logs).

---

**Décisions**
- Priorité à la robustesse ESP32 (build, drivers, tests), p
- Harmonisation et documentation continue pour chaque plateforme.
- Audit et automatisation réguliers pour garantir la conformité AGENTS.md.