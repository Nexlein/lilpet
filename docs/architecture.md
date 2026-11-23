# Architecture

Vue d'ensemble de lilpet et ce qui reste ouvert. Les choix et leurs raisons sont dans les décisions ([adr/](adr/)),
le détail de chaque fichier dans la carte du code (`.claude/codemap.md`), ce que doit faire le premier objet fini dans
[objet-v1.md](objet-v1.md).

## Les parties

- **Moteur des yeux** (`engine/`) : C17, état, animations et dessin, sans dépendance ni accès au matériel. Compilé pour
  l'ESP32-S3, en WebAssembly et pour le PC ([décision 1](adr/0001-moteur-c-partage.md)), il dessine ligne par ligne en
  RGB565 ([décision 2](adr/0002-rendu-par-lignes.md)). Ses rendus de référence fixent l'apparence des yeux
  ([décision 3](adr/0003-rendus-de-reference.md)), et ses tests C fixent la logique avec des valeurs écrites en dur.
- **Données** (`eyes/`) : les 100 couleurs officielles, le look par défaut, la forme de l'œil et des pupilles, les
  émotions et les réactions, en JSON. Les tables C du moteur en sont générées (`npm run tables`), et l'app y lit les
  couleurs, les noms, le look par défaut et les formes qu'elle dessine : les looks et les animations restent des
  données, que l'app PC éditera.
- **Firmware** (`firmware/`) : ESP-IDF, une image par carte ([décision 6](adr/0006-une-image-par-carte.md)), le rendu
  sur les deux cœurs ([décision 9](adr/0009-rendu-deux-coeurs.md)), la veille ([décision 12](adr/0012-energie.md)).
  Détail et mesures : [firmware.md](firmware.md).
- **App PC** (`app/`) : une page web qui fait tourner le moteur WebAssembly et parle à l'objet par Web Serial
  ([décision 4](adr/0004-app-web-serial.md)). Spec : [app.md](app.md).
- **Protocole** : des lignes de texte, les mêmes sur l'USB et l'UART ([décision 5](adr/0005-protocole-texte.md)).
  Détail : [protocole.md](protocole.md).
