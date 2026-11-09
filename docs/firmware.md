# Firmware et mesures

Le firmware ESP-IDF de la carte écran (`firmware/`) : une image par carte
([décision 6](adr/0006-une-image-par-carte.md)), le rendu sur les deux cœurs
([décision 9](adr/0009-rendu-deux-coeurs.md)). Les commandes sont dans le README (« Tester sur un ESP32-S3 »).

## Les cartes

| Carte           | Écran                                           | Capteur de mouvement    | Cadence                |
| --------------- | ----------------------------------------------- | ----------------------- | ---------------------- |
| `atoms3r`       | LCD 128 px en SPI (`display_atoms3r.c`)         | BMI270 (`imu_bmi270.c`) | 62,5 images/s, sans TE |
| `waveshare_132` | AMOLED rond 466 px en QSPI (`display_co5300.c`) | aucun (`imu_none.c`)    | signal TE de l'écran   |
| `stamp_s3a`     | aucun : le pilote CO5300 envoie dans le vide    | aucun (`imu_none.c`)    | 62,5 images/s, sans TE |

Une carte sans signal TE cadence ses images avec `display_paced.c`, à 62,5 images/s : des ticks de 1 ms ne font pas 60.

## Ce que fait le firmware

- Il affiche les yeux en direct, qui vivent seuls (saccades, clignements, tête qui suit le regard), avec le look choisi
  dans l'app PC (`cherry` tant qu'aucun n'a été envoyé). Les commandes de l'app arrivent par le port USB, celles du
  futur modèle arriveront par l'UART ([protocole](protocole.md)).
