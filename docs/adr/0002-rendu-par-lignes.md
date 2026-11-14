# 2. Dessin ligne par ligne, en RGB565, sans bibliothèque graphique

- Date : 2026-10-27
- Statut : accepté

## Contexte

Les yeux ne sont que des ellipses découpées par des droites (paupières) et par d'autres ellipses (joie, croissant).
L'ESP32-S3 a peu de mémoire interne, calcule lentement en `double`, et son écran rond reçoit l'image par QSPI.

## Décision

- **Pour chaque ligne de l'écran, le moteur calcule directement les segments à remplir**, avec un lissage des bords
  tiré de la couverture du pixel.
- **Image en RGB565** : 2 octets par pixel, environ 434 Ko pour l'écran entier.
- **Toute taille d'écran carré jusqu'à 466 px** (`lp_renderer_set_size`), par exemple 128 pour l'AtomS3R : le dessin
  reste calculé en coordonnées 466 et n'est réduit qu'en sortie. L'image est la même, juste plus petite (à 128 px,
  écart moyen de 0,06 à 0,21 avec l'image 466 réduite).
- **Dessin par bandes, sans image entière en mémoire.** Le moteur ne relit jamais l'image précédente : chaque ligne de
  la zone modifiée est redessinée de zéro. Sur l'objet, il dessine donc quelques lignes à la fois (30 lignes, 28 Ko,
  comme les démos Waveshare) en mémoire interne, plus rapide que la PSRAM, et chaque bande part vers l'écran pendant
  qu'il dessine la suivante (`lp_render_begin` puis `lp_render_rows`). `lp_render` dessine l'image entière d'un coup,
  pour le PC, le WebAssembly et les tests.
- **Ne renvoyer que ce qui bouge** (zones modifiées).
- **`float` et entiers, jamais `double`** : sur l'ESP32-S3, les `double` sont émulés en logiciel et très lents, et même
  les `float` restent plus lents que les entiers
  ([guide de performance ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/performance/speed.html)).
- **Hasard à graine** (mulberry32) : la même suite de nombres sur toutes les machines, donc les mêmes particules à
  graine égale.

## Conséquences

- La zone modifiée a toujours des coordonnées paires : le contrôleur de l'écran (CO5300) n'accepte que des fenêtres qui
  commencent sur un pixel pair et de taille paire.
- À 40 MHz en QSPI, l'horloge utilisée pour cet écran
  ([LCD Wizard](https://lcdwizard.com/boards/waveshare-s3-touch-amoled-132)), renvoyer tout l'écran plafonnerait vers
  45 images/s (estimation). Mesuré par `mise run engine:bench`, la zone redessinée couvre 50 % de l'écran en moyenne et
  70 % au pire (particules du mode amoureux), soit environ 11 ms et 16 ms d'envoi par image (estimation) : sous les
  16,7 ms d'une image à 60 images/s, mais avec très peu de marge au pire.
- Le compilateur refuse tout `double` implicite (`-Wdouble-promotion`, avertissements traités comme des erreurs).
- Le rendu se partage entre les deux cœurs de la puce ([décision 9](0009-rendu-deux-coeurs.md)).
