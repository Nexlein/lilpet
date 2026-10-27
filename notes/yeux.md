# Yeux

Analyse faite à partir de [references/starboy-eye-looks.png](../references/starboy-eye-looks.png).

## Base commune

- Deux ovales sur fond noir, très rapprochés et un peu inclinés vers l'intérieur.
- Une couleur unie pour l'œil (le « blanc »), sans dégradé.

## Les 4 types de pupilles

| Type                 | Clé     | Description                                                                   | Exemples sur la capture             |
| -------------------- | ------- | ----------------------------------------------------------------------------- | ----------------------------------- |
| Point                | `dot`   | Toute petite pupille ronde                                                    | Rouge, jaune, cyan, violet          |
| Fente                | `slit`  | Ovale vertical fin, façon chat                                                | Blanc et rouge, vert, jaune et noir |
| Gland (nom officiel) | `acorn` | Grand œuf vertical, plus large en bas, posé bas et coupé par le bord de l'œil | Turquoise, bleu et rose             |
| Ronde moyenne        | `round` | Souvent avec un petit reflet blanc                                            | Vert, kaki et bleu                  |

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
- Mesures de face (en fraction du diamètre de l'écran D) : demi-axes 0,20 D × 0,235 D, centres à ±0,22 D, inclinés
  d'environ 21° vers l'extérieur. Le contour est une **vraie ellipse** (écart ≤ 2 % sur tout le tour) : pas de forme
  d'œuf, l'impression vient de l'inclinaison et des pupilles `acorn` coupées par le bord.
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

## Analyse de deux TikTok d'utilisateurs (2026-10-27)

Sources : vidéos @lilguynet et @pikespicks, fournies par Nielsen (non versionnées).
Planches : [cœurs](../references/tiktok-love-hearts.png), [inclinaison](../references/tiktok-tilt.png), [colère](../references/tiktok-angry.png), [choix des yeux](../references/tiktok-eye-picking.png), [réaction au geste](../references/tiktok-gesture-reaction.png).

### Nouvelles expressions

- **Amoureux** : les yeux plissés de joie, collés en haut de l'écran, avec un petit reflet blanc. Des **cœurs rouges** montent du bas de l'écran en dérivant.
- **Colère** (doigt d'honneur, ou tenu à l'envers) :
    - d'abord un **plissement extrême** : les deux paupières se ferment jusqu'à une barre horizontale épaisse aux bouts arrondis ;
    - puis des yeux réduits à des **croissants du haut** (⌒⌒), coins intérieurs vers le bas, comme des sourcils froncés ;
    - ou bien une paupière en diagonale très marquée (▼▼) avec de petites pupilles blanches en bas, côté intérieur.
- **Fermé, endormi** : deux traits courbes ‿‿, parfois avec une étincelle à côté.
- **Content, satisfait** : œil en dôme, avec le bas coupé à plat.
- **Méfiant ou endormi** : paupière du haut plate, à mi-hauteur.

### Effets et comportements

- **Particules** : cœurs qui montent, **étincelles** blanches à 4 branches (✦) qui scintillent autour des yeux.
- **Choix des yeux** (premier démarrage, « picking his eyes for life ») : défilé rapide de looks façon machine à sous, avec des étincelles, puis arrêt sur le look définitif.
- **Formes spéciales** : des yeux en forme de **flammes** (jaune et orange), et des flammes posées au-dessus des yeux.
- **Hétérochromie** visible : yeux blancs, pupille orange à gauche, cyan à droite.
- **Inclinaison** : quand on penche l'objet, les yeux glissent et tournent selon la gravité (accéléromètre).
- **Taille** : les yeux occupent presque tout l'écran et se touchent souvent.
- Sur l'écran réel, les couleurs « bavent » un peu (lueur OLED à travers le verre bombé).

## Idées perso

_(à remplir)_
