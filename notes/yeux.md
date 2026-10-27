# Yeux

Analyse faite à partir de [references/starboy-eye-looks.png](../references/starboy-eye-looks.png).

## Base commune

- Deux ovales sur fond noir, très rapprochés et un peu inclinés vers l'intérieur.
- Une couleur unie pour l'œil (le « blanc »), sans dégradé.

## Les 4 types de pupilles

| Type | Clé | Description | Exemples sur la capture |
|---|---|---|---|
| Point | `dot` | Toute petite pupille ronde | Rouge, jaune, cyan, violet |
| Fente | `slit` | Ovale vertical fin, façon chat | Blanc et rouge, vert, jaune et noir |
| Grande ovale | `large` | Remplit la majeure partie de l'œil | Turquoise, bleu et rose |
| Ronde moyenne | `round` | Souvent avec un petit reflet blanc | Vert, kaki et bleu |

## Variantes

- Couleur de l'œil.
- Couleur de la pupille, **qui peut différer entre l'œil gauche et le droit** (ex. : jaune avec pupille rouge à gauche, bleue à droite).
- Reflet blanc ou non.
- L'œil réduit à un trait, sur la capture, est sûrement une image de clignement ou de sommeil, pas un look à part.

Un look tient donc en quelques valeurs. Format : voir [eyes/looks.json](../eyes/looks.json).

## Animations à faire

Idées tirées des techniques classiques d'animation d'yeux :

- **Clignements** à intervalle irrégulier.
- **Petits mouvements** constants de la pupille (saccades).
- **Suivi du visage** repéré par la caméra.
- **Squash & stretch** : l'œil s'écrase et s'étire pendant les mouvements.
- **Paupières** : caches noirs qui coupent l'ovale pour les émotions.
  - Colère : coin intérieur coupé en haut.
  - Tristesse : coin extérieur coupé en haut.
  - Joie (^^) : bas de l'œil coupé en arc.
- **Pupilles** qui grossissent ou rétrécissent selon l'humeur.

## Analyse vidéo de la galerie officielle (2026-10-27)

Source : enregistrement d'écran de la galerie creature.company/eyes (environ 27 images/s, donc environ 37 ms par image).
Planches : [vue d'ensemble](../references/eyes-gallery-overview.png), [clignement « dot »](../references/blink-dot-frames.png), [clignement « acorn » + mouvement de tête](../references/blink-acorn-headturn-frames.png).

### Noms officiels

- Types de pupilles : `dot` (10 %), `acorn` (20 %), d'autres à identifier.
- Couleurs nommées avec leur rareté, ex. `kite` (0,9 %) = œil bleu clair et pupille magenta.

### Style

- Aplats de couleur, bords nets, aucun contour, aucun reflet.
- Les yeux sont **gros** : chacun fait environ 40 % du diamètre de l'écran, et les deux se touchent presque.
- Les yeux se placent comme sur un **visage en 3D** :
  - la tête qui tourne fait grossir l'œil proche et rapetisser l'œil éloigné ;
  - la tête qui penche fait tourner toute la paire ;
  - quand la tête se tourne, les ovales s'aplatissent (perspective).
- Les pupilles `acorn` sont de grands ovales verticaux, placés en bas vers l'intérieur et **coupés par le bord de l'œil** : c'est ce qui leur donne leur forme de gland.
- Les pupilles `dot` sont de petits ovales verticaux, tournés vers l'intérieur : un regard légèrement louchon, mignon.

### Le clignement, image par image

1. **Anticipation** (1-2 images) : l'œil s'élargit un peu et le haut s'aplatit.
2. **Fermeture rapide** (2-3 images, environ 80-110 ms) : une paupière droite descend du haut, inclinée comme la tête. Les pupilles ne bougent pas : elles sont simplement recouvertes. L'œil devient une demi-lune, puis un croissant.
3. **Fermé** (2-8 images, environ 75-300 ms) : un **fin trait courbe** (environ 3 px, couleur de l'œil) qui suit le bas de l'œil, comme un sourire.
4. **Ouverture plus lente** (4-6 images, environ 150-220 ms) : la paupière remonte.
5. Le clignement sert souvent à **masquer un grand changement de regard ou de tête** : les yeux se ferment dans une pose et se rouvrent dans une autre.

## Idées perso

_(à remplir)_
