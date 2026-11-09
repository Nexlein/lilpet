# Architecture

Décision prise le 2026-10-27. À mettre à jour si un choix change.

## Contexte

Les yeux doivent tourner à trois endroits : sur l'objet (ESP32-S3 + écran rond AMOLED 466×466), dans un simulateur
sur PC, et dans la future app PC qui sert à changer les yeux et les animations. Le prototype actuel est en JavaScript
(Canvas 2D) : parfait pour explorer le design, mais il ne peut pas tourner sur l'ESP32. Tout ce qui y est ajouté
devrait être réécrit en C pour l'objet.

## Options étudiées

| Option                                            | Pour                                                          | Contre                                                                              |
| ------------------------------------------------- | ------------------------------------------------------------- | ----------------------------------------------------------------------------------- |
| Rester en JS, réécrire en C au moment du firmware | Rien à changer maintenant                                     | Deux rendus à maintenir, écarts visuels, réécriture de plus en plus longue          |
| LVGL (bibliothèque d'interface)                   | Pilotes d'écran fournis, simulateur PC                        | Lourd pour deux yeux, peu adapté aux paupières découpées                            |
| Moteur en Rust (embarqué + WebAssembly)           | Langage moderne et sûr                                        | Outils pour la puce Xtensa de l'ESP32-S3 moins mûrs, langage nouveau pour le projet |
| **Moteur en C partagé (ESP-IDF + Emscripten)**    | Un seul rendu, pixel pour pixel identique partout, C maîtrisé | Il faut écrire le dessin soi-même                                                   |

## Choix

**Un seul moteur des yeux, en C**, compilé :

- pour l'ESP32-S3 avec ESP-IDF, l'outil officiel d'Espressif (écrit en C) ;
- en WebAssembly avec Emscripten, pour le simulateur et l'app PC.

Exemple d'un projet qui partage déjà un moteur de rendu C entre ESP32 AMOLED et navigateur :
[gea-embedded](https://github.com/dashersw/gea-embedded).

### Rendu

- **Dessin ligne par ligne, sans bibliothèque graphique.** Les yeux ne sont que des ellipses découpées par des droites
  (paupières) et d'autres ellipses (joie, croissant). Pour chaque ligne de l'écran, on calcule directement les segments
  à remplir, avec un lissage des bords calculé à partir de la couverture du pixel.
- **Image en RGB565** (2 octets par pixel, environ 434 Ko pour l'écran entier).
- **Toute taille d'écran carré jusqu'à 466 px** (`lp_renderer_set_size`), par exemple 128 pour l'AtomS3R : le dessin
  reste calculé en coordonnées 466 et n'est réduit qu'en sortie, donc l'image est la même, juste plus petite (test :
  à 128 px, écart moyen de 0,06 à 0,21 avec l'image 466 réduite).
- **Dessin par bandes, sans image entière en mémoire.** Le moteur ne relit jamais l'image précédente : chaque ligne de
  la zone modifiée est redessinée de zéro. Sur l'objet, il dessine donc quelques lignes à la fois (30 lignes, 28 Ko,
  comme les démos Waveshare) en mémoire interne, plus rapide que la PSRAM, et chaque bande part vers l'écran pendant
  qu'il dessine la suivante (`lp_render_begin` puis `lp_render_rows`). `lp_render` dessine l'image entière d'un coup,
  pour le PC, le WebAssembly et les tests. La zone modifiée a toujours des coordonnées paires : le contrôleur de
  l'écran (CO5300) n'accepte que des fenêtres qui commencent sur un pixel pair et de taille paire.
- **Ne renvoyer que ce qui bouge** (zones modifiées). À 40 MHz en QSPI, l'horloge utilisée pour cet écran
  ([LCD Wizard](https://lcdwizard.com/boards/waveshare-s3-touch-amoled-132)), renvoyer tout l'écran plafonne vers
  45 images/s (estimation). Mesuré par `mise run engine:bench` : la zone redessinée couvre 50 % de l'écran en moyenne
  et 70 % au pire (particules du mode amoureux), soit environ 11 ms et 16 ms d'envoi par image (estimation), sous les
  16,7 ms d'une image à 60 images/s, mais avec très peu de marge au pire.
- **`float` et entiers, jamais `double`** : sur l'ESP32-S3, les `double` sont émulés en logiciel et très lents, et même
  les `float` restent plus lents que les entiers
  ([guide de performance ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/speed.html)).
- **Hasard à graine** (mulberry32) : même suite de nombres en JS et en C, donc mêmes particules à graine égale.

### App PC

- Page web en **TypeScript + Vite**, qui fait tourner le moteur WebAssembly et sert d'éditeur (looks, animations).
- Communication avec l'objet par USB grâce à **Web Serial** : rien à installer. Marche sous Chrome et Edge, pas sous
  Safari, Firefox incertain
  ([support navigateurs](https://www.testmuai.com/learning-hub/web-serial-api-browser-support/)). Espressif s'en sert
  déjà pour flasher depuis le navigateur ([esptool-js](https://iotjournal.net/web-serial-api-flash-esp32-in-browser/)).
- Les looks et animations restent des **données** (JSON côté PC), envoyées à l'objet.
- Détail de l'app (version 1, modules, protocole) : [app.md](app.md).

## Organisation cible du dépôt

| Dossier      | Rôle                                                                           | État           |
| ------------ | ------------------------------------------------------------------------------ | -------------- |
| `prototype/` | Référence de design en JS. Scènes de référence dans `prototype/scenes.json`    | Fait           |
| `eyes/`      | Données des looks (puis des animations)                                        | Fait           |
| `engine/`    | Moteur C17 : état, animations, dessin. Aucune dépendance, aucun accès matériel | Fait (étape 2) |
| `app/`       | App PC : TypeScript + Vite, moteur en WebAssembly, éditeur, Web Serial         | Version 1      |
| `firmware/`  | ESP-IDF, carte écran : pilotes, capteurs, appelle le moteur                    | 3 cartes       |
| `vision/`    | ESP-IDF, carte caméra (XIAO), sans le moteur : modèle, événements par UART     | Créé à l'achat |
| `scripts/`   | Outils du dépôt (validation des données…)                                      | Fait           |

## Règle de parité

Le prototype JS est la référence visuelle tant que le moteur C n'a pas tout repris. Chaque scène de
`prototype/scenes.json` (look, émotion, phase du clignement, pose de la tête, regard, particules à graine) doit être
rendue de la même façon par le moteur. Les bords lissés ne seront pas identiques au pixel près (Canvas 2D et notre
dessin ligne par ligne lissent différemment), donc la comparaison tolère un petit écart. Une fois une fonction portée,
le moteur devient la source de vérité pour elle.

Pour l'image, `prototype/reference.html` rend chaque scène avec le prototype et avec le moteur compilé en WebAssembly,
côte à côte, et calcule l'écart. Tolérance : écart moyen ≤ 1,5 sur 255 par canal, au plus 760 gros pixels, dont un canal
s'écarte de plus de 32, et au plus 3 pixels de cœur, c'est-à-dire gros comme leurs 4 voisins. Le test Playwright `tests/parity.spec.mjs` fait échouer la CI si
une scène sort de la tolérance. La règle a changé en novembre 2026 : compter tous les pixels à plus de 32 laissait
passer des étincelles absentes, et faisait échouer des looks blancs sur leurs seuls bords lissés. L'écart de cœur
mesuré vaut 0 sur les scènes et sur 12 600 combinaisons, 1 au plus sur 450 scènes de particules.

L'animation est vérifiée aussi : sur la page interactive, en côte à côte, les deux rendus reçoivent les mêmes commandes
et avancent par pas fixes de 1/64 s. Le test `tests/live.spec.mjs` les fait avancer à la main (repos, suivi de la
souris, cœurs, caresse, endormi) et échoue s'ils sortent de la même tolérance. Une différence d'arrondi entre `float`
et `double` qui ferait basculer un événement les décalerait : la page affiche l'écart et propose de les resynchroniser.

Pour la logique (hasard, émotions, clignement, regard, tête, particules), la comparaison est **exacte** : le script
`scripts/gen-engine-vectors.mjs` (lancé par `npm run vectors`, avec `gen-emotions-table.mjs` pour les tables C) fait
tourner le prototype et écrit les valeurs attendues dans
`engine/tests/js_vectors.h`, que les tests C vérifient. Le hasard donne les mêmes nombres dans les deux langages (24
bits, exacts dans un `float`), et les simulations avancent par pas de 1/64 s pour que le temps soit exact en `float`
comme en `double`.

## Outillage du moteur (mis en place en 2a)

- **CMake + Ninja**, avec des presets : `host-debug` (tests avec AddressSanitizer et UBSan), `host-release`, `wasm`.
- **clang-format** et **clang-tidy**, l'équivalent de prettier et oxlint côté C. Installés par mise depuis leurs
  paquets PyPI, pour avoir la même version en local et en CI (le clang-format du système était en retard).
- **Avertissements traités comme des erreurs**, dont `-Wdouble-promotion` qui refuse tout `double` implicite.
- **Tests** : mini-lanceur maison (`engine/tests/test.h`), sans dépendance.
- **Emscripten dans Docker** (image officielle `emscripten/emsdk`), en local comme en CI. Installé par mise, emsdk
  ajoute son propre clang et son propre node au PATH : ils masquaient ceux du système et faisaient boucler ccache. La
  version est épinglée dans `engine/wasm/Dockerfile` (7.0.0 début novembre 2026) : avec `latest`, une nouvelle
  version pouvait casser la CI sans changement de notre côté. Dependabot propose chaque nouvelle version.
- **ESP-IDF dans Docker**, avec la même commande en local et en CI (`BOARD=<carte> mise run firmware:build`). La version est
  épinglée à un seul endroit, `firmware/Dockerfile` : la dernière version publiée (v6.1 fin octobre 2026). Le tag
  `latest` de l'image officielle suit la branche de développement (`master`), trop instable pour flasher une vraie
  carte ([doc de l'image](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/tools/idf-docker-image.html)).
  Dependabot ouvre une pull request à chaque nouvelle version, que la CI vérifie avant qu'on l'accepte. La CI garde le
  binaire prêt à flasher de chaque carte. Flash et lecture du port USB par esptool (installé par mise) et pyserial
  (`scripts/requirements.txt`, par uv), sans installer ESP-IDF. La
  compilation passe par ccache, gardé dans `firmware/.ccache` (et entre deux runs en CI) : une recompilation complète
  passe d'environ 1 min à 26 s en local.
- **Vérification sur carte** (`BOARD=<carte> mise run firmware:check-device`) : la seule que la CI ne peut pas faire, faute de
  matériel. Elle parle à la carte comme l'app et vérifie qu'elle garde un look après un redémarrage.
- **Tâches mise** (`mise run engine:check`, etc.), lancées telles quelles par la CI.
- Toutes les versions sont figées (novembre 2026 : avec `latest`, la CI tournait déjà sous Node 26 et le poste sous
  Node 24, et un nouveau clang-format aurait pu la faire échouer sur du code inchangé). Chaque nouvelle version arrive
  en pull request, que la CI vérifie avant qu'on l'accepte : Renovate pour les outils de `mise.toml`, que Dependabot
  ne sait pas lire, Dependabot pour npm, les images Docker et les actions, toutes épinglées par
  commit.

## Outillage du dépôt (novembre 2026)

- **Un seul point d'entrée** : `mise run check` lance tout ce qu'un changement doit passer (`npm run check`,
  `engine:check`, la parité). La CI lance ces trois commandes, chacune dans son job pour aller plus vite, au lieu de
  recopier les étapes de `npm run check` : une étape ajoutée au contrôle rapide ne peut plus manquer en CI. Une
  quatrième partie de `mise run check` devrait aussi devenir un job (commentaire en tête des jobs de `ci.yml`). `npm run check` reste la partie rapide (environ 10 s), lancée avant
  chaque push ; ce qui demande Docker reste à la CI.
- **Versions figées partout** : mise lui-même (`min_version`, et la version de la CI, tenue à jour par Renovate),
  l'image `ubuntu-26.04` de la CI, Python (par mise, et uv l'utilise au lieu d'en télécharger un), et les linters du
  shell (shellcheck), du workflow (actionlint) et du Python (ruff). Le compilateur C du PC reste celui du système :
  `engine/CMakeLists.txt` refuse un clang plus ancien que celui de la CI (21), qui n'est pas testé.
- **Hooks git** : avant un commit, le scan de secrets, lint-staged (lint et formatage des fichiers indexés, contrôle des
  fichiers générés quand leurs sources changent) et la carte du code ; avant un push, `npm run check`. Prettier ne
  traite que les fichiers suivis par git, pour ne jamais toucher un fichier local non suivi.
- **Carte du code** (`scripts/codemap.mjs`, sans dépendance) : `.claude/codemap.md` décrit chaque fichier source (rôle,
  exports, fichiers utilisés, et fichiers, tâches et hooks qui l'utilisent), les données et qui les lit, les commandes
  et les paquets npm. Elle se lit depuis l'index git, donc elle correspond toujours à ce qui sera commité. La CI échoue
  si elle est périmée, si un import local est introuvable ou si un fichier ne dit pas à quoi il sert, ce qui oblige
  chaque fichier à commencer par une phrase sur son rôle.

## Vision

Le but : un modèle d'IA dans l'objet traite ce que voit la caméra, les gestes, le visage et les expressions de la
personne, et choisit les réactions. La carte caméra a son propre projet, `vision/`, créé à l'achat de la XIAO : elle n'a
pas besoin du moteur, et ses broches de caméra sont celles de l'écran sur l'autre carte.

Le modèle décide, le moteur exécute. Il peut choisir une réaction par son nom (`> react gesture_like`), dont
`eyes/emotions.json` donne les valeurs par défaut, ou une émotion et sa durée (`> emotion happy 2`), et il dit où regarder
(`> face 0.2 -0.1`). Protocole dans [app.md](app.md). L'aperçu web, l'AtomS3R et l'objet final jouent ainsi les mêmes
réactions, et changer de modèle ne touche pas au moteur. Un modèle qui répète un événement ne noie pas l'écran : les
étincelles partent au plus toutes les 0,5 s, quelle que soit l'émotion.

La liaison UART lira les mêmes lignes que l'USB : `lp_command_parse` ignore tout ce qui ne commence pas par `> `, donc le
journal de démarrage de l'autre puce. À l'arrivée de la XIAO, il faudra une seconde tâche, qui donne ses lignes au même
analyseur et réponde sur son propre flux. Sans carte caméra, les mêmes commandes se testent depuis le PC sur l'AtomS3R : `lp_senses` lâche un visage
sans nouvelle position, pour qu'une source qui s'arrête ne laisse pas les yeux figés.

## Points ouverts

Ce que les audits de novembre 2026 laissent à faire. Ils n'ont trouvé ni crash, ni corruption mémoire, ni perte de données
(118 400 images et 3 millions de lignes de commande au hasard), et JS et C restent identiques. Tout le reste est corrigé.

À mesurer ou à essayer sur du vrai matériel :

- **Dessin à 466 px encore un peu lent pour 60 images/s.** Mesuré le 8 novembre 2026 sur la M5Stamp S3A, même processeur
  que le Waveshare (`BOARD=stamp_s3a mise run firmware:deploy`), pour 51 % de l'écran redessiné en moyenne. Sur un cœur :
  dessin 29,5 ms en moyenne et 41 ms au pire, dessin plus envoi 32,5 ms, soit 30,7 images/s. Sur les deux cœurs, rendu
  en IRAM, une image préparée partagée : dessin 16,4 ms et 23 ms au pire, dessin plus envoi 19,3 ms, soit 51 à
  52 images/s. Il manque environ
  3 ms pour tenir 16,7 ms : accélérer le rendu lui-même (`draw_row`), et essayer le QSPI à 80 MHz sur la carte (moins
  d'attente sur la dernière bande). Un cache d'instructions de 32 Ko n'apporte que 1,5 % de plus : écarté. Sur le
  Waveshare, chaque image attend le signal TE suivant : si l'écran rafraîchit à 60 Hz (à vérifier), une image de 19 ms
  tombe un rafraîchissement sur deux, donc 30 images/s stables, et 60 seulement sous 16,7 ms.
- **Waveshare 1,32"** : pilote CO5300 et signal TE jamais essayés sur la carte. La démo de Waveshare définit la broche TE sans
  s'en servir.
- **AtomS3R à ST7735S** : cette variante d'écran n'a jamais tourné.
- **App et vraie carte dans Chrome** : les pannes de port (écriture morte, réponse restée en attente) sont prouvées sur un faux
  port seulement.
- **Renovate** : réactivé le 9 novembre 2026 sur le repo recréé, il n'a encore ouvert aucune pull request (il passe le
  lundi matin). Vérifier qu'il suit les `tools` des tâches de `mise.toml` et `scripts/requirements.txt`.

À décider ou à construire pour l'objet final :

- **Mouvement et toucher** : le 1,32" n'a pas de capteur de mouvement, et son bouton n'est pas celui de l'AtomS3R. Sans module
  ajouté, pas d'inclinaison ni de vertige ; le toucher demande un pilote pour le CST820.
- **Modèle** : aucun modèle ESP-DL listé dans [materiel.md](materiel.md) ne lit les expressions du visage.
- **Carte vision** : le projet `vision/`, et sa tâche UART, qui devra répondre sur son propre flux, avec un verrou autour du
  look courant. Son firmware devra répondre à `> hello` : une image `stamp_s3a` sur la XIAO n'est pas arrêtée par le boîtier de
  la puce, les deux sont en QFN56.

Limites connues, à reprendre quand elles gêneront :

- L'app associe une réponse à une demande par ordre d'arrivée : le protocole n'a pas d'identifiants.
- Le WebAssembly garde une seule instance d'yeux, ce que l'éditeur devra changer.
- `EyeGlyph` recopie les constantes de pupilles du moteur.
- Les lignes de particules parcourent toute leur boîte : les limiter aux colonnes touchées déplaçait un pixel.

## Performance (mise en place en 2e)

- `mise run engine:bench` compile le moteur en mode optimisé et mesure le temps de rendu :
    - pour chaque scène de référence, l'écran entier (première image) puis les yeux seuls (image suivante) ;
    - sur une animation de 18 s à 60 images/s : chaque émotion pendant une seconde, amoureux pendant 5 s pour que les cœurs
      s'accumulent, avec saccades, clignements et
      particules, puis le regard qui suit un visage. Chaque image ne redessine que sa zone modifiée, comme sur
      l'objet.
- En CI, les tableaux vont dans le résumé du job « C engine », pour comparer d'un commit à l'autre. Il n'y a pas de
  seuil qui fait échouer la CI : les machines partagées de GitHub sont trop irrégulières pour ça.
- Fin octobre 2026, sur un AMD Ryzen 7 7730U : 0,34 ms par image en moyenne sur l'animation, 0,51 ms au pire. Les
  pupilles « gland », les plus grandes, coûtent environ 20 % de plus que les autres (60 % quand elles étaient des
  polygones, avant novembre).
- Ces chiffres ne servent qu'à repérer les régressions : l'ESP32-S3 (240 MHz) est beaucoup plus lent qu'un PC. La
  vraie mesure se fera sur l'objet, avec le firmware.

### Sur la puce

- `BOARD=atoms3r mise run firmware:build`, `firmware:flash` puis `firmware:monitor` : le firmware affiche les yeux en direct, qui
  vivent seuls (saccades, clignements, tête qui suit le regard), avec le look choisi dans l'app PC (gardé en mémoire
  flash, `cherry` tant qu'aucun n'a été envoyé). À chaque tour de 1080 images, il écrit sur le port USB
  le nombre d'images par seconde, le temps de dessin et le temps de dessin plus envoi, et la part de l'écran
  redessinée. Les commandes de l'app passent par le même port ([app.md](app.md)).
- Chaque image démarre sur le signal de rafraîchissement de l'écran (TE), pour éviter les déchirures. Les bandes
  partent par DMA pendant que le moteur dessine la suivante, dans deux tampons de 28 Ko en mémoire interne.
- **Les deux cœurs dessinent.** Chaque bande est coupée en deux : le cœur 0 dessine le haut, une tâche épinglée sur le
  cœur 1 dessine le bas. Les deux lisent la même image, préparée une fois (`lp_render_begin`), et chacun compose ses
  lignes dans son propre brouillon (`lp_row_scratch`, 12 Ko ; `lp_render_rows_with` pour le second cœur), au lieu d'un
  second moteur de rendu entier (31 Ko). Deux sémaphores lui passent ses lignes et les reprennent ; tout est statique,
  donc rien ne peut échouer au démarrage, et une carte qui mettrait `CONFIG_FREERTOS_UNICORE` ne compile pas. Une seule
  bande part à la fois, donc l'envoi reste caché derrière le dessin de la suivante (une bande sur deux par cœur
  attendait l'envoi de l'autre : 37 images/s au lieu de 44). Le code du rendu (`render.c`, `raster.c`) tourne depuis la
  mémoire interne (IRAM, `firmware/components/lilpet/linker.lf`) : depuis la flash, les deux cœurs se disputaient le
  cache d'instructions de 16 Ko. Un test du moteur vérifie que deux brouillons qui se partagent les lignes donnent la
  même image.
- **Une image par carte**, choisie à la compilation (`BOARD`). Une carte est un dossier, `firmware/boards/<carte>/` :
  `board.cmake` liste ses composants, `board_config.h` ses broches, son montage, sa taille d'écran et le boîtier de sa
  puce, un `sdkconfig.defaults` facultatif ses options ESP-IDF. Le dossier donne son nom à la carte. CMake, la CI et `device-check` lisent ces dossiers. Chaque en-tête (`display.h`, `imu.h`) est l'interface, chaque
  composant un fichier `.c`, sans table de fonctions : tout est choisi à la compilation. Un seul binaire reconnaissant sa
  carte n'est pas possible : l'AtomS3R et le Waveshare 1,32" ont le même boîtier (ESP32-S3-PICO), et sonder un bus ou lire
  l'identifiant d'un écran obligerait à piloter des broches sur une carte inconnue. Ajouter un écran : un `.c` de 4
  fonctions et une ligne dans le `board.cmake`. Ajouter un capteur : un `.c` de 2 fonctions et une ligne. Ajouter une carte
  aux composants connus : son dossier, rien d'autre.
- **Mauvaise image** : deux gardes. `firmware:flash` (`scripts/flash-guard.py`) demande `> hello` à la carte et refuse si
  une autre carte répond, puis donne ce port à esptool. `FORCE=1` passe outre sans rien demander. Une carte vierge, ou qui
  redémarre en boucle, ne répond pas et passe. Au démarrage, `board_init` compare le boîtier
  de la puce et s'arrête avant de toucher une broche. Limite : une image `stamp_s3a` sur la XIAO n'est pas arrêtée, les
  deux sont en boîtier QFN56 ; le futur firmware de la XIAO devra répondre à `> hello` pour que le flash la protège.
- Le pilote de l'écran rond (`firmware/main/display_co5300.c`) reprend la démo ESP-IDF officielle de Waveshare pour la carte
  1,32" : broches, séquence de démarrage du CO5300, QSPI à 40 MHz, rotation à 180°, décalage de 6 colonnes
  ([démo](https://github.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.32/blob/main/Example/ESP-IDF/07_LVGL_V9_Test/components/lvgl_port_bsp/lvgl_port_bsp.cpp)).
  Il n'a encore jamais tourné sur la carte : au premier essai, vérifier le sens de l'image, le décalage, les couleurs
  et l'absence de déchirure.
- Le pilote de l'AtomS3R (`firmware/main/display_atoms3r.c`) reprend les réglages de la bibliothèque de M5Stack
  ([M5GFX](https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp), `board_M5AtomS3R`) : SPI à 40 MHz,
  identifiant lu pour distinguer les deux puces d'écran (ST7735S ou GC9107), séquence d'initialisation complète de
  chaque puce, décalages, orientation (bit BGR compris) et inversion des couleurs, rétroéclairage par une puce sur le
  bus I2C. Pas de signal TE : il cadence à 62,5 images/s (`display_paced.c`, des ticks de 1 ms ne font pas 60). Testé le 1er novembre 2026 sur un AtomS3R à GC9107 : image
  propre, couleurs justes. Sans la séquence du fabricant ni le bit BGR, l'écran montrait une bande de bruit en haut et
  des couleurs inversées. La variante ST7735S n'a pas encore tourné.
- Temps de dessin sur l'AtomS3R, yeux au repos en 128 px : 8,2 ms par image, puis 4,9 ms (novembre 2026) une fois
  le mélange des couleurs mis en `inline` et les divisions faites une fois par ligne plutôt qu'à chaque sous-ligne
  (la puce n'a pas de division rapide), et 3,6 ms le 6 novembre, avec les couleurs converties une fois par image et les
  pixels répétés réutilisés. Dessin plus envoi : 6,7 ms, pour 16 ms disponibles à 62,5 images/s. Sur les deux cœurs
  (8 novembre) : dessin 1,9 ms, dessin plus envoi 5,1 ms, image propre à l'œil.
- **Inclinaison** : sur l'AtomS3R, `firmware/main/imu_bmi270.c` lit l'accéléromètre du BMI270 à chaque image
  (démarrage d'après sa fiche technique, avec le fichier de configuration de Bosch), `main.c` lisse la mesure et la
  passe au moteur par `lp_senses` (lissage, secousse, anti-rebond du bouton, capteur muet : `engine/src/senses.c`, testé
  sur PC). Le Waveshare 1,32" n'a aucun capteur de mouvement (`imu_none.c`) : sans module ajouté, pas d'inclinaison ni
  de vertige sur l'objet final, et le toucher demandera un pilote pour le CST820.
- **Réactions** : le moteur reçoit des événements (`lp_eyes_react` : toucher, secousse) et repère seul la position à
  l'envers dans la gravité ; chaque événement mène à une réaction décrite comme une donnée (émotion, durée,
  étincelles), pour que l'app puisse un jour les changer. Le firmware ne fait que traduire le matériel : appui sur
  l'écran de l'AtomS3R (broche 41), secousse repérée par `lp_shake_update` sur la mesure brute, avant le lissage.
  Mesuré sur la carte : une secousse à la main monte jusqu'à 11 g et s'écarte de 3 à 6 g de 1 g en moyenne, vers
  6 Hz, contre 0,01 g au repos ; une petite secousse et une franche ont la même force, seule leur durée change. Elle
  compte après environ 0,4 s (un choc ou un coup de poignet bref ne comptent pas) ; le moteur la reçoit comme un état (`lp_eyes_set_shaking`), comme la position à l'envers.
- Sans écran ni PSRAM, sur la M5Stamp S3A déjà en ma possession, l'image `stamp_s3a` cadence à 62,5 images/s et ses
  bandes de 466 px partent sur des broches non branchées, donc les temps restent réalistes.
  Personne ne regarde : il joue en boucle l'animation de la mesure sur PC (`engine/tools/bench_script.h`), avec le
  même look (cherry), la même graine et le même pas fixe de 1/60 s, pour dessiner les mêmes images que
  `mise run engine:bench` : la part redessinée se compare, mais son temps de dessin est celui des deux cœurs, contre un
  seul sur PC. Avec un écran, les temps affichés suivent l'animation libre et ne se comparent pas.
