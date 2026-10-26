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

## Idées perso

_(à remplir)_
