# L'objet v1

Ce que doit faire le premier objet fini, décidé le 2026-11-17 ([#46](https://github.com/Nexlein/lilpet/issues/46)).
Les détails marqués **(proposé)** restent à confirmer, au plus tard au banc d'essai. Cette page sert de cible aux étapes
qui suivent ; leur ordre est dans le [README](../README.md#où-on-en-est).

## Les choix

- **Ce qu'il fait** : d'abord paraître vivant. Il se réveille quand on le prend en main, regarde la personne et
  la suit des yeux, réagit au toucher, aux secousses et au pouce levé, et montre quand sa batterie faiblit.
- **Deux jalons.** Le jalon A est l'objet complet sans caméra ; le jalon B ajoute la carte vision par-dessus. La caméra
  est la partie la plus chère en énergie, en épaisseur et en travail (une deuxième carte, une liaison UART, un modèle) :
  elle arrive sur un objet qui marche déjà.
- **Autonomie** : une journée, endormi presque tout le temps.
- **App PC** : telle quelle.
- **Objet** : la v1 ne cherche pas à être petite ; la taille viendra avec le boîtier.
- **Vie privée** : aucune image ne quitte la carte vision, et rien n'est enregistré. Seules des commandes texte en
  sortent, comme `> face 0.2 -0.1` ou `> react gesture_like` ([protocole.md](protocole.md)).

## Ce qu'il perçoit, et ce qu'il en fait

Les réactions sont celles de `eyes/emotions.json`, que l'app permettra un jour de modifier.

### Jalon A : l'objet sans caméra

| Il perçoit                            | Par                                                        | Il réagit                                                                                                       | Déjà prêt                                                                             |
| ------------------------------------- | ---------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------- |
| Une caresse                           | L'écran tactile de la carte écran (CST9217)                | Joie et étincelles (`tap`)                                                                                      | La réaction ; le pilote du CST9217 reste à écrire                                     |
| Une secousse, l'envers, l'inclinaison | L'accéléromètre QMI8658 de la carte écran                  | Vertige (`shake`) ; colère tête en bas (`upside_down`) ; le visage reste droit et les yeux glissent du côté bas | Le moteur, essayé sur l'AtomS3R avec son BMI270 ; le pilote du QMI8658 reste à écrire |
| La prise en main                      | L'interruption de mouvement du même accéléromètre (GPIO21) | Il se réveille, quitte l'air ensommeillé et reprend ses coups d'œil                                             | Sur l'AtomS3R, en lisant le mouvement chaque seconde                                  |
| Le bouton                             | Le bouton marche/arrêt de la carte écran (par son AXP2101) | (proposé) Un appui le réveille, un appui long l'éteint (AXP2101)                                                | Le réveil, sur l'AtomS3R ; l'arrêt reste à écrire                                     |
| Sa batterie                           | La jauge de l'AXP2101 de la carte écran                    | (proposé) Batterie faible : il a l'air fatigué (`tired`) ; en charge : une petite animation                     | L'air fatigué, sur l'AtomS3R ; voir la charge reste à faire                           |
| Personne                              | Ni toucher ni mouvement                                    | Il prend l'air ensommeillé (`sleepy`), puis s'endort, écran éteint                                              | Sur l'AtomS3R ([décision 12](adr/0012-energie.md))                                    |

### Jalon B : la vision

| Il perçoit    | Par                                                           | Il réagit                                                                                                  | Déjà prêt                                                           |
| ------------- | ------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------- |
| Un visage     | La caméra de la carte vision, détection ESP-DL (environ 20/s) | Les yeux suivent le visage ; un visage qui revient après 3 s d'absence déclenche la surprise (`face_seen`) | Le moteur et la commande `> face` ; le modèle reste à faire tourner |
| Le pouce levé | Le modèle de gestes d'Espressif (environ 5/s)                 | Joie et étincelles (`gesture_like`), puis il montre le niveau de sa batterie                               | La réaction et la commande `> react` ; le niveau reste à dessiner   |

Un seul geste en v1 : chercher la main puis le geste coûte environ 200 ms de calcul par image, et chez l'original la
reconnaissance est lente et rate souvent ([recherche.md](recherche.md)). Chaque geste ajouté multiplie les fausses
réactions. Le pouce baissé, ok, la main ouverte et « appelle-moi » ont déjà leurs réactions dans les données : ils
s'ajouteront après le banc d'essai, s'ils se trompent peu.

## Ce qu'il ne fait pas en v1

- **Les autres gestes** : après la v1, comme dit plus haut. Ceux de l'original hors du modèle d'Espressif (une
  trentaine, dont le doigt d'honneur) demanderaient un modèle entraîné.
- **Lire les expressions du visage**, faute de modèle : aucun modèle ESP-DL ne le fait. Les réactions `expression_*`
  existent déjà ; il manque un modèle, à trouver ou à entraîner, qui tournera sur le visage déjà détecté.
- **La voix et le son** : écartés dès le départ, l'objet réagit à ce que voit sa caméra. **Le froid** : il n'a pas de
  capteur de température.
- **Le vibreur** : écarté. L'original en a un ([recherche.md](recherche.md)), pas lilpet.
- **Parler à d'autres objets en Bluetooth** : écarté.

## Autonomie : une journée, endormi presque tout le temps

Trois états :

- **Éveillé** : écran allumé, il réagit (et au jalon B, la vision est active).
- **Endormi** (proposé : après 2 min sans toucher ni mouvement, et au jalon B sans visage) : il prend l'air
  ensommeillé, puis l'écran s'éteint, la carte écran passe en veille légère (proposé) et la carte vision en veille
  profonde. Le toucher, la
  prise en main ou le bouton le réveillent.
- **Éteint** (proposé) : un appui long sur le bouton coupe la batterie.

Le délai est fixe dans le firmware pour la v1 ; les réglages de l'app pourront le changer plus tard.

Les courants sont des **estimations**, à mesurer au banc d'essai, pour la batterie de 400 mAh
([materiel.md](materiel.md#batterie)) :

- **Endormi** : quelques mA. La carte vision prend 3 mA en veille profonde, d'après Seeed
  ([materiel.md](materiel.md#carte-vision--seeed-xiao-esp32s3-sense)) ; la carte écran en veille légère n'est pas
  mesurée.
- **Éveillé, jalon A** : l'écran et la carte écran seuls, sans doute moins de 100 mA (60 mA supposés pour l'écran de
  la 1,32", sans doute plus sur la 1,75", [materiel.md](materiel.md#batterie)).
- **Éveillé, jalon B** : environ 150 à 300 mA. [materiel.md](materiel.md#batterie) estime environ 150 mA avec une
  vision qui ne filme pas sans arrêt ; en filmant, la carte vision seule prend 155 mA d'après Seeed, et le pré-mortem
  arrive à 280 à 400 mA pour l'ensemble.

(Proposé.) Une journée, c'est 16 h loin du chargeur ; à 5 mA endormi, elle coûte environ 80 mAh. En gardant 20 % de
marge (80 mAh), il reste environ 240 mAh d'éveil : environ 3 à 4 h au jalon A, et environ 50 min à 1 h 30 au jalon B.
[materiel.md](materiel.md#batterie) estimait 2 à 3,5 h éveillé en continu : la journée repose donc sur la veille.
**Le critère du banc d'essai** (proposé) : moins de 5 mA endormi, et moins de 300 mA éveillé avec la vision.

## Ce qu'il faut de chaque partie

- **Carte écran** (jalon A) : la Waveshare 1,75" ([materiel.md](materiel.md)). Les pilotes du toucher (CST9217), de
  l'accéléromètre (QMI8658) et de l'alimentation (AXP2101), la veille et le réveil, la lecture du bouton et de la
  batterie, et un courant de charge sûr : **200 mA, réglés dans l'AXP2101 avant de brancher la batterie**, le maximum
  de la batterie choisie.
- **Accéléromètre** (jalon A) : le QMI8658 de la carte écran, son interruption de mouvement sur GPIO21, une broche RTC
  qui peut aussi réveiller la puce d'une veille profonde. Le pilote est à écrire ; celui du BMI270 reste celui de
  l'AtomS3R.
- **Carte vision** (jalon B, proposé) : détecter un visage au moins 10 fois par seconde éveillée, et chercher le pouce
  levé moins souvent (environ 200 ms par recherche, à régler au banc), envoyer `> face` et `> react` par l'UART, dormir
  quand la carte écran le demande et se réveiller quand elle le dit (une ligne GPIO, ou la coupure de son alimentation).
- **Boîtier** (après les deux jalons) : il fixera la taille et le poids. Le bouton et le port USB-C restent
  accessibles, la caméra va en façade à côté de l'écran, et la batterie garde un peu de marge.

## En attendant le matériel

La commande est prévue maintenant ([#47](https://github.com/Nexlein/lilpet/issues/47)). D'ici là, l'AtomS3R a déjà
un écran, un bouton et un accéléromètre. La mise en veille, le réveil au mouvement et au bouton, l'air ensommeillé et la
batterie faible y tournent, sur sa base de batterie ([décision 12](adr/0012-energie.md)). Ces règles passeront sur la
carte écran. Sa puce d'alimentation (AXP2101) y règle déjà la charge et lit la batterie ; ses autres pilotes restent à
écrire, à commencer par son accéléromètre (QMI8658), qui n'est pas celui de l'AtomS3R.

## L'app PC

Elle reste telle quelle pour la v1 ([app.md](app.md)) : galerie, aperçu, envoi du look, humeur et caresse. Les looks
perso, les réglages et l'éditeur viennent après. En v1, la luminosité et le délai de mise en veille sont donc fixes
dans le firmware.
