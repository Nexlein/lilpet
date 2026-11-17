# 11. La NVS reste à sa place, et la flash se remplit après elle

- Date : 2026-11-17
- Statut : accepté

## Contexte

Les cartes ont au moins 8 Mo de flash et une appli d'environ 300 Ko ; la 1,75" en a 16, dont elle n'utilise que 8 :
`firmware/sdkconfig.defaults` est commun à toutes les cartes ([décision 6](0006-une-image-par-carte.md)). Jusqu'ici, elles prenaient la table de partitions par défaut
d'ESP-IDF : `nvs` à 0x9000 (24 Ko), `phy_init`, puis une seule appli de 1 Mo, sans coredump ni place pour des mises à
jour (OTA). La NVS garde le look choisi dans l'app.

Changer la table ne coûte pas forcément ce look. La NVS ne lit que sa propre zone, et `firmware:flash` écrit le
bootloader, la table et l'appli, jamais la NVS
([tables de partitions](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html)).
Une table qui garde `nvs` au même endroit et de la même taille garde donc ses données. Déplacer la NVS, au contraire,
perd ce qu'elle tenait.

Depuis la v0.7.5 ([#65](https://github.com/Nexlein/lilpet/pull/65)), le watchdog fait redémarrer la carte quand la boucle de dessin se bloque. Sans coredump, on sait
seulement que la carte a planté (`reset=task_wdt`), pas où.

## Décision

- **La NVS reste à 0x9000, sur 0x6000**, là où la table par défaut l'avait mise. C'est une règle :
  `scripts/partitions.test.mjs` la vérifie.
- **Les partitions se suivent, et une nouvelle va après la dernière.** `firmware/partitions.csv` écrit chaque adresse
  et chaque taille, sans calcul laissé à ESP-IDF.
- **L'appli a 2 Mo**, de quoi grandir.
- **Un coredump de 64 Ko**, écrit en flash à chaque crash, au format ELF, que `mise run firmware:coredump` lit et
  décode ([coredump](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/core_dump.html)).
- **Pas encore d'OTA ni de partition de données.** Les mises à jour par l'app (OTA : `otadata` et deux emplacements
  d'appli, [OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/ota.html)) et une
  partition pour de grosses données viendront après le coredump, le jour où on en aura besoin, sans rien perdre.

## Conséquences

- Le premier flash avec cette table a gardé le look de l'AtomS3R (`storage=ok`, même look).
- Un crash laisse un coredump qui montre chaque tâche et sa pile. Essayé sur l'AtomS3R : après `> test hang`, il montre
  la boucle de dessin arrêtée dans `vTaskDelay`, à la ligne de `main.c` qui la bloque.
- Le coredump ne se lit qu'avec l'ELF de l'image que la carte fait tourner : compiler, flasher, puis le lire après le
  crash.
- Les formats des données gardées (le look dans la NVS, les favoris dans le navigateur) prendront une version quand ils
  changeront, avec les looks perso ([#57](https://github.com/Nexlein/lilpet/issues/57)).
