# 1. Un seul moteur des yeux, en C

- Date : 2026-10-27
- Statut : accepté

## Contexte

Les yeux doivent tourner à trois endroits : sur l'objet (ESP32-S3 et écran rond AMOLED de 466 × 466), dans un aperçu
sur PC et dans l'app PC qui change les yeux et les animations. Le premier prototype, en JavaScript (Canvas 2D),
suffisait pour explorer le design, mais ne pouvait pas tourner sur l'ESP32 : tout ce qui y était ajouté aurait dû être
réécrit en C pour l'objet.

| Option                                            | Pour                                                          | Contre                                                                              |
| ------------------------------------------------- | ------------------------------------------------------------- | ----------------------------------------------------------------------------------- |
| Rester en JS, réécrire en C au moment du firmware | Rien à changer maintenant                                     | Deux rendus à maintenir, écarts visuels, réécriture de plus en plus longue          |
| LVGL (bibliothèque d'interface)                   | Pilotes d'écran fournis, simulateur PC                        | Lourd pour deux yeux, peu adapté aux paupières découpées                            |
| Moteur en Rust (embarqué + WebAssembly)           | Langage moderne et sûr                                        | Outils pour la puce Xtensa de l'ESP32-S3 moins mûrs, langage nouveau pour le projet |
| **Moteur en C partagé (ESP-IDF + Emscripten)**    | Un seul rendu, pixel pour pixel identique partout, C maîtrisé | Il faut écrire le dessin soi-même                                                   |

## Décision

Un seul moteur des yeux en C17 (`engine/`), sans dépendance ni accès au matériel, compilé :

- pour l'ESP32-S3 avec ESP-IDF, l'outil officiel d'Espressif (écrit en C) ;
- en WebAssembly avec Emscripten, pour l'aperçu et l'app PC ;
- pour le PC, où tournent ses tests.

Un projet partage déjà un moteur de rendu C entre ESP32 AMOLED et navigateur :
[gea-embedded](https://github.com/dashersw/gea-embedded).

## Conséquences

- Un seul rendu à maintenir, le même sur l'objet et dans le navigateur.
- Le dessin est écrit à la main ([décision 2](0002-rendu-par-lignes.md)).
- Le prototype JS a été retiré en novembre 2026, une fois tout repris par le moteur ; les rendus de référence ont pris
  sa place de référence ([décision 3](0003-rendus-de-reference.md)).
