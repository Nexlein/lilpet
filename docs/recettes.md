# Faire évoluer lilpet

Les gestes qui reviennent : ajouter une carte, un pilote, une émotion, une pupille, une commande, un champ de `hello` ;
les essais qui se font à la main ; sortir une version. Pour chaque endroit à modifier, ce qui échoue si on l'oublie, et
ce que rien ne vérifie : c'est là qu'il faut être attentif.

## Ajouter une carte

1. Créer `firmware/boards/<carte>/` avec `board.cmake`, qui liste ses pilotes, et `board_config.h`, ses broches et ses
   faits. Parmi eux, `BOARD_CHIP_PICO`, `BOARD_FLASH_MB` et `BOARD_PSRAM_MB` décrivent sa puce, que le firmware compare
   au démarrage avant de toucher une broche ([décision 6](adr/0006-une-image-par-carte.md)). Un fait mal orthographié,
   ou une puce identique à celle de la carte vision, fait échouer `scripts/boards.test.mjs`.
2. Ajouter son nom à la liste des cartes de `.github/workflows/ci.yml`, et son job (« ESP32-S3 firmware build
   (<carte>) ») aux checks requis de `.github/rulesets/dev.json` et `main.json`. `scripts/ci.test.mjs` compare les
   trois aux dossiers.
