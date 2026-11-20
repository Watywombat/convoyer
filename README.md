# The Conveyor · dossier de revue 1.1.0-poc

**Verdict.** La livraison Gemini ne pouvait pas trier un colis réel. Cette révision fournit une chaîne C++ testable, des adaptateurs ESP-IDF, un module Dolibarr et un protocole de recette. Son statut est « POC à intégrer ». Le dossier de mise en production reste ouvert.

| Attente du client | Dans le ZIP initial | Dans cette révision et preuve disponible |
|---|---|---|
| Automate maître du cycle | 8 états, arrivée fictive et reprise vide | Table de transitions, contrôle d’événements et journal NVS. Cycle nominal, erreurs et matrice testés sur PC. |
| NFC et trois destinations | Référence constante, sortie 1 imposée | Lecture UART encadrée et association explicite de trois identifiants WMS. Format et branchements à confirmer. |
| Disponibilité et mouvement | Aucun appel HTTP | Recherche par référence, lecture de capacité, création de mouvement, signalement. Module PHP fourni. Intégration non exécutée. |
| Boucle physique | Le temps écoulé vaut succès | Position de porte confirmée et détection de sortie indépendante. Proposition et critères dans la feuille de route. |
| Coupure et panne réseau | Effacement implicite après 5 s | Écriture avant action, arrêt sur résultat incertain, acquittement physique après réconciliation. Coupures à provoquer sur banc. |
| Configuration et sécurité | JSON codé en dur, URL HTTP, jeton fictif | Configuration SD signée et vérifiée au redémarrage, accès WMS HTTPS. Provisionnement des clés et confidentialité physique à qualifier. |
| Santé et surveillance | Compteur de tours, hash omis, pas de date | Temps monotone, empreinte de partition, horodatage relatif et watchdog configurable. Mesures sur cible à faire. |
| Mise à jour et release | Absentes | Installation SD vers partition inactive, protection conditionnée au démarrage sécurisé et essai avec retour arrière. Binaire signé, SHA de distribution et démonstration encore requis. |
| Qualité et dossier | Assertion toujours vraie, aucun document | Tests exécutables, diagrammes, guide opérateur, cartographie des tâches et feuille de recette. |

**Par où commencer.** Lire [la feuille de route et la recette](docs/recette.md), puis [le guide exploitation](docs/exploitation.md). Le [diagramme de l’automate](docs/automate.md) suit la table de transition du programme. Les diagrammes sont en Mermaid, visualisables dans un lecteur Markdown compatible. La [note de version](RELEASE.md) distingue les essais effectués des preuves à obtenir.

### Reproduire les vérifications

À la racine du projet, lancer `sh tools/test.sh` avec un compilateur C++17. Le test emploie le même en-tête que la cible. Pour la cible, installer PlatformIO avec accès aux paquets Espressif puis lancer `pio run -e controller`. La cible est celle déclarée par Gemini. En l’absence de profil de brochage, l’application reste en récupération. Aucun numéro de broche n’a été supposé. Le ZIP ne contient aucune clé privée.

### Organisation

- `include/conveyor.hpp` définit événements, décisions et erreurs portables.
- `src/main.cpp` orchestre tâches, santé, échéances et reprise.
- `src/carrier.cpp` traduit actions et signaux matériels. `docs/brochage.hpp` décrit le contrat à renseigner.
- `src/station.cpp` valide la configuration, conserve le journal et écrit les images OTA.
- `src/wms.cpp` effectue les échanges HTTPS et valide les réponses. `dolibarr/custom/conveyor` ajoute les deux opérations spécifiques au client.
