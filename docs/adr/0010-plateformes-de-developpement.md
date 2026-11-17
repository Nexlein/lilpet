# 10. Linux d'abord ; sous Windows, le code dans WSL et les cartes en natif

- Date : 2026-11-17
- Statut : accepté

## Contexte

Le projet se développe sur un seul PC, personnel, sous Linux aujourd'hui ; il passera peut-être un jour sous Windows.
Tout suppose POSIX : les tâches de `mise.toml` sont écrites en `sh` (boucles, `case`, `[ ]`), les scripts npm passent
par `xargs -0`, les hooks git sont des scripts `sh`, et les compilations du firmware et du WebAssembly tournent dans
Docker. Sous Windows :

- mise lance une tâche avec `cmd /c`, sauf si elle demande un autre shell (`shell = "bash -c"`,
  [mise](https://mise.jdx.dev/tasks/task-configuration.html)) ;
- une carte attachée à WSL par usbipd n'est plus visible de Windows, Chrome compris
  ([Microsoft](https://learn.microsoft.com/en-us/windows/wsl/connect-usb)) : l'app PC ne la voit plus tant qu'elle n'est
  pas détachée ;
- pyserial et esptool tournent en natif ; le port USB des cartes (USB-Serial-JTAG, 303A:1001) y est un port COM, dont
  le pilote vient d'ordinaire avec le système et s'installe tout seul
  ([Espressif](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/get-started/establish-serial-connection.html)).

## Options étudiées

- **Tout dans WSL**, cartes comprises : le plus simple à mettre en place, mais l'app PC, dans Chrome sous Windows, perd
  la carte tant que WSL la tient.
- **Tout en natif Windows** (Git Bash, mise pour Windows, Docker Desktop) : chaque tâche à faire passer par `bash`, et
  les outils à vérifier un par un sous Windows.
- **Le code dans WSL, les cartes sous Windows.**

## Décision

- **Linux, tant que le PC y reste.** Rien ne change.
- **Sous Windows, le code dans WSL2** (Ubuntu, Docker Engine installé dans WSL) : le dépôt, mise, les compilations, les
  tests et Claude, avec les mêmes commandes que sous Linux.
- **Sous Windows, les cartes en natif** : flasher, `check-device` et le moniteur depuis Windows, là où Chrome, pour
  l'app PC, voit le même port. Les scripts des cartes ne dépendent de `termios` que là où il existe
  (`scripts/board_link.py`).
- **L'app PC en ligne** sur GitHub Pages ([décision 4](0004-app-web-serial.md)) : sous Windows, l'ouvrir ne demande
  rien d'installer.

## Conséquences

- À faire le jour où le PC passe sous Windows, pas avant : lancer les tâches des cartes depuis Windows, soit sous
  `bash`, soit avec un script à part ; installer Python et uv côté Windows ; vérifier le pilote USB de la carte.
- La mémoire du projet dans Claude vit hors du dépôt, dans un dossier nommé d'après le chemin Linux du projet
  (`~/.claude/projects/`) : la recopier en changeant de PC. Ce qui doit durer va dans les docs du dépôt.
- Les cartes dans WSL restent possibles (usbipd, voir le README), pour qui n'a pas besoin de Chrome en même temps.
