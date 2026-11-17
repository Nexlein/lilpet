# 6. Une image de firmware par carte, une carte par dossier

- Date : 2026-11-05
- Statut : accepté

## Contexte

Le firmware tourne sur plusieurs cartes : l'AtomS3R en attendant le matériel final, l'écran rond Waveshare 1,32", une
M5Stamp S3A sans écran pour mesurer la vitesse. Un seul binaire qui reconnaîtrait sa carte n'est pas possible :
l'AtomS3R et le Waveshare 1,32" ont le même boîtier de puce (ESP32-S3-PICO), et sonder un bus ou lire l'identifiant d'un
écran obligerait à piloter des broches sur une carte inconnue.

## Décision

- **Une image par carte**, choisie à la compilation (`BOARD`).
- **Une carte est un dossier**, `firmware/boards/<carte>/` : `board.cmake` liste ses composants, `board_config.h` ses
  broches, son montage, sa taille d'écran et sa puce (boîtier, flash et PSRAM). Les options d'ESP-IDF sont celles de
  `firmware/sdkconfig.defaults`, communes à toutes les cartes. Le dossier donne son nom à la carte. CMake, la CI et les
  scripts de carte (`scripts/board_link.py`) lisent ces dossiers.
- Chaque en-tête de composant (`display.h`, `imu.h`, `battery.h`) est une interface, chaque composant un fichier `.c`,
  sans table de fonctions : tout est choisi à la compilation. Seuls quelques en-têtes portent de la logique pure, pour
  que les tests sur PC la voient (`firmware/tests`) ; leur liste est dans [firmware.md](../firmware.md#tests).
- **Deux gardes contre une mauvaise image.** `firmware:flash` (`scripts/flash-guard.py`) demande `> hello` à la carte et
  refuse si une autre carte répond, puis donne ce port à esptool ; `FORCE=1` passe outre sans rien demander. Au
  démarrage, `board_init` compare la puce à celle de la carte, son boîtier, la taille de sa flash et celle de sa PSRAM
  (`chip.h`), et s'arrête avant de toucher une broche.

## Conséquences

- Ajouter un écran ou un capteur : un `.c` qui suit l'en-tête de sa sorte (`display.h`, `imu.h`, `battery.h`) et une
  ligne dans le `board.cmake`. Ajouter une carte aux composants connus : son dossier, et son nom dans la liste des
  cartes de la CI, que des tests comparent aux dossiers. Les étapes et ce qui les vérifie :
  [recettes.md](../recettes.md).
- Une carte vierge, ou qui redémarre en boucle, ne répond pas à `hello` et passe la première garde. Sans carte trouvée
  sur un port USB de la puce, le flash s'arrête : esptool essaierait chaque port série et flasherait ce qui répond.
  `FORCE=1` le laisse chercher.
- La XIAO, la carte caméra, partage le boîtier QFN56 de la 1,75" et du M5Stamp S3A, et son firmware d'origine ne répond
  pas à `> hello` : la seconde garde l'arrête quand même, par sa flash (8 Mo contre 16 sur la 1,75") ou sa PSRAM (8 Mo,
  aucune sur le M5Stamp). Un test (`scripts/boards.test.mjs`) refuse une carte qui aurait la puce de la XIAO. Deux cartes
  à la même puce (l'AtomS3R et la 1,32") ne se distinguent que par la première garde.
