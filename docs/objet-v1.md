# L'objet v1

Ce que doit faire le premier objet fini. Les quatre choix ont été faits le 2026-11-17
([#46](https://github.com/Nexlein/lilpet/issues/46)). Les détails marqués **(proposé)** restent à confirmer, au plus
tard au banc d'essai ([#53](https://github.com/Nexlein/lilpet/issues/53)). Cette page sert de cible au matériel
([#47](https://github.com/Nexlein/lilpet/issues/47)) et à l'énergie
([#51](https://github.com/Nexlein/lilpet/issues/51)). Elle sert aussi au banc d'essai, à la carte écran, à la carte
vision et au boîtier ([#53](https://github.com/Nexlein/lilpet/issues/53) à
[#56](https://github.com/Nexlein/lilpet/issues/56)).

## Les choix du 2026-11-17

- **Ce qu'il perçoit** : le plus complet que permettent les pièces prévues, sans fermer la porte à la suite. Il suit les
  visages, reconnaît des gestes, sent le toucher et le bouton, et reçoit un accéléromètre ajouté.
- **Autonomie** : une journée, endormi presque tout le temps.
- **App PC** : telle quelle.
- **Objet** : la v1 ne cherche pas à être petite ; la taille viendra avec le boîtier.

## Ce qu'il perçoit, et ce qu'il en fait

Les réactions sont celles de `eyes/emotions.json`, que l'app pourra changer un jour. La colonne « Déjà prêt » dit ce
qui marche aujourd'hui, sans les vraies pièces.

| Il perçoit                            | Par                                                           | Il réagit                                                                                                                  | Déjà prêt                                                                                                        |
| ------------------------------------- | ------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------- |
| Un visage                             | La caméra de la carte vision, détection ESP-DL (environ 20/s) | Les yeux suivent le visage ; un visage qui revient après 3 s d'absence déclenche la surprise (`face_seen`)                 | Le moteur et la commande `> face` ; le modèle reste à faire tourner                                              |
| Cinq gestes de la main (proposé)      | Le même modèle ESP-DL, main puis geste (environ 5/s)          | Pouce levé : joie et étincelles ; pouce baissé : tristesse ; ok : joie ; main ouverte : surprise ; « appelle-moi » : amour | Les réactions `gesture_*` et la commande `> react`                                                               |
| Une caresse                           | L'écran tactile de la carte écran (CST820)                    | Joie et étincelles (`tap`)                                                                                                 | La réaction ; le pilote du CST820 reste à écrire                                                                 |
| Une secousse, l'envers, l'inclinaison | Un accéléromètre I2C ajouté (la carte écran n'en a pas)       | Vertige (`shake`) ; colère tête en bas (`upside_down`) ; le visage reste droit et les yeux glissent du côté bas            | Le moteur, essayé sur l'AtomS3R ; le module reste à choisir ([#47](https://github.com/Nexlein/lilpet/issues/47)) |
| La prise en main                      | L'interruption de mouvement du même accéléromètre             | Il se réveille (voir l'autonomie)                                                                                          | Rien : à écrire ([#51](https://github.com/Nexlein/lilpet/issues/51))                                             |
| Le bouton                             | `PWR_KEY` de la carte écran                                   | (proposé) Un appui le réveille, un appui long l'éteint en coupant la batterie (`BAT_EN`)                                   | Rien : à écrire ([#51](https://github.com/Nexlein/lilpet/issues/51))                                             |
| Personne                              | Ni visage ni toucher                                          | (proposé) Il somnole, puis s'endort                                                                                        | L'émotion `sleepy` ; l'état somnolent et la veille sont à écrire                                                 |

Cinq gestes plutôt que deux ou trois (proposé) : leurs réactions existent déjà, et le modèle d'Espressif les reconnaît
au même coût parmi ses dix ([materiel.md](materiel.md#vision-sur-la-puce)). Les autres, comme les chiffres, ne
déclenchent rien en v1. Au réveil (proposé), il quitte l'air endormi et reprend ses coups d'œil.

## Ce qu'il ne fait pas en v1

- **Lire les expressions du visage**, faute de modèle : aucun modèle ESP-DL ne le fait. Les réactions
  `expression_happy`, `expression_sad`, `expression_angry` et `expression_surprised` existent déjà ; il manque un
  modèle, à trouver ou à entraîner, qui tournera sur le visage déjà détecté.
- **Les autres gestes de l'original**, une trentaine dont le doigt d'honneur ([recherche.md](recherche.md)) : hors du
  modèle d'Espressif.
- **La voix et le son** : écartés dès le départ, l'objet réagit à ce que voit sa caméra. **Le froid** : il n'a pas de
  capteur de température.
- **Le vibreur** : pas retenu pour la v1 ; il est prévu avec le boîtier ([materiel.md](materiel.md#liste-à-commander)).
- **Montrer sa batterie au pouce levé**, comme l'original (proposé : plus tard) : il faut d'abord mesurer la batterie
  ([#51](https://github.com/Nexlein/lilpet/issues/51)).
- **Parler à d'autres objets en Bluetooth** : écarté.

## Autonomie : une journée, endormi presque tout le temps

Une journée, c'est ici (proposé) **16 h loin du chargeur**, de la recharge du matin à celle du soir. Quatre états :

- **Éveillé** : écran allumé, vision active, il réagit.
- **Somnolent** (proposé) : après 30 s sans visage ni toucher, il prend l'air endormi et la vision regarde moins
  souvent. Cet état est nouveau, à écrire.
- **Endormi** (proposé : après 2 min) : l'écran s'éteint, la carte vision passe en veille profonde et la carte écran
  en veille légère. Le toucher, la prise en main ou le bouton le réveillent.
- **Éteint** (proposé) : un appui long sur le bouton coupe la batterie.

Les délais sont fixes dans le firmware pour la v1 ; les réglages de l'app pourront les changer plus tard
([#58](https://github.com/Nexlein/lilpet/issues/58)).

### Le budget, en estimations

Les courants sont des **estimations**, à mesurer au banc d'essai ([#53](https://github.com/Nexlein/lilpet/issues/53)),
pour une batterie de 300 mAh :

- **Endormi : environ 5 mA**, estimé. La carte vision prend 3 mA en veille profonde, d'après Seeed
  ([materiel.md](materiel.md#carte-vision--seeed-xiao-esp32s3-sense)). Les 2 mA de la carte écran en veille légère
  sont une supposition, faute de mesure.
- **Éveillé : environ 150 à 300 mA**, estimé. [materiel.md](materiel.md#batterie) donne environ 2 h sur 300 mAh avec
  la caméra qui analyse souvent, soit environ 150 mA. Ce chiffre suppose 60 mA pour l'écran et une vision qui ne
  filme pas sans arrêt. En filmant, la carte vision seule prend 155 mA en moyenne, d'après Seeed. Avec l'écran et le
  processeur de la carte écran, l'analyse du pré-mortem arrive à 280 à 400 mA. Le calcul prend 300 mA, le cas
  prudent.

16 h endormi coûtent environ 80 mAh. En gardant 20 % de marge (60 mAh), il reste environ 160 mAh pour jouer : environ
30 min d'éveil par jour à 300 mA, ou 1 h à 150 mA. **Le critère du banc d'essai** : avec moins de 5 mA endormi et
moins de 300 mA éveillé, l'objectif tient.

Il faut pour cela :

- la mise en veille et le réveil dans le firmware ([#51](https://github.com/Nexlein/lilpet/issues/51)) ;
- que la carte écran endorme la carte vision et la réveille (proposé : une ligne GPIO vers la XIAO, ou la coupure de
  son alimentation ; [#47](https://github.com/Nexlein/lilpet/issues/47),
  [#55](https://github.com/Nexlein/lilpet/issues/55)) ;
- une broche d'interruption pour l'accéléromètre et une autre pour le toucher
  ([#47](https://github.com/Nexlein/lilpet/issues/47)) ;
- un courant de charge sûr. Le schéma de Waveshare règle la puce de charge (ETA6098) à 2 A avec R4 = 82 kΩ
  ([schéma](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.32/ESP32-S3-Touch-AMOLED-1.32-Schematic.pdf)),
  bien au-delà des 300 mA que supporte la batterie : à mesurer à la réception et à corriger
  ([#47](https://github.com/Nexlein/lilpet/issues/47)).

## Ce qu'il faut de la carte vision

(Proposé, pour [#55](https://github.com/Nexlein/lilpet/issues/55).)

- Détecter un visage au moins 10 fois par seconde quand il est éveillé, pour que les yeux suivent sans à-coups.
  Espressif annonce environ 20 par seconde.
- Reconnaître les gestes à environ 5 par seconde, comme l'estime [materiel.md](materiel.md#vision-sur-la-puce).
- Envoyer `> face` et `> react` par l'UART, au protocole de [protocole.md](protocole.md).
- Dormir quand la carte écran le demande, et se réveiller quand elle le dit.

## L'app PC

Elle reste telle quelle pour la v1 ([app.md](app.md)) : galerie, aperçu, envoi du look, humeur et caresse. Les looks
perso, les réglages et l'éditeur viennent après ([#57](https://github.com/Nexlein/lilpet/issues/57) à
[#59](https://github.com/Nexlein/lilpet/issues/59)). En v1, la luminosité et les délais de mise en veille sont donc
fixes dans le firmware.

## L'objet physique

La v1 ne cherche pas à être petite : ni taille ni poids imposés. Les deux cartes et la batterie sont empilées, environ
30 mm d'épaisseur ([materiel.md](materiel.md#encombrement)). L'accéléromètre est relié au bus I2C de la carte écran. Le
but est de faire marcher l'objet complet.

- La charge passe par l'USB-C de la carte écran, une fois son courant ramené à une valeur sûre.
- Une seule batterie et un seul chargeur, ceux de la carte écran ([materiel.md](materiel.md#batterie)).

Pour le boîtier ([#56](https://github.com/Nexlein/lilpet/issues/56)), qui fixera la taille et le poids :

- le bouton et le port USB-C doivent rester accessibles ;
- la caméra doit être en façade, à côté de l'écran ;
- la batterie doit avoir un peu de marge, sans pression.
