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
- **Image en RGB565** (2 octets par pixel, environ 434 Ko pour l'écran entier), en PSRAM sur l'ESP32.
- **Ne renvoyer que ce qui bouge** (zones modifiées). À 40 MHz en QSPI, l'horloge utilisée pour cet écran
  ([LCD Wizard](https://lcdwizard.com/boards/waveshare-s3-touch-amoled-132)), renvoyer tout l'écran plafonne vers
  45 images/s (estimation) ; la zone des yeux seule (environ 30 % de l'écran) laisse de la marge pour 60 images/s.
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

## Organisation cible du dépôt

| Dossier      | Rôle                                                                           | État                    |
| ------------ | ------------------------------------------------------------------------------ | ----------------------- |
| `prototype/` | Référence de design en JS. Scènes de référence dans `prototype/scenes.json`    | Fait                    |
| `eyes/`      | Données des looks (puis des animations)                                        | Fait                    |
| `engine/`    | Moteur C17 : état, animations, dessin. Aucune dépendance, aucun accès matériel | Étape 2                 |
| `app/`       | App PC : TypeScript + Vite, moteur en WebAssembly, éditeur, Web Serial         | Étape 4                 |
| `firmware/`  | ESP-IDF : pilote d'écran, caméra, capteurs, appelle le moteur                  | À l'arrivée du matériel |
| `scripts/`   | Outils du dépôt (validation des données…)                                      | Fait                    |

## Règle de parité

Le prototype JS est la référence visuelle tant que le moteur C n'a pas tout repris. Chaque scène de
`prototype/scenes.json` (look, émotion, phase du clignement, pose de la tête, regard, particules à graine) doit être
rendue de la même façon par le moteur. Les bords lissés ne seront pas identiques au pixel près (Canvas 2D et notre
dessin ligne par ligne lissent différemment), donc la comparaison tolère un petit écart. Une fois une fonction portée,
le moteur devient la source de vérité pour elle.

## Outils prévus à l'étape 2

- **CMake** (déjà dans mise), utilisé par ESP-IDF et par Emscripten.
- **Emscripten** pour la version WebAssembly.
- **clang-format** (formatage) et **clang-tidy** (analyse), l'équivalent de prettier et oxlint côté C.
- **Tests unitaires** du moteur lancés sur le PC.
- **CI** : formatage et analyse du C, tests, compilation WebAssembly, puis compilation du firmware avec ESP-IDF.
- Versions des outils déclarées dans `mise.toml` quand c'est possible, comme Node.
