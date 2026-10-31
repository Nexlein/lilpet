# Yeux

Analyse faite à partir de [references/starboy-eye-looks.png](../references/starboy-eye-looks.png).

## Base commune

- Deux ovales sur fond noir, très rapprochés et un peu inclinés vers l'intérieur.
- Une couleur unie pour l'œil (le « blanc »), sans dégradé.

## Les 4 types de pupilles

| Type                 | Clé      | Description                                                        | Exemples sur la capture             |
| -------------------- | -------- | ------------------------------------------------------------------ | ----------------------------------- |
| Point                | `dot`    | Toute petite pupille ronde                                         | Rouge, jaune, cyan, violet          |
| Fente                | `cat`    | Ovale vertical fin, façon chat                                     | Blanc et rouge, vert, jaune et noir |
| Gland (nom officiel) | `acorn`  | Grande ellipse verticale posée bas, coupée par le bas de l'œil     | Turquoise, bleu et rose             |
| Ronde moyenne        | `circle` | Disque moyen (reflet sur la capture, absent du catalogue officiel) | Vert, kaki et bleu                  |

## Variantes

- Couleur de l'œil.
- Couleur de la pupille, **qui peut différer entre l'œil gauche et le droit** (ex. : jaune avec pupille rouge à gauche, bleue à droite).
- Reflet blanc ou non (aucun dans le catalogue officiel : seulement avec certaines émotions).
- L'œil réduit à un trait, sur la capture, est sûrement une image de clignement ou de sommeil, pas un look à part.

Un look = une couleur de [eyes/looks.json](../eyes/looks.json) + un type de pupille.

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

