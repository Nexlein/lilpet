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
  45 images/s (estimation). Mesuré par `mise run engine:bench` : la zone redessinée couvre 42 % de l'écran en moyenne
  et 67 % au pire (particules du mode amoureux), soit environ 9 ms et 15 ms d'envoi par image (estimation), sous les
  16,7 ms d'une image à 60 images/s, mais avec peu de marge au pire.
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

| Dossier      | Rôle                                                                           | État                   |
| ------------ | ------------------------------------------------------------------------------ | ---------------------- |
| `prototype/` | Référence de design en JS. Scènes de référence dans `prototype/scenes.json`    | Fait                   |
| `eyes/`      | Données des looks (puis des animations)                                        | Fait                   |
| `engine/`    | Moteur C17 : état, animations, dessin. Aucune dépendance, aucun accès matériel | Fait (étape 2)         |
| `app/`       | App PC : TypeScript + Vite, moteur en WebAssembly, éditeur, Web Serial         | Étape 4                |
| `firmware/`  | ESP-IDF : pilote d'écran, caméra, capteurs, appelle le moteur                  | Compilation seule (2a) |
| `scripts/`   | Outils du dépôt (validation des données…)                                      | Fait                   |

## Règle de parité

Le prototype JS est la référence visuelle tant que le moteur C n'a pas tout repris. Chaque scène de
`prototype/scenes.json` (look, émotion, phase du clignement, pose de la tête, regard, particules à graine) doit être
rendue de la même façon par le moteur. Les bords lissés ne seront pas identiques au pixel près (Canvas 2D et notre
dessin ligne par ligne lissent différemment), donc la comparaison tolère un petit écart. Une fois une fonction portée,
le moteur devient la source de vérité pour elle.

Pour l'image, `prototype/reference.html` rend chaque scène avec le prototype et avec le moteur compilé en WebAssembly,
côte à côte, et calcule l'écart. Tolérance : écart moyen ≤ 2 sur 255 par canal, et au plus 0,5 % de pixels qui
diffèrent de plus de 32. Le test Playwright `tests/parity.spec.mjs` fait échouer la CI si une scène sort de la
tolérance (fin octobre 2026 : 33/33, écart moyen le plus haut 1,24).

Pour la logique (hasard, émotions, clignement, regard, tête, particules), la comparaison est **exacte** : le script
`scripts/gen-engine-vectors.mjs` fait tourner le prototype et écrit les valeurs attendues dans
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
  ajoute son propre clang et son propre node au PATH : ils masquaient ceux du système et faisaient boucler ccache.
- **ESP-IDF seulement en CI** pour l'instant (`espressif/esp-idf-ci-action`, dernière version d'ESP-IDF) : la
  mini-app `firmware/` prouve que le moteur compile pour l'ESP32-S3, sans matériel.
- **Tâches mise** (`mise run engine:check`, etc.), lancées telles quelles par la CI.
- Tout suit la dernière version (`latest`), comme Node avec `lts`.

## Performance (mise en place en 2e)

- `mise run engine:bench` compile le moteur en mode optimisé et mesure le temps de rendu :
    - pour chaque scène de référence, l'écran entier (première image) puis les yeux seuls (image suivante) ;
    - sur une animation de 12 s à 60 images/s : chaque émotion pendant une seconde, avec saccades, clignements et
      particules, puis le regard qui suit un visage. Chaque image ne redessine que sa zone modifiée, comme sur
      l'objet.
- En CI, les tableaux vont dans le résumé du job « C engine », pour comparer d'un commit à l'autre. Il n'y a pas de
  seuil qui fait échouer la CI : les machines partagées de GitHub sont trop irrégulières pour ça.
- Fin octobre 2026, sur un AMD Ryzen 7 7730U : 0,34 ms par image en moyenne sur l'animation, 0,51 ms au pire. Les
  pupilles « gland », dessinées comme des polygones, coûtent environ 65 % de plus que les autres.
- Ces chiffres ne servent qu'à repérer les régressions : l'ESP32-S3 (240 MHz) est beaucoup plus lent qu'un PC. La
  vraie mesure se fera sur l'objet, avec le firmware.
