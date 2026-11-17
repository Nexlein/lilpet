# L'objet v1

Ce que doit faire le premier objet fini, décidé le 2026-11-17 ([#46](https://github.com/Nexlein/lilpet/issues/46)).
Les détails marqués **(proposé)** restent à confirmer, au plus tard au banc d'essai. Cette page sert de cible aux étapes
qui suivent ; leur ordre est dans le [README](../README.md#où-on-en-est).

## Les choix

- **Ce qu'il fait** : ce qui le rend vivant d'abord. Il se réveille quand on le prend en main, regarde la personne et
  la suit des yeux, réagit au toucher, aux secousses et au pouce levé, et montre quand sa batterie faiblit.
- **Deux jalons.** Le jalon A est l'objet complet sans caméra ; le jalon B ajoute la carte vision par-dessus. La caméra
  est la partie la plus chère en énergie, en épaisseur et en travail (une deuxième carte, une liaison UART, un modèle) :
  elle arrive sur un objet qui marche déjà.
- **Autonomie** : une journée, endormi presque tout le temps.
- **App PC** : telle quelle.
- **Objet** : la v1 ne cherche pas à être petite ; la taille viendra avec le boîtier.
- **Vie privée** : aucune image ne quitte la carte vision, et rien n'est enregistré. Seuls des mots en sortent, comme
  `> face 0.2 -0.1` ou `> react gesture_like` ([protocole.md](protocole.md)).

## Ce qu'il perçoit, et ce qu'il en fait

Les réactions sont celles de `eyes/emotions.json`, que l'app pourra changer un jour.

### Jalon A : l'objet sans caméra

| Il perçoit                            | Par                                                     | Il réagit                                                                                                       | Déjà prêt                                                  |
| ------------------------------------- | ------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------- |
| Une caresse                           | L'écran tactile de la carte écran (CST820)              | Joie et étincelles (`tap`)                                                                                      | La réaction ; le pilote du CST820 reste à écrire           |
| Une secousse, l'envers, l'inclinaison | Un accéléromètre I2C ajouté (la carte écran n'en a pas) | Vertige (`shake`) ; colère tête en bas (`upside_down`) ; le visage reste droit et les yeux glissent du côté bas | Le moteur, essayé sur l'AtomS3R avec son BMI270            |
| La prise en main                      | L'interruption de mouvement du même accéléromètre       | Il se réveille, quitte l'air endormi et reprend ses coups d'œil                                                 | Rien : à écrire                                            |
| Le bouton                             | `PWR_KEY` de la carte écran                             | (proposé) Un appui le réveille, un appui long l'éteint en coupant la batterie (`BAT_EN`)                        | Rien : à écrire                                            |
| Sa batterie                           | `BAT_ADC` de la carte écran                             | Batterie faible : il a l'air fatigué ; en charge (proposé) : une petite animation                               | Rien : à écrire ; la façon de voir la charge est à trouver |
| Personne                              | Ni toucher ni mouvement                                 | Il prend l'air endormi (`sleepy`), puis s'endort, écran éteint                                                  | L'émotion `sleepy` ; la veille reste à écrire              |

### Jalon B : la vision

| Il perçoit    | Par                                                           | Il réagit                                                                                                  | Déjà prêt                                                           |
| ------------- | ------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------- |
| Un visage     | La caméra de la carte vision, détection ESP-DL (environ 20/s) | Les yeux suivent le visage ; un visage qui revient après 3 s d'absence déclenche la surprise (`face_seen`) | Le moteur et la commande `> face` ; le modèle reste à faire tourner |
| Le pouce levé | Le modèle de gestes d'Espressif (environ 5/s)                 | Joie et étincelles (`gesture_like`), puis il montre le niveau de sa batterie                               | La réaction et la commande `> react` ; le niveau reste à dessiner   |

Un seul geste en v1 : la reconnaissance des gestes coûte environ 200 ms de calcul par image et se trompe, le défaut
relevé chez l'original ([recherche.md](recherche.md)). Un faux geste qui le rend triste sans raison serait pire que pas
de geste. Le pouce baissé, ok, la main ouverte et « appelle-moi » ont déjà leurs réactions dans les données : ils
s'ajouteront après le banc d'essai, s'ils se trompent peu.

## Ce qu'il ne fait pas en v1

- **Les autres gestes** : après la v1, comme dit plus haut. Ceux de l'original hors du modèle d'Espressif (une
  trentaine, dont le doigt d'honneur) demanderaient un modèle entraîné.
- **Lire les expressions du visage**, faute de modèle : aucun modèle ESP-DL ne le fait. Les réactions `expression_*`
  existent déjà ; il manque un modèle, à trouver ou à entraîner, qui tournera sur le visage déjà détecté.
- **La voix et le son** : écartés dès le départ, l'objet réagit à ce que voit sa caméra. **Le froid** : il n'a pas de
  capteur de température.
- **Le vibreur** : écarté, l'original en a un ([recherche.md](recherche.md)), mais lilpet s'en passe.
- **Parler à d'autres objets en Bluetooth** : écarté.

## Autonomie : une journée, endormi presque tout le temps

Trois états :

- **Éveillé** : écran allumé, il réagit (et au jalon B, la vision est active).
- **Endormi** (proposé : après 2 min sans toucher ni mouvement, et au jalon B sans visage) : il prend l'air endormi,
  puis l'écran s'éteint, la carte écran passe en veille légère et la carte vision en veille profonde. Le toucher, la
  prise en main ou le bouton le réveillent.
- **Éteint** (proposé) : un appui long sur le bouton coupe la batterie.

Le délai est fixe dans le firmware pour la v1 ; les réglages de l'app pourront le changer plus tard.

Les courants sont des **estimations**, à mesurer au banc d'essai, pour la batterie de 300 mAh :

- **Endormi** : quelques mA. La carte vision prend 3 mA en veille profonde, d'après Seeed
  ([materiel.md](materiel.md#carte-vision--seeed-xiao-esp32s3-sense)) ; la carte écran en veille légère n'est pas
  mesurée.
- **Éveillé, jalon A** : l'écran et la carte écran seuls, sans doute moins de 100 mA (60 mA supposés pour l'écran,
  [materiel.md](materiel.md#batterie)).
- **Éveillé, jalon B** : environ 150 à 300 mA. [materiel.md](materiel.md#batterie) estime environ 150 mA avec une
  vision qui ne filme pas sans arrêt ; en filmant, la carte vision seule prend 155 mA d'après Seeed, et le pré-mortem
  arrive à 280 à 400 mA pour l'ensemble.

À 5 mA endormi, une journée de 16 h loin du chargeur coûte environ 80 mAh. Il reste alors de quoi rester éveillé
plusieurs heures au jalon A, et environ 30 min à 1 h au jalon B, avec 20 % de marge. **Le critère du banc d'essai** :
moins de 5 mA endormi, et moins de 300 mA éveillé avec la vision.

## Ce qu'il faut de chaque partie

- **Carte écran** (jalon A) : les pilotes du toucher et de l'accéléromètre, la veille et le réveil, la lecture du
  bouton et de la batterie, et un courant de charge sûr. Son schéma règle la puce de charge (ETA6098) à 2 A avec
  R4 = 82 kΩ
  ([schéma](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.32/ESP32-S3-Touch-AMOLED-1.32-Schematic.pdf)),
  bien au-delà des 300 mA que supporte la batterie : **ne pas laisser la batterie en charge avant de l'avoir mesuré et
  corrigé**.
- **Accéléromètre** (jalon A) : un module I2C avec une broche d'interruption pour le réveil, branché sur une broche
  libre du connecteur d'extension (GPIO0 à 3, 5, 21 ou 45, [materiel.md](materiel.md)). Proposé : un BMI270, le
  capteur de l'AtomS3R, pour réutiliser son pilote déjà écrit et testé.
- **Carte vision** (jalon B, proposé) : détecter un visage au moins 10 fois par seconde éveillée, reconnaître le pouce
  levé, envoyer `> face` et `> react` par l'UART, dormir quand la carte écran le demande et se réveiller quand elle le
  dit (une ligne GPIO, ou la coupure de son alimentation).
- **Boîtier** (après les deux jalons) : il fixera la taille et le poids. Le bouton et le port USB-C restent
  accessibles, la caméra va en façade à côté de l'écran, et la batterie garde un peu de marge.

## En attendant le matériel

La commande est prévue d'ici un mois. D'ici là, l'AtomS3R a déjà un écran, un bouton et le même BMI270. On peut y écrire
la veille, le réveil au mouvement et au bouton, l'air endormi et la batterie faible (si sa base de batterie se lit).
Ce code passera ensuite sur la carte écran.

## L'app PC

Elle reste telle quelle pour la v1 ([app.md](app.md)) : galerie, aperçu, envoi du look, humeur et caresse. Les looks
perso, les réglages et l'éditeur viennent après. En v1, la luminosité et le délai de mise en veille sont donc fixes
dans le firmware.
