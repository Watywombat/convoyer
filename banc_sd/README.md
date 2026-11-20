# Essai A/B sur M5Stack Basic v2.7

Profil de banc autonome pour le **Basic v2.7**. Confirmer la référence au dos. Laisser GRBL, moteur, servo et RFID débranchés. La micro-SD interne et les trois boutons suffisent. Le moniteur série USB affiche les résultats. Le premier flash remplace le firmware et la table de partitions présents sur le Core. Sauvegarder le contenu à conserver avant ce flash. Utiliser une carte de développement libre, hors installation client. Aucune opération eFuse.

1. Installer et activer le terminal ESP-IDF 5.4.x sur le PC. Dans CLion, ouvrir le dossier banc_sd et utiliser un terminal qui hérite de cet environnement. Vérifier avec `idf.py --version`. Le compilateur C++ du PC seul ne produit pas d’image ESP32.
2. Dans banc_sd, lancer `python build_pair.py`. Le script construit B, calcule son empreinte, puis construit A qui acceptera uniquement ce fichier B. Il crée sd/firmware.bin. Conserver les deux répertoires de build pour cet essai.
3. Depuis ce même dossier, brancher le Core par USB et lancer `idf.py -B build_A -p PORT flash monitor` en remplaçant PORT par le port réel. Vérifier la ligne « Version A | partition ota_0 ». Si la carte démarre sur une autre image, interrompre l’essai et vérifier l’état de la partition de sélection OTA. Ne pas effacer la flash par défaut.
4. Copier uniquement sd/firmware.bin à la racine d’une micro-SD FAT32, puis insérer la carte. Sur A, appuyer sur le bouton gauche. Attendre « Version B | partition ota_1 » et « B en essai ». Le bouton droit demande le rollback. Attendre le redémarrage et la ligne « Version A | partition ota_0 ».
5. Refaire l’installation avec le bouton gauche. Cette fois appuyer sur le bouton central de B. Redémarrer après « B confirmee ». La carte doit afficher à nouveau B confirmée. Pour tester une interruption de premier démarrage, réinitialiser pendant « B en essai » avant confirmation.
6. Essai de rejet à faire depuis A : copier une version modifiée du fichier sous le même nom. Attendre le diagnostic d’erreur puis vérifier qu’un redémarrage reste sur A. Remettre le fichier exact avant le prochain essai.

Ce banc démontre la sélection de partitions et le retour arrière. L’empreinte de B est intégrée à A. Le profil de production conserve son mécanisme de signature et de démarrage sécurisé. Le banc ne nécessite ni réseau, ni configuration Dolibarr, ni pilote moteur. Le moteur du convoyeur attend toujours un adaptateur GRBL et le lecteur un pilote correspondant à sa variante.

Sources : https://docs.m5stack.com/en/core/Basic_v2.7 et https://docs.espressif.com/projects/esp-idf/en/v5.4/esp32/api-reference/system/ota.html

Le moniteur affiche aussi une sonde I²C sans commande de mouvement (GPIO21/22 du profil Basic). 0x28 correspond à l’adresse attendue du lecteur et 0x70/0x71 aux adresses possibles du GRBL M035. ESP_OK indique un acquittement ; un périphérique absent ne bloque pas le test SD. Cette sonde ne lit pas encore de tag.