3. Ajouter sa ligne au [tableau des cartes](firmware.md#les-cartes), que `scripts/ci.test.mjs` vérifie aussi.
4. Une fois la pull request fusionnée, appliquer les règles avec `mise run github:rulesets` (gh connecté). **Rien ne le
   vérifie** : sans cela, GitHub n'exige pas la compilation de la nouvelle carte. Renommer ou retirer une carte ou un
   job suit le même chemin ; un check requis dont le job n'existe plus bloque chaque pull request.

## Ajouter un pilote

Chaque sorte de composant a son en-tête, et chaque pilote son `.c`, qu'une carte choisit dans son `board.cmake` :

- un écran : `display.h` (ses fonctions disent ce qu'un pilote doit faire ; `display_wait_frame` vient du pilote s'il a
  le signal TE, de `display_paced.c` sinon) ;
- un capteur de mouvement : `imu.h` ;
- une batterie ou une puce d'alimentation : `battery.h`.

Un appareil du bus I2C passe par `board_i2c_device`, `board_i2c_read` et `board_i2c_write` (`board.h`). Ce qui peut se
calculer sans ESP-IDF (des registres, une règle) va dans un en-tête pur, testé sur PC dans `firmware/tests` et inscrit
dans `engine/tests/test.h`. Une broche qui doit rester tenue pendant la veille légère se déclare dans
`PINS_KEPT_ASLEEP` (`board.c`) : ESP-IDF laisse sinon flotter toutes les broches. **Rien ne le vérifie.**

Ce qui réveille les yeux, ou les garde éveillés, se lit à un seul endroit, `read_inputs` dans `main.c` : un nouveau
capteur (un toucher, une touche) s'y ajoute. Il faut aussi le compter dans `rests`, ce qui permet à une carte de
s'endormir seule.

## Ajouter une émotion ou une réaction

1. L'ajouter à `eyes/emotions.json`, avec son libellé.
2. Ajouter `LP_EMOTION_<NOM>` (ou `LP_EVENT_<NOM>` pour une réaction) à l'énumération de
   `engine/include/lilpet/lilpet.h`, avant le `_COUNT` qui la termine.
3. `npm run tables` : la table générée nomme chaque entrée par son `LP_EMOTION_<NOM>`, et vérifie que les deux listes
   ont la même longueur ; un oubli fait échouer la compilation.
4. Ajouter une scène qui la montre à `engine/tests/scenes.json` (`npm run check:data` échoue sinon), puis
   `mise run engine:wasm` et `npm run goldens`, et regarder les images réécrites.
5. Le dessin a changé : la prochaine version augmente la version mineure du moteur (voir plus bas).

Une réaction qui se répète tant que sa cause dure (comme `shake`) doit aussi entrer dans la liste `HELD` de
`scripts/validate-data.mjs`, qui lui interdit les étincelles. **Rien ne vérifie** que cette liste suit le moteur.

L'app PC prend la nouvelle émotion d'elle-même, son libellé compris : `app/src/ui/labels.ts` le lit dans le JSON.

## Ajouter une pupille

`eyes/looks.json`, l'énumération `lp_pupil` de `lilpet.h`, `PUPILS` dans `engine/wasm/engine.mjs` et son type dans
`engine.d.mts`, son dessin dans `engine/src/render.c`, son libellé dans `app/src/ui/labels.ts`, et une scène. Les
tables, `engine/wasm/engine.test.mjs`, le `switch` de `render.c` et `tsc` échouent chacun quand il manque sa part.

## Ajouter une commande

Le protocole est dans [protocole.md](protocole.md). Une commande touche :

- le moteur : `lp_command_type` et sa donnée dans `lilpet.h`, sa lecture dans `engine/src/command.c`, ses tests dans
  `engine/tests/test_command.c` ;
- le firmware : sa réponse dans `handle()` (`firmware/main/commands.c`) et son effet dans `apply()`
  (`firmware/main/main.c`). Les deux sont des `switch` sans `default` : la compilation échoue tant que l'un l'oublie ;
- l'app, si elle l'envoie : `app/src/device/protocol.ts`, et la fausse carte de ses tests, `fake-port.ts` ;
- `scripts/check-device.py`, qui essaie chaque commande sur la carte, et la fausse carte de
  `scripts/test_board_scripts.py` ;
- [protocole.md](protocole.md).

`scripts/docs.test.mjs` vérifie que [protocole.md](protocole.md) nomme la commande, pas ce qu'elle prend ni ce qu'elle
répond. **Rien ne vérifie** que `check-device` l'essaie.

## Ajouter un champ à `hello`

`commands_hello` (`firmware/main/commands.h`), la ligne que `reply_hello` écrit (`commands.c`), ce que `main.c` y met,
[protocole.md](protocole.md), et `parseHello` dans `app/src/device/protocol.ts` si l'app s'en sert. Les tests de l'app
et des scripts lisent le format de la ligne dans `commands.c` : leurs fausses cartes suivent.

## Les essais à la main

Ce que la CI ne peut pas faire, faute de carte :

- `BOARD=<carte> mise run firmware:check-device` sur chaque carte que le changement touche (les deux cartes à la
  maison : l'AtomS3R et le M5Stamp S3A). Un correctif de carte se prouve d'abord sur l'ancien firmware : le zip de la
  Release précédente se flashe tel quel ([firmware.md](firmware.md#tests)).
- Sur batterie, sans câble ([décision 12](adr/0012-energie.md)) : l'endormissement après 2 min, le réveil au bouton et
  au mouvement, les couleurs après un long sommeil, l'air fatigué.
- L'app PC avec une vraie carte, dans Chrome : le panneau de l'app de développement n'a pas de sélecteur de port série.
- À l'arrivée de la carte écran : les étapes « À la réception, dans cet ordre » de
  [materiel.md](materiel.md#carte-écran--waveshare-esp32-s3-touch-amoled-175).

## Sortir une version

1. Une pull request qui monte les versions : celle du firmware, `PROJECT_VER` dans `firmware/CMakeLists.txt`, et celle
   du moteur, `LP_VERSION_*` dans `lilpet.h`. Un changement du dessin monte au moins sa version mineure : l'app PC s'en
   sert pour dire qu'une carte ne dessine pas comme son aperçu.
2. Les essais à la main, sur le firmware de cette version.
3. Une pull request de `dev` vers `main`, fusionnée quand sa CI est verte.
4. Un tag annoté sur le commit de fusion de `main`, dont le message devient les notes de la Release :
   `git tag -a vX.Y.Z <commit> -F <notes>`, puis `git push origin vX.Y.Z`.
5. `release.yml` vérifie que le tag est annoté, porte la version du firmware, est sur `main`, et que la version mineure
   du moteur a monté si le dessin a changé depuis la dernière Release. Puis il relance la CI, publie la Release avec un
   zip par carte et met l'app PC à jour sur GitHub Pages.