- **Vision** (`vision/`, créé à l'achat de la XIAO) : la carte vision, sa caméra et son modèle, reliés à la carte
  écran par UART ; le modèle décide, le moteur exécute ([décision 7](adr/0007-deux-cartes-uart.md)). Matériel :
  [materiel.md](materiel.md).
- **Outillage** : les mêmes outils aux mêmes versions en local et en CI ([décision 8](adr/0008-outils-figes.md)). CMake
  et Ninja avec des presets (`host-debug` avec AddressSanitizer et UBSan, `host-release`, `wasm`), clang-format et
  clang-tidy pour le C, avertissements traités comme des erreurs, un mini-lanceur de tests sans dépendance
  (`engine/tests/test.h`), qui fait aussi tourner les parties pures du firmware (`firmware/tests`) ; côté TypeScript,
  sans JavaScript ni build ([décision 13](adr/0013-typescript-sans-build.md)), tsc, Prettier et oxlint ; des hooks git
  et une carte du code générée, vérifiée en CI.

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
10. [Linux d'abord ; sous Windows, le code dans WSL et les cartes en natif](adr/0010-plateformes-de-developpement.md)
11. [La NVS reste à sa place, et la flash se remplit après elle](adr/0011-disposition-de-la-flash.md)
12. [Les yeux s'endorment, et la veille légère attend que le PC soit débranché](adr/0012-energie.md)
13. [Les scripts en TypeScript, que Node lance tel quel](adr/0013-typescript-sans-build.md)

Chaque décision tient en quatre parties : contexte, options étudiées (s'il y en a eu), décision, conséquences. Une
nouvelle décision prend le numéro suivant ; une décision remplacée garde son fichier, avec le statut « remplacée par »
et le lien vers la nouvelle.

## Points ouverts

Les audits de novembre 2026 n'ont trouvé ni crash, ni corruption mémoire, ni perte de données (118 400 images et
3 millions de lignes de commande au hasard). Ce qui reste :

### À mesurer ou à essayer sur du vrai matériel

- **Images à 466 px plus longues que 16,7 ms au pire** : depuis le 11 novembre 2026, une image prend 15,6 ms en
  moyenne, sous les 16,7 ms de 60 images/s, mais 21,4 ms au pire, où l'envoi limite
  ([mesures](firmware.md#sur-la-puce)). Pistes : faire le travail de l'image suivante pendant l'envoi de la dernière
  bande, essayer le QSPI à 80 MHz sur la carte (les broches 11 et 12 ne le permettent peut-être pas), et pour le
  dessin, `sqrtf`, appelé en flash à chaque sous-ligne d'une ellipse (une copie en IRAM, ou une racine plus rapide).
  Sur le Waveshare, chaque image attend le signal TE suivant, et l'attente oublie celui venu pendant l'image : si
  l'écran rafraîchit à 60 Hz (à vérifier), une image de plus de 16,7 ms attend un rafraîchissement de plus. À corriger
  et régler avec la carte.
- **Waveshare 1,75"**, la carte écran de l'objet ([materiel.md](materiel.md)) : pilote CO5300, sa mise en veille et
  signal TE jamais essayés sur la carte. Le pilote a été écrit pour la 1,32", dont la
  [démo de Waveshare](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.32/blob/main/Example/ESP-IDF/07_LVGL_V9_Test/components/lvgl_port_bsp/lvgl_port_bsp.cpp)
  définit la broche TE sans s'en servir.
- **AtomS3R à ST7735S** : cette variante d'écran n'a jamais tourné.
- **App et vraie carte dans Chrome** : les pannes de port (écriture morte, réponse restée en attente) sont prouvées sur
  un faux port seulement.
- **Renovate pour tout** : depuis le 2026-11-23, il gère aussi npm, les images Docker et les actions, à la place de
  Dependabot ([décision 8](adr/0008-outils-figes.md)). Son passage sur les outils de `mise.toml` a fait ses preuves
  (#90) ; sur ces paquets-là, sa première pull request n'est pas encore passée.

### À décider ou à construire pour l'objet final

- **Mouvement, toucher et alimentation** : la 1,75" a un accéléromètre QMI8658, un toucher CST9217, un extenseur
  TCA9554 et une puce d'alimentation AXP2101 ([materiel.md](materiel.md)). Son dossier de carte, `waveshare_175`, règle
  déjà la charge et lit la batterie par l'AXP2101 ; les autres pilotes, le bouton et l'arrêt restent à écrire
  ([#54](https://github.com/Nexlein/lilpet/issues/54)).
- **Modèle** : la v1 n'a besoin que du visage et du pouce levé, que les modèles ESP-DL savent faire. Aucun ne lit les
  expressions du visage : leurs réactions attendent un modèle.
- **Carte vision** : le projet `vision/`, dont le firmware devra répondre à `> hello`
  ([décision 6](adr/0006-une-image-par-carte.md)) ; côté carte écran, la tâche qui lira l'UART
  ([protocole](protocole.md#côté-firmware)).

### Limites connues, à reprendre quand elles gêneront

- Le protocole n'a pas d'identifiants : l'app associe une réponse à une demande par ordre d'arrivée. Elle écarte une
  réponse d'un autre genre (un `hello` arrivé après son délai, pris sinon pour la réponse d'un look), mais deux
  réponses en retard du même genre peuvent encore s'échanger.
- Le WebAssembly garde une seule instance d'yeux, ce que l'éditeur devra changer.
- `lp_eyes_set_shape` accepte toute inclinaison : la rotation des émotions (jusqu'à -1,2 rad) a été réglée pour
  l'inclinaison mesurée (0,37 rad), et au-delà un œil peut tourner de plus d'un quart de tour. L'éditeur des yeux devra
  la borner.
- Les lignes de particules parcourent toute leur boîte : les limiter aux colonnes touchées déplaçait un pixel.
- Chaque particule garde un polygone de `LP_POLY_MAX` (64) points, la taille du trait de l'œil fermé, quand un cœur ou
  une étincelle en prend 24 : environ 10 Ko de mémoire interne jamais écrits. Les réduire demande un autre type de
  polygone dans `render_state.h`, public, donc une nouvelle version mineure du moteur et une mesure sur les cartes ; à
  reprendre quand la mémoire manquera (#38).