- Types de pupilles : `dot` (10 %), `circle` (40 %), `cat` (30 %), `acorn` (20 %). Liste complète des couleurs : voir
  [Catalogue officiel](#catalogue-officiel-2026-10-31).
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
3. **Fermé** (2-8 images, environ 75-300 ms) : un **fin trait** couleur de l'œil, à plat, en forme de bas d'ellipse
   aplatie : fond plat, bouts qui remontent, épaisseur régulière et bouts arrondis. Mesures sur 7 images (fraction du
   diamètre de l'écran D) : largeur 0,31-0,34 D, hauteur 0,060-0,066 D, épaisseur 0,009-0,011 D, point le plus bas
   0,164 D sous le centre de l'écran (environ 70 % de la hauteur de l'œil).
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

## Catalogue officiel (2026-10-31)

Source : la page [creature.company/eyes](https://creature.company/eyes?swatch=dot-socks), mesurée dans le navigateur
sur le rendu : couleurs lues sur les appels de dessin du canvas (valeurs exactes), formes mesurées sur les pixels du
grand œil central, yeux au repos. Aucun fichier du site téléchargé. Les noms officiels servent de repère, pour un
usage perso.

### La page

- Un look = **une couleur** (`swatch`, 100 en tout) × **un type de pupille** (4). L'adresse les donne :
  `?swatch=<type>-<couleur>`, par exemple `?swatch=cat-kite`. Sans paramètre, la page ouvre `marmalade`.
- Les couleurs sont rangées sur une grille hexagonale de 10 × 10 cases qui boucle dans les deux sens ; les trois
  petites bulles autour de l'œil central montrent les trois autres types de pupille.
- Chaque couleur et chaque type affiche sa rareté (« 0,9 % chance »). Les 100 raretés affichées font 100,5 % (arrondis) :
  la liste est complète.
- Sur la page, les yeux respirent (montée et descente d'environ ±8 px), clignent et les pupilles dérivent de quelques
  pixels.

### Les couleurs

- Le blanc de l'œil est **toujours le même** pour les deux yeux.
- La pupille diffère entre gauche et droite sur **14 couleurs**, toutes au palier le plus rare (0,4 %).
- Paliers de rareté : 1,9 % (14 couleurs), 1,7 % (12), 1,5 % (5), 1,3 % (5), 0,9 % (13), 0,6 % (37), 0,4 % (14).
- Pupille plus claire que l'œil sur plusieurs couleurs (pupille blanche : `bubblegum`, `gumball`, `sprinkler`).
- **Aucun reflet** : chaque œil n'est dessiné qu'avec deux aplats, le blanc puis la pupille, quel que soit le type.

### Les types de pupille

Mesures sur `juicebox`, moyenne des deux yeux (œil de 160 × 180 px pour un visage de 394 px de diamètre), converties en
fraction des demi-axes de l'œil RX et RY ; x vers le nez, y vers le bas, depuis le centre de l'œil.

| Type     | Rareté | Taille (demi-largeur × demi-hauteur) | Position au repos (x ; y) | Forme                                                |
| -------- | ------ | ------------------------------------ | ------------------------- | ---------------------------------------------------- |
| `dot`    | 10 %   | 0,105 RX × 0,125 RY                  | 0,57 RX ; +0,12 RY        | petit ovale vertical                                 |
| `circle` | 40 %   | disque de rayon 0,52 RX              | 0,36 RX ; 0               | disque d'environ la moitié de la largeur de l'œil    |
| `cat`    | 30 %   | 0,21 RX × 0,72 RY                    | 0,35 RX ; +0,11 RY        | fente en ovale                                       |
| `acorn`  | 20 %   | 0,50 RX × 0,98 RY                    | 0,18 RX ; +0,25 RY        | grande ellipse posée bas, coupée par le bas de l'œil |

- La forme de gland vient seulement de la coupe : sur 10 niveaux mesurés, la partie visible suit une ellipse à 0,02 RX
  près. Pas d'œuf dessiné à la main.
- Les pupilles penchent d'environ 5°, le haut vers le nez (mesuré sur `cat` et `acorn`, aux deux yeux), à l'inverse
  de l'inclinaison de l'œil.
- Prototype et moteur suivent ces valeurs (`PUPIL_REST`, `PUPIL_LEAN` et `#drawPupil` dans `prototype/eyes.js`). Notre
  rendu, mesuré de la même façon que la page officielle, tombe à 0,011 largeur d'œil près au pire (largeur du `dot`).
- Looks : repris tels quels dans `eyes/looks.json`, sans reflet (le reflet ne vient qu'avec certaines émotions).

### Les 100 couleurs

`=` : même couleur de pupille que l'œil gauche.

| Nom         | Rareté | Œil       | Pupille gauche | Pupille droite |
| ----------- | ------ | --------- | -------------- | -------------- |
| avocado     | 1,3 %  | `#8BD67C` | `#230606`      | =              |
| beachball   | 0,6 %  | `#F6FD21` | `#1C9DE3`      | =              |
| blacklight  | 0,6 %  | `#6922F0` | `#A3F410`      | =              |
| bubblegum   | 1,5 %  | `#F442E7` | `#FFFFFF`      | =              |
| bumblebee   | 0,4 %  | `#FDCB21` | `#0A0A0A`      | `#D10566`      |
| buoy        | 0,6 %  | `#14ABD6` | `#EEFA24`      | =              |
| cactus      | 0,6 %  | `#C4F41D` | `#530AEC`      | =              |
| calculator  | 0,6 %  | `#737373` | `#09E151`      | =              |
| candle      | 0,6 %  | `#973849` | `#F7FBBC`      | =              |
| candycane   | 1,7 %  | `#9CFBD5` | `#790C05`      | =              |
| chamomile   | 0,9 %  | `#509156` | `#F4C524`      | =              |
| cherry      | 1,9 %  | `#AE0002` | `#FF5252`      | =              |
| crocus      | 0,6 %  | `#D602E8` | `#FBF823`      | =              |
| cupcake     | 1,7 %  | `#F9FFB2` | `#CA00CA`      | =              |
| denim       | 1,9 %  | `#6C92F8` | `#102A6E`      | =              |
| doorbell    | 0,6 %  | `#BEBEBE` | `#E42D06`      | =              |
| dragonfruit | 0,6 %  | `#B60053` | `#CFFA0F`      | =              |
| eggplant    | 1,3 %  | `#E202E8` | `#0A0A0A`      | =              |
| ember       | 0,6 %  | `#994400` | `#00EEFF`      | =              |
| eraser      | 0,4 %  | `#666666` | `#FFEDED`      | `#000000`      |
| flamingo    | 0,9 %  | `#FCB7C3` | `#068F9A`      | =              |
| flowerpot   | 0,9 %  | `#CC6262` | `#3E4002`      | =              |
| flytrap     | 0,6 %  | `#15F817` | `#5E045E`      | =              |
| frog        | 1,9 %  | `#42DE86` | `#436A16`      | =              |
| glowstick   | 0,6 %  | `#E9F905` | `#360342`      | =              |
| glowworm    | 0,6 %  | `#F40DF8` | `#19F515`      | =              |
| goldfish    | 1,7 %  | `#E0F4FB` | `#DE9109`      | =              |
| guava       | 0,6 %  | `#84FB8E` | `#F64982`      | =              |
| gumball     | 1,5 %  | `#F20F07` | `#FFFFFF`      | =              |
| hibiscus    | 0,6 %  | `#B6003C` | `#05E605`      | =              |
| highlighter | 0,6 %  | `#95F124` | `#F50DCF`      | =              |
| houseplant  | 1,9 %  | `#039442` | `#71FF6F`      | =              |
| hydrangea   | 1,7 %  | `#E1BFE1` | `#2722DB`      | =              |
| jam         | 0,6 %  | `#C5013C` | `#09CE93`      | =              |
| jawbreaker  | 1,9 %  | `#F6759F` | `#56031F`      | =              |
| jelly       | 0,6 %  | `#B60080` | `#429EFB`      | =              |
| juicebox    | 0,9 %  | `#F6826C` | `#95059B`      | =              |
| jukebox     | 0,6 %  | `#024BDE` | `#FB4CC3`      | =              |
| junebug     | 0,6 %  | `#14DA70` | `#3516AF`      | =              |
| kite        | 0,9 %  | `#7DB5F4` | `#C10787`      | =              |
| kiwi        | 0,6 %  | `#55F927` | `#9F7717`      | =              |
| koi         | 0,4 %  | `#FE6873` | `#0A0A0A`      | `#D0F910`      |
| ladybug     | 1,3 %  | `#C3110E` | `#000000`      | =              |
| laser       | 1,7 %  | `#FFFFFF` | `#FF0000`      | =              |
| lilac       | 0,6 %  | `#7202FC` | `#FDB0CE`      | =              |
| lilypad     | 0,4 %  | `#D1F63B` | `#10812B`      | `#0F91F4`      |
| limeade     | 1,7 %  | `#EAFCC5` | `#09B6CE`      | =              |
| lipgloss    | 1,9 %  | `#A10180` | `#FC609C`      | =              |
| lobster     | 0,6 %  | `#F95320` | `#044A5F`      | =              |
| mallard     | 0,9 %  | `#7B8401` | `#0737A7`      | =              |
| marble      | 0,4 %  | `#D1D9FA` | `#C11207`      | `#D20ADF`      |
| marmalade   | 1,9 %  | `#F99F05` | `#6E6123`      | =              |
| matcha      | 1,9 %  | `#C7FBA6` | `#5E6E06`      | =              |
| moon        | 1,5 %  | `#4028FF` | `#EBFFFC`      | =              |
| motel       | 0,6 %  | `#FA486F` | `#92FABE`      | =              |
| moth        | 0,9 %  | `#EC75F6` | `#564803`      | =              |
| neapolitan  | 0,4 %  | `#87493B` | `#EB76DD`      | `#B7ABF3`      |
| nightlight  | 0,6 %  | `#7768FE` | `#55FC87`      | =              |
| og          | 1,3 %  | `#FFFFFF` | `#000000`      | =              |
| parakeet    | 0,6 %  | `#018335` | `#23FAFB`      | =              |
| peach       | 1,7 %  | `#FDFBE2` | `#F76E5D`      | =              |
| peacock     | 0,9 %  | `#02B0B6` | `#7923FB`      | =              |
| pebble      | 1,9 %  | `#645252` | `#A4A4A4`      | =              |
| petunia     | 1,9 %  | `#F8AFFB` | `#F006B4`      | =              |
| pinball     | 0,4 %  | `#B60207` | `#D2F9F9`      | `#60D105`      |
| plum        | 1,9 %  | `#7260E6` | `#622058`      | =              |
| pool        | 1,5 %  | `#05A8F9` | `#FCEEEE`      | =              |
| popcorn     | 0,4 %  | `#EAF66E` | `#BB240A`      | `#0964E7`      |
| popsicle    | 0,6 %  | `#71FEAE` | `#5598FC`      | =              |
| postcard    | 0,9 %  | `#BE6A6A` | `#76D8EB`      | =              |
| puddle      | 0,9 %  | `#A7AFF6` | `#837605`      | =              |
| pumpkin     | 0,6 %  | `#E87102` | `#55FC6E`      | =              |
| robin       | 0,6 %  | `#15ABF8` | `#5E2304`      | =              |
| rosebush    | 0,6 %  | `#1E6935` | `#FC7AC0`      | =              |
| sandbox     | 0,9 %  | `#B8804A` | `#F7F574`      | =              |
| seaglass    | 1,7 %  | `#FBFBFB` | `#167B61`      | =              |
| seashell    | 1,7 %  | `#FCD9CF` | `#070571`      | =              |
| sherbet     | 0,4 %  | `#E4F4E2` | `#4A0A99`      | `#DB8405`      |
| siren       | 0,6 %  | `#FD0C0D` | `#2905E6`      | =              |
| slushie     | 0,6 %  | `#C206AD` | `#49F6D9`      | =              |
| smoothie    | 1,7 %  | `#FFECE0` | `#CE0959`      | =              |
| snowball    | 1,7 %  | `#FFFFFF` | `#5A79F3`      | =              |
| socks       | 0,4 %  | `#B865A8` | `#76D8EB`      | `#8F1716`      |
| sonar       | 0,6 %  | `#126487` | `#2EE605`      | =              |
| spearmint   | 0,6 %  | `#9CFBCE` | `#790572`      | =              |
| sprinkler   | 1,5 %  | `#59BF05` | `#FFFFFF`      | =              |
| spumoni     | 0,4 %  | `#F6EAB9` | `#177E39`      | `#2722DB`      |
| starboy     | 0,6 %  | `#7B43F5` | `#F6BD49`      | =              |
| static      | 0,4 %  | `#666666` | `#000000`      | `#FFFFFF`      |
| stoplight   | 0,4 %  | `#34F7FD` | `#038C03`      | `#F91024`      |
| strawberry  | 0,9 %  | `#FCB7F2` | `#069A1E`      | =              |
| submarine   | 1,3 %  | `#0F7BA9` | `#000000`      | =              |
| sunset      | 0,6 %  | `#AE2400` | `#F855FC`      | =              |
| taffy       | 0,6 %  | `#F474DC` | `#E8FCA6`      | =              |
| teacup      | 1,7 %  | `#F6E5A5` | `#0D73F7`      | =              |
| terrarium   | 1,9 %  | `#6FF5D0` | `#106E54`      | =              |
| tomato      | 0,9 %  | `#60A611` | `#B4040F`      | =              |
| umbrella    | 0,4 %  | `#E7E3E9` | `#F65F28`      | `#4F6BF8`      |
| valentine   | 1,9 %  | `#DD06CB` | `#7B1612`      | =              |
| whale       | 1,9 %  | `#0059A3` | `#0095FF`      | =              |

## Idées perso

_(à remplir)_
