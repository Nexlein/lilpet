# 3. Rendus de référence tirés du build WebAssembly

- Date : 2026-11-09
- Statut : accepté

## Contexte

Le moteur C est la seule référence des yeux ([décision 1](0001-moteur-c-partage.md)). Jusqu'en novembre 2026, il était
comparé au prototype JS, scène par scène avec une petite tolérance pour les bords lissés, et sur des valeurs de logique
calculées par le prototype. Chaque changement de comportement s'écrivait donc deux fois, et le prototype a été retiré.
Il fallait une référence tirée du moteur lui-même, qui ne bouge que quand on le veut.

## Décision

- `scripts/goldens.mjs` fait dessiner par le moteur compilé en WebAssembly chaque scène de `engine/tests/scenes.json`
  (look, émotion, phase du clignement, pose de la tête, regard, particules à graine), gardée en PNG dans
  `engine/tests/goldens/`.
- Il joue aussi une animation scriptée (repos, suivi du pointeur, objet tourné, presque à plat, à l'envers, secoué,
  triste, cœurs, caresse, réactions à la caméra, plissé, endormi), par pas de 1/64 s, depuis quatre graines et quatre
  looks. Une image sur 16 est gardée sous forme d'empreinte dans `animation.txt`, et un PNG des fins de partie du
  premier passage reste pour l'œil. Une image sur 16, parce que les entrées changent au milieu des mouvements : une
  vitesse ou une rampe modifiée se voit, ce qu'une image prise à la fin de chaque partie ne montrerait pas. Trois lignes
  du moteur modifiées (rampe de l'inclinaison, regard derrière un clignement, vitesse de suivi du pointeur) passaient
  avec les seules fins de partie.
- Le build WebAssembly, parce qu'il donne les mêmes pixels sur toutes les machines : pas de multiplication-addition
  fusionnée (FMA), et sa propre libm. Un build natif peut utiliser FMA, ce qui change quelques arrondis, donc quelques
  pixels.
- Tout est comparé exactement par `mise run engine:goldens`, donc par `engine:check`, `mise run check` et le job du
  moteur C en CI. Ce qui diffère fait échouer le contrôle et va dans `engine/build/goldens/` : l'image dessinée, avec le
  nombre de pixels qui changent, ou la première image de l'animation qui diffère. La CI le garde comme artefact.
- La logique (hasard, émotions, réactions, regard, inclinaison, particules) est vérifiée à part, avec des valeurs
  écrites en dur dans les tests C (`engine/tests/`).

## Conséquences

- Un changement voulu de l'apparence ou du mouvement réécrit les références (`npm run goldens`, voir le README), qu'on
  regarde avant de les commiter, et un changement visuel vient avec une scène qui le montre.
- Le hasard donne des nombres de 24 bits, exacts dans un `float`, et le pas de 1/64 s garde le temps exact en `float`.
