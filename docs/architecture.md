# Architecture

Vue d'ensemble de lilpet et ce qui reste ouvert. Les choix et leurs raisons sont dans les décisions ([adr/](adr/)),
le détail de chaque fichier dans la carte du code (`.claude/codemap.md`).

## Les parties

- **Moteur des yeux** (`engine/`) : C17, état, animations et dessin, sans dépendance ni accès au matériel. Compilé pour
  l'ESP32-S3, en WebAssembly et pour le PC ([décision 1](adr/0001-moteur-c-partage.md)), il dessine ligne par ligne en
  RGB565 ([décision 2](adr/0002-rendu-par-lignes.md)). Ses rendus de référence fixent l'apparence des yeux
  ([décision 3](adr/0003-rendus-de-reference.md)), et ses tests C fixent la logique avec des valeurs écrites en dur.
- **Données** (`eyes/`) : les 100 couleurs officielles, les émotions et les réactions, en JSON. Les tables C du moteur
  en sont générées (`npm run tables`), et l'app en lit les noms : les looks et les animations restent des données, que
  l'app PC éditera.
- **Firmware** (`firmware/`) : ESP-IDF, une image par carte ([décision 6](adr/0006-une-image-par-carte.md)), le rendu
  sur les deux cœurs ([décision 9](adr/0009-rendu-deux-coeurs.md)). Détail et mesures : [firmware.md](firmware.md).
- **App PC** (`app/`) : une page web qui fait tourner le moteur WebAssembly et parle à l'objet par Web Serial
  ([décision 4](adr/0004-app-web-serial.md)). Spec : [app.md](app.md).
- **Protocole** : des lignes de texte, les mêmes sur l'USB et l'UART ([décision 5](adr/0005-protocole-texte.md)).
  Détail : [protocole.md](protocole.md).
- **Vision** (`vision/`, créé à l'achat de la XIAO) : la carte caméra et son modèle, reliés à la carte écran par UART ;
  le modèle décide, le moteur exécute ([décision 7](adr/0007-deux-cartes-uart.md)). Matériel :
  [materiel.md](materiel.md).
- **Outillage** : les mêmes outils aux mêmes versions en local et en CI ([décision 8](adr/0008-outils-figes.md)).
  CMake et Ninja avec des presets (`host-debug` avec AddressSanitizer et UBSan, `host-release`, `wasm`), clang-format
  et clang-tidy pour le C, avertissements traités comme des erreurs, un mini-lanceur de tests sans dépendance
  (`engine/tests/test.h`) ; côté JS, Prettier et oxlint ; des hooks git et une carte du code générée, vérifiée en CI.

## Décisions

1. [Un seul moteur des yeux, en C](adr/0001-moteur-c-partage.md)
2. [Dessin ligne par ligne, en RGB565, sans bibliothèque graphique](adr/0002-rendu-par-lignes.md)
3. [Rendus de référence tirés du build WebAssembly](adr/0003-rendus-de-reference.md)
4. [App PC : une page web qui parle à l'objet par Web Serial](adr/0004-app-web-serial.md)
5. [Un protocole en lignes de texte, le même sur l'USB et l'UART](adr/0005-protocole-texte.md)
6. [Une image de firmware par carte, une carte par dossier](adr/0006-une-image-par-carte.md)
7. [Deux cartes reliées en UART : le modèle décide, le moteur exécute](adr/0007-deux-cartes-uart.md)
8. [Les mêmes outils, aux mêmes versions, en local et en CI](adr/0008-outils-figes.md)
9. [Le rendu sur les deux cœurs de la puce, depuis la mémoire interne](adr/0009-rendu-deux-coeurs.md)

Une nouvelle décision prend le numéro suivant ; une décision remplacée garde son fichier, avec le statut « remplacée
par » et le lien vers la nouvelle.

## Points ouverts

Les audits de novembre 2026 n'ont trouvé ni crash, ni corruption mémoire, ni perte de données (118 400 images et
3 millions de lignes de commande au hasard). Il reste :

À mesurer ou à essayer sur du vrai matériel :

- **Dessin à 466 px encore un peu lent pour 60 images/s** : 19,3 ms par image sur les deux cœurs
  ([mesures](firmware.md#sur-la-puce)), il manque environ 3 ms pour tenir 16,7 ms. Pistes : accélérer le rendu
  lui-même (`draw_row`), et essayer le QSPI à 80 MHz sur la carte (moins d'attente sur la dernière bande). Sur le
  Waveshare, chaque image attend le signal TE suivant : si l'écran rafraîchit à 60 Hz (à vérifier), une image de 19 ms
  tombe un rafraîchissement sur deux, donc 30 images/s stables, et 60 seulement sous 16,7 ms.
- **Waveshare 1,32"** : pilote CO5300 et signal TE jamais essayés sur la carte. La démo de Waveshare définit la broche
  TE sans s'en servir.
- **AtomS3R à ST7735S** : cette variante d'écran n'a jamais tourné.
- **App et vraie carte dans Chrome** : les pannes de port (écriture morte, réponse restée en attente) sont prouvées sur
  un faux port seulement.
- **Renovate** : réactivé le 9 novembre 2026 sur le dépôt recréé, il n'a encore ouvert aucune pull request (il passe le
  lundi matin). Vérifier qu'il suit les `tools` des tâches de `mise.toml` et `scripts/requirements.txt`.

À décider ou à construire pour l'objet final :

- **Mouvement et toucher** : le 1,32" n'a pas de capteur de mouvement (`imu_none.c`), et son bouton n'est pas celui de
  l'AtomS3R. Sans module ajouté, pas d'inclinaison ni de vertige ; le toucher demande un pilote pour le CST820.
- **Modèle** : aucun modèle ESP-DL listé dans [materiel.md](materiel.md) ne lit les expressions du visage.
- **Carte vision** : le projet `vision/` et sa tâche UART, qui devra répondre sur son propre flux, avec un verrou autour
  du look courant, et répondre à `> hello` ([décision 6](adr/0006-une-image-par-carte.md)).

Limites connues, à reprendre quand elles gêneront :

- L'app associe une réponse à une demande par ordre d'arrivée : le protocole n'a pas d'identifiants.
- Le WebAssembly garde une seule instance d'yeux, ce que l'éditeur devra changer.
- `EyeGlyph` recopie les constantes de pupilles du moteur.
- Les lignes de particules parcourent toute leur boîte : les limiter aux colonnes touchées déplaçait un pixel.
