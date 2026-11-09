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
  broches, son montage, sa taille d'écran et le boîtier de sa puce, un `sdkconfig.defaults` facultatif ses options
  ESP-IDF. Le dossier donne son nom à la carte. CMake, la CI et les scripts de carte (`scripts/board_link.py`) lisent
  ces dossiers.
- Chaque en-tête (`display.h`, `imu.h`) est une interface, chaque composant un fichier `.c`, sans table de fonctions :
  tout est choisi à la compilation.
- **Deux gardes contre une mauvaise image.** `firmware:flash` (`scripts/flash-guard.py`) demande `> hello` à la carte et
  refuse si une autre carte répond, puis donne ce port à esptool ; `FORCE=1` passe outre sans rien demander. Au
  démarrage, `board_init` compare le boîtier de la puce et s'arrête avant de toucher une broche.

## Conséquences

- Ajouter un écran : un `.c` de 4 fonctions et une ligne dans le `board.cmake`. Ajouter un capteur : un `.c` de 2
  fonctions et une ligne. Ajouter une carte aux composants connus : son dossier, que la CI compile d'elle-même (README,
  « Tester sur un ESP32-S3 »).
- Une carte vierge, ou qui redémarre en boucle, ne répond pas à `hello` et passe la première garde. Sans carte trouvée
  sur un port USB de la puce, le flash s'arrête : esptool essaierait chaque port série et flasherait ce qui répond.
  `FORCE=1` le laisse chercher.
- Une image `stamp_s3a` sur la XIAO n'est pas arrêtée par la seconde : les deux sont en boîtier QFN56. Le futur firmware
  de la XIAO devra répondre à `> hello` pour que le flash la protège.
