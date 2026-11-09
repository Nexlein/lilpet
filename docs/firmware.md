# Firmware et mesures

Le firmware ESP-IDF de la carte écran (`firmware/`) : une image par carte
([décision 6](adr/0006-une-image-par-carte.md)), le rendu sur les deux cœurs
([décision 9](adr/0009-rendu-deux-coeurs.md)). Les commandes sont dans le README (« Tester sur un ESP32-S3 »).

## Ce que fait le firmware

- Il affiche les yeux en direct, qui vivent seuls (saccades, clignements, tête qui suit le regard), avec le look choisi
  dans l'app PC, gardé en mémoire flash (`cherry` tant qu'aucun n'a été envoyé). Les commandes de l'app et du futur
  modèle passent par le même port USB ([protocole](protocole.md)).
- À chaque tour de l'animation de mesure (`LP_BENCH_FRAMES` images, environ 17 s), il écrit sur ce port le nombre
  d'images par seconde, le temps de dessin, le temps de dessin plus envoi, et la part de l'écran redessinée
  (`mise run firmware:monitor` pour les lire).
- Chaque image démarre sur le signal de rafraîchissement de l'écran (TE), pour éviter les déchirures. Les bandes partent
  par DMA pendant que le moteur dessine la suivante, dans deux tampons de 28 Ko en mémoire interne.
- **Inclinaison** : sur l'AtomS3R, `firmware/main/imu_bmi270.c` lit l'accéléromètre du BMI270 à chaque image
  (démarrage d'après sa fiche technique, avec le fichier de configuration de Bosch, `firmware/third_party/bmi270/`).
  `main.c` passe la mesure au moteur par `lp_senses` (`engine/src/senses.c`, testé sur PC), qui la lisse, suit le
  bouton sans rebond et repère un capteur muet.
- **Réactions** : le moteur reçoit des événements (`lp_eyes_react` : toucher, secousse) et repère seul la position à
  l'envers dans la gravité ; chaque événement mène à une réaction décrite comme une donnée (émotion, durée, étincelles,
  dans `eyes/emotions.json`), pour que l'app puisse un jour les changer. Le firmware ne fait que traduire le matériel :
  appui sur l'écran de l'AtomS3R (broche 41), secousse repérée par `lp_shake_update` sur la mesure brute, avant le
  lissage.
- **Secousse**, mesurée sur la carte : une secousse à la main monte jusqu'à 11 g et s'écarte de 3 à 6 g de 1 g en
  moyenne, vers 6 Hz, contre 0,01 g au repos ; une petite secousse et une franche ont la même force, seule leur durée
  change. Elle compte après environ 0,4 s (un choc ou un coup de poignet bref ne comptent pas) ; le moteur la reçoit
  comme un état (`lp_eyes_set_shaking`), comme la position à l'envers.

## Pilotes d'écran

- **Écran rond** (`firmware/main/display_co5300.c`) : il reprend la démo ESP-IDF officielle de Waveshare pour la carte
  1,32" : broches, séquence de démarrage du CO5300, QSPI à 40 MHz, rotation à 180°, décalage de 6 colonnes
  ([démo](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.32/blob/main/Example/ESP-IDF/07_LVGL_V9_Test/components/lvgl_port_bsp/lvgl_port_bsp.cpp)).
  Il n'a encore jamais tourné sur la carte : au premier essai, vérifier le sens de l'image, le décalage, les couleurs
  et l'absence de déchirure.
- **AtomS3R** (`firmware/main/display_atoms3r.c`) : il reprend les réglages de la bibliothèque de M5Stack
  ([M5GFX](https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp), `board_M5AtomS3R`) : SPI à 40 MHz,
  identifiant lu pour distinguer les deux puces d'écran (ST7735S ou GC9107), séquence d'initialisation complète de
  chaque puce, décalages, orientation (bit BGR compris) et inversion des couleurs, rétroéclairage par une puce sur le
  bus I2C. Sans signal TE, il cadence à 62,5 images/s (`display_paced.c` : des ticks de 1 ms ne font pas 60). Testé le
  1er novembre 2026 sur un AtomS3R à GC9107 : image propre, couleurs justes. Sans la séquence du fabricant ni le bit
  BGR, l'écran montrait une bande de bruit en haut et des couleurs inversées.

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

- **M5Stamp S3A, 466 px** (`BOARD=stamp_s3a mise run firmware:deploy`), même processeur que le Waveshare : sans écran
  ni PSRAM, son image cadence à 62,5 images/s et ses bandes de 466 px partent sur des broches non branchées, donc les
  temps restent réalistes. Elle joue en boucle l'animation de la mesure sur PC, avec le même look (cherry), la même
  graine et le même pas fixe de 1/60 s, pour dessiner les mêmes images que `mise run engine:bench` : la part redessinée
  se compare, mais son temps de dessin est celui des deux cœurs, contre un seul sur PC. Mesuré le 8 novembre 2026,
  pour 51 % de l'écran redessiné en moyenne : sur un cœur, dessin 29,5 ms en moyenne et 41 ms au pire, dessin plus
  envoi 32,5 ms, soit 30,7 images/s ; sur les deux cœurs, rendu en IRAM, dessin 16,4 ms et 23 ms au pire, dessin plus
  envoi 19,3 ms, soit 51 à 52 images/s.
- **AtomS3R, 128 px**, yeux au repos : 8,2 ms de dessin par image, puis 4,9 ms (novembre 2026) une fois le mélange des
  couleurs mis en `inline` et les divisions faites une fois par ligne plutôt qu'à chaque sous-ligne (la puce n'a pas de
  division rapide), et 3,6 ms le 6 novembre, avec les couleurs converties une fois par image et les pixels répétés
  réutilisés. Dessin plus envoi : 6,7 ms, pour 16 ms disponibles à 62,5 images/s. Sur les deux cœurs (8 novembre) :
  dessin 1,9 ms, dessin plus envoi 5,1 ms, image propre à l'œil.
- Avec un écran, les temps affichés suivent l'animation libre et ne se comparent pas.

## Compiler et vérifier

- ESP-IDF tourne dans Docker ([décision 8](adr/0008-outils-figes.md)), sans installation locale. La CI garde le binaire
  prêt à flasher de chaque carte.
- `firmware:check-device` est la seule vérification que la CI ne peut pas faire, faute de matériel : elle parle à la
  carte comme l'app, vérifie qu'elle garde un look après un redémarrage, et remet son look et son humeur à la fin.