- Toutes les `LP_BENCH_FRAMES` images (la longueur de l'animation de mesure, `engine/tools/bench_script.h`), il écrit
  sur le port USB le nombre d'images par seconde, le temps de dessin, le temps de dessin plus envoi, et la part de
  l'écran redessinée (`mise run firmware:monitor` pour les lire).
- Sur un écran qui a le signal de rafraîchissement (TE, le CO5300), chaque image démarre sur ce signal, pour éviter les
  déchirures. Les bandes partent par DMA pendant que le moteur dessine la suivante, dans deux tampons en mémoire
  interne.
- **Capteurs** : sur l'AtomS3R, `firmware/main/imu_bmi270.c` lit l'accéléromètre du BMI270 à chaque image (démarrage
  d'après sa fiche technique, avec le fichier de configuration de Bosch, `firmware/third_party/bmi270/`), et
  `board.c` lit le bouton (l'écran de l'AtomS3R, broche 41). `main.c` passe ces valeurs brutes au moteur par
  `lp_senses_update` (`engine/src/senses.c`, le même sur toutes les cartes, testé sur PC) : il repère la secousse sur
  la mesure brute (`lp_shake_update`), puis la lisse pour l'inclinaison, suit le bouton sans rebond, et repère un
  capteur muet.
- **Réactions** : le moteur reçoit des événements (`lp_eyes_react` : appui, visage vu…) et des états (secousse, gravité,
  dont il tire seul la position à l'envers) ; chaque cause mène à une réaction décrite comme une donnée (émotion, durée,
  étincelles, dans `eyes/emotions.json`), pour que l'app puisse un jour les changer.
- **Secousse**, mesurée sur la carte : une secousse à la main monte jusqu'à 11 g, vers 6 Hz, et s'écarte en moyenne de
  3 à 6 g de la gravité (1 g), contre 0,01 g au repos ; une petite secousse et une franche ont la même force, seule
  leur durée change. Elle compte après environ 0,4 s : un choc ou un coup de poignet bref ne comptent pas.

## Tests

Les parties pures du firmware ont leurs tests sur PC, dans `firmware/tests`, lancés avec ceux du moteur
(`mise run engine:test`) : la reconnaissance de la puce de l'écran de l'AtomS3R (`lcd_id.h`, dont la branche ST7735S
n'a jamais tourné sur une carte), le passage des axes du capteur de mouvement aux axes de l'écran (`imu_axes.h`), le
moment où un look s'écrit en flash (`look_saving.h`) et le calcul d'une image : les bandes partagées entre les deux
cœurs, la part de l'écran redessinée (`frame.h`).
Le reste se vérifie sur la carte, avec `firmware:check-device`.

## Pilotes d'écran

- **Écran rond** (`firmware/main/display_co5300.c`) : il reprend la démo ESP-IDF officielle de Waveshare pour la carte
  1,32" : broches, séquence de démarrage du CO5300, QSPI à 40 MHz, rotation à 180°, décalage de 6 colonnes
  ([démo](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.32/blob/main/Example/ESP-IDF/07_LVGL_V9_Test/components/lvgl_port_bsp/lvgl_port_bsp.cpp)).
  Il n'a encore jamais tourné sur la carte : au premier essai, vérifier le sens de l'image, le décalage, les couleurs,
  l'absence de déchirure, et que le signal TE vient avant que l'écran s'allume. L'écran ne s'allume qu'après la première
  image, car sa mémoire garde du bruit au démarrage, et `display_init` cherche le TE avant : s'il n'en voit pas, le
  journal le dit (« no TE signal »).
- **AtomS3R** (`firmware/main/display_atoms3r.c`) : il reprend les réglages de la bibliothèque de M5Stack
  ([M5GFX](https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp), `board_M5AtomS3R`) : SPI à 40 MHz,
  identifiant lu pour distinguer les deux puces d'écran (ST7735S ou GC9107), séquence d'initialisation complète de
  chaque puce, décalages, orientation (bit BGR compris) et inversion des couleurs, rétroéclairage par une puce sur le
  bus I2C. Testé le 1er novembre 2026 sur un AtomS3R à GC9107 : image propre, couleurs justes. Sans la séquence du
  fabricant ni le bit BGR, l'écran montrait une bande de bruit en haut et des couleurs inversées.

## Mesures

### Sur PC

`mise run engine:bench` compile le moteur en mode optimisé et mesure le temps de rendu :

- pour chaque scène de référence, l'écran entier (première image) puis les yeux seuls (image suivante) ;
- sur l'animation de mesure (`engine/tools/bench_script.h`, à 60 images/s) : chaque émotion pendant une seconde,
  amoureux pendant 5 s pour que les cœurs s'accumulent, avec saccades, clignements et particules, puis le regard qui
  suit un visage. Chaque image ne redessine que sa zone modifiée, comme sur l'objet.

En CI, les tableaux vont dans le résumé du job « C engine », pour comparer d'un commit à l'autre, sans seuil : les
machines partagées de GitHub sont trop irrégulières pour ça. Fin octobre 2026, sur un AMD Ryzen 7 7730U : 0,34 ms par
image en moyenne sur l'animation, 0,51 ms au pire. Les pupilles « gland », les plus grandes, coûtent environ 20 % de
plus que les autres. Ces chiffres ne servent qu'à repérer les régressions : l'ESP32-S3 (240 MHz) est beaucoup plus lent
qu'un PC.

### Sur la puce

- **M5Stamp S3A, 466 px** (`BOARD=stamp_s3a mise run firmware:deploy`), même processeur que le Waveshare : sans écran ni
  PSRAM, ses bandes de 466 px partent sur des broches non branchées, donc les temps restent réalistes. Elle joue en
  boucle l'animation de mesure, avec le même look (cherry), la même graine et le même pas fixe de 1/60 s que sur PC.
  Chaque tour démarre comme chaque passe sur PC (les mêmes yeux, leur première image dessinée entière et non comptée),
  donc il dessine les mêmes images que `mise run engine:bench`, toutes : la part redessinée se compare, mais son temps
  de dessin est celui des deux cœurs, contre un seul sur PC. Mesuré le 8 novembre 2026, avant que chaque tour reparte de
  zéro (les tours suivant le premier continuaient alors l'animation du précédent), pour 51 % de l'écran redessiné en
  moyenne : sur un cœur, dessin 29,5 ms en moyenne et 41 ms au pire, dessin plus envoi 32,5 ms, soit 30,7 images/s ; sur
  les deux cœurs, rendu en IRAM, dessin 16,4 ms et 23 ms au pire, dessin plus envoi 19,3 ms, soit 51 à 52 images/s.
- **AtomS3R, 128 px**, yeux au repos : 8,2 ms de dessin par image, puis 4,9 ms (novembre 2026) une fois le mélange des
  couleurs mis en `inline` et les divisions faites une fois par ligne plutôt qu'à chaque sous-ligne (la puce n'a pas de
  division rapide), et 3,6 ms le 6 novembre, avec les couleurs converties une fois par image et les pixels répétés
  réutilisés. Dessin plus envoi : 6,7 ms, pour 16 ms disponibles à 62,5 images/s. Sur les deux cœurs (8 novembre) :
  dessin 1,9 ms, dessin plus envoi 5,1 ms, image propre à l'œil.
- Avec un écran, les temps affichés suivent l'animation libre et ne se comparent pas.
