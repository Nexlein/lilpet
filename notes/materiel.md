# Matériel

Statut : choix arrêté le 2026-10-27, rien n'est encore commandé. Prix et stocks relevés à cette date.

## Architecture : deux cartes reliées en UART

Aucune carte trouvée ne combine un écran rond AMOLED et une caméra (recherche d'octobre 2026 chez Waveshare, Spotpear,
Seeed, LilyGO, M5Stack, Viewe et Tailor Pixels). On garde donc deux cartes :

1. **Carte écran** : rendu des yeux et animations, batterie, bouton marche/arrêt.
2. **Carte vision** : caméra, détection du visage et des gestes. Elle envoie des événements à la carte écran
   (`face_seen`, `thumbs_up`…) par UART.

Chaque puce garde toute sa puissance pour une seule tâche. Partager les deux sur une carte n'est pas possible : la carte
écran n'a pas assez de broches libres pour une caméra (il en faut une douzaine).

## Carte écran : Waveshare ESP32-S3-Touch-AMOLED-1.32

Comparaison des trois cartes rondes AMOLED de Waveshare, toutes en 466×466 :

| Carte                              | 1,32"                                    | 1,43" (favorite jusqu'ici)                       | 1,75"                                     |
| ---------------------------------- | ---------------------------------------- | ------------------------------------------------ | ----------------------------------------- |
| Surface affichée                   | Ø33,75 mm                                | Ø36,75 mm                                        | Ø43,76 mm                                 |
| Verre (taille extérieure)          | Ø36,4 mm                                 | Ø46 mm                                           | Ø48,96 mm                                 |
| Épaisseur                          | 10 mm                                    | environ 10,2 mm                                  | 10,4 mm                                   |
| Puce, flash, PSRAM                 | ESP32-S3-PICO-1-N8R8 : 8 Mo, 8 Mo        | ESP32-S3R8 : 16 Mo, 8 Mo                         | ESP32-S3R8 : 16 Mo, 8 Mo                  |
| Capteurs, audio                    | Micro, codec ES8311, haut-parleur fourni | Accéléromètre QMI8658, horloge PCF85063, microSD | Accéléromètre, horloge, microSD, 2 micros |
| Bouton marche/arrêt                | Oui                                      | Non (BOOT et RST seulement)                      | Oui                                       |
| Signal TE de l'écran relié à l'ESP | Oui (GPIO9)                              | Non                                              | Oui (GPIO13)                              |
| Prix sur waveshare.com             | 21,99 $                                  | 26,99 à 31,99 $                                  | 29,99 à 39,99 $                           |

Sources : 1,32" [page produit](https://www.waveshare.com/esp32-s3-touch-amoled-1.32.htm),
[plan coté](https://docs.waveshare.com/assets/images/ESP32-S3-Touch-AMOLED-1.32-ProductSize-1b4badf1cf52c297442bedbd100abfe5.webp),
[tableau des broches](https://docs.waveshare.com/assets/images/ESP32-S3-Touch-AMOLED-1.32-IntfIntro-c23d0503601aa0db6533daedd2c49bd4.webp) ;
1,43" [page produit](https://www.waveshare.com/esp32-s3-touch-amoled-1.43.htm),
[plan coté](https://docs.waveshare.com/assets/images/600px-ESP32-S3-Touch-AMOLED-1.43-details-size-cba612bed3eb78da9bb8398e40e496c9.webp),
[documentation](https://docs.waveshare.com/ESP32-S3-Touch-AMOLED-1.43) ; 1,75"
[page produit](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm),
[plan coté](https://docs.waveshare.com/assets/images/ESP32-S3-Touch-AMOLED-1.75-details-size-75dc998071807f62c5915afad69d7e80.webp),
[référence matérielle](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/main/HARDWARE_REFERENCE.md).

Pourquoi la 1,32" (recommandation) :

- **La taille.** Même résolution, et seulement 3 mm de surface affichée en moins que la 1,43", mais 10 mm de moins en
  diamètre extérieur : la 1,43" a une large bordure noire. Avec la caméra en façade, le galet reste vers 48-50 mm avec
  la 1,32", contre au moins 55-58 mm avec la 1,43".
- **Le signal TE** (début du rafraîchissement de l'écran) arrive sur une broche de l'ESP32 : on pourra envoyer les zones
  modifiées au bon moment, sans déchirure de l'image.
- **Le bouton marche/arrêt** sur batterie (`PWR_KEY` sur GPIO17, coupure batterie `BAT_EN` sur GPIO18).
- **Le micro et le haut-parleur ne sont pas nécessaires au départ.** Le haut-parleur est une pièce à part : on ne le
  branche pas. Le codec audio se coupe par logiciel (`Codec_EN` sur GPIO16). Le micro pourra servir plus tard à réagir
  aux bruits, comme l'original (voir [recherche.md](recherche.md)).

Ce qu'on perd :

- **L'accéléromètre**, utile pour réagir quand on secoue ou penche l'objet. Un petit module I2C pourra s'ajouter plus
  tard sur le connecteur d'extension.
- La microSD : inutile, les looks et les animations tiennent dans la flash.
- 8 Mo de flash au lieu de 16 : largement assez pour le moteur et les données.

À vérifier à la réception :

- **Le premier affichage** : le pilote de l'écran a été écrit d'après la démo Waveshare, sans la carte. Vérifier le
  sens de l'image, le décalage de colonnes, les couleurs et l'absence de déchirure (voir
  [architecture.md](architecture.md)).
- **La puce de charge et son courant de charge** : non publiés. Il faut les connaître avant de choisir définitivement la
  batterie (voir plus bas).
- **Le brochage du connecteur d'extension SH1.0 12 broches** (câble fourni) : non publié en texte. D'après le tableau
  des broches, GPIO43 et GPIO44 (l'UART) sont libres, ainsi que GPIO0 à 3, 5, 21 et 45.
- Disponibilité : vendue sur waveshare.com, aucun revendeur européen trouvé. Frais de port et TVA à voir au panier.

Les démos ESP-IDF de Waveshare pour cette carte utilisent le pilote `esp_lcd_sh8601` et une horloge QSPI de 40 MHz
([démo LVGL 9](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.32/main/Example/ESP-IDF/07_LVGL_V9_Test/components/lvgl_port_bsp/lvgl_port_bsp.cpp)),
l'hypothèse déjà prise dans [architecture.md](architecture.md).

## Carte vision : Seeed XIAO ESP32S3 Sense

- 21 × 17,8 × 15 mm avec sa carte caméra, ESP32-S3 avec 8 Mo de PSRAM et 8 Mo de flash, micro, chargeur de batterie
  intégré ([wiki Seeed](https://github.com/Seeed-Studio/wiki-documents/blob/docusaurus-version/sites/en/docs/Sensor/SeeedStudio_XIAO/SeeedStudio_XIAO_ESP32S3/XIAO_ESP32S3_Getting_Started.md)).
- **La caméra a changé** : l'OV2640 est arrêtée, la carte est livrée avec une OV3660 ou une OV5640 selon le stock
  ([fiche technique, octobre 2026](https://files.seeedstudio.com/Bazaar/product_pdf/113991115.pdf)). Les deux
  conviennent. Certains revendeurs affichent encore « OV2640 » dans le titre.
- Consommation mesurée par Seeed sous 3,8 V : 43 mA au repos, 5 mA en veille légère, 3 mA en veille profonde, 155 mA en
  moyenne en filmant ([wiki Seeed](https://github.com/Seeed-Studio/wiki-documents/blob/docusaurus-version/sites/en/docs/Sensor/SeeedStudio_XIAO/SeeedStudio_XIAO_ESP32S3/XIAO_ESP32S3_Getting_Started.md)).
- UART : D6 (TX, GPIO43) et D7 (RX, GPIO44), libres même caméra active
  ([brochage](https://www.espboards.dev/esp32/xiao-esp32s3-sense/)). À croiser avec GPIO44 et GPIO43 de la carte
  écran.
- La caméra est reliée à sa carte par une nappe : l'objectif peut être placé en façade, à côté de l'écran, comme sur
  l'original. Longueur de la nappe à mesurer à la réception.

Alternative écartée : M5Stack AtomS3R-CAM (24 × 24 × 13,5 mm, caméra 0,3 Mpx, pas de chargeur), caméra moins bonne
et objectif fixé sur la carte ([documentation](https://docs.m5stack.com/en/core/AtomS3R%20Cam)).

### Vision sur la puce

Modèles Espressif ESP-DL, temps mesurés par Espressif sur ESP32-S3 :

| Modèle                                                                                                     | Entrée  | Temps par image |
| ---------------------------------------------------------------------------------------------------------- | ------- | --------------- |
| [human_face_detect](https://components.espressif.com/components/espressif/human_face_detect) (MSR + MNP)   | 120×160 | 36,5 + 6,6 ms   |
| [hand_detect](https://components.espressif.com/components/espressif/hand_detect)                           | 224×224 | 128,6 ms        |
| [hand_gesture_recognition](https://components.espressif.com/components/espressif/hand_gesture_recognition) | 128×128 | 73,9 ms         |

Calcul (non publié) : environ 20 détections de visage par seconde au plus, et environ 5 gestes par seconde si on cherche
la main puis le geste à chaque image. Assez pour des réactions.

10 gestes reconnus : one, two, three, four, five, like, ok, no_gesture, call, dislike
([exemple ESP-DL](https://github.com/espressif/esp-dl/blob/master/examples/hand_gesture_recognition/README.md)). Le
doigt d'honneur n'en fait pas partie : il faudra entraîner un modèle perso si on le veut.

## Batterie

- **LiPo 1S 3,7 V avec circuit de protection, fiche 1,25 mm 2 broches (MX1.25)**, le connecteur de la carte écran. Les
  batteries Adafruit (JST-PH 2 mm) ne rentrent pas. **La polarité varie selon les vendeurs : la vérifier au multimètre
  avant de brancher.**
- Taille conseillée : **602030** (300 mAh, 30 × 20 × 6 mm), qui tient derrière l'écran
  ([Ampul](https://ampul.eu/en/battery/4156-li-pol-battery-300mah-37v-602030), 6,82 € TTC ;
  [Amazon.fr](https://www.amazon.fr/602030-Liter-Rechargeable-thermistance-Radiocommande/dp/B0CY8NS7ZM)). Plus petite
  si le boîtier manque de place : 502030 (250 mAh, 5 mm d'épaisseur).
- Charger à 1C au plus, donc 300 mA pour 300 mAh ([guide Adafruit](https://cdn-learn.adafruit.com/downloads/pdf/li-ion-and-lipoly-batteries.pdf)).
  Le courant de charge de la carte n'est pas publié : à vérifier avant d'y laisser la batterie en charge.
- Une seule batterie, un seul chargeur (celui de la carte écran) : ne pas relier aussi la batterie au chargeur de la
  carte vision. L'alimentation de la carte vision (3,3 V de la carte écran ou directement la batterie) sera choisie au
  montage, selon ce que le régulateur de la carte écran peut fournir (la caméra monte à 366 mA en pointe).
- Ne pas percer ni écraser la batterie ; lui laisser un peu de marge dans le boîtier.

**Autonomie estimée : courte.** Estimation grossière, avec 60 mA supposés pour l'écran (non mesuré, yeux sur fond
noir) :

- environ 2 h avec 300 mAh si la caméra analyse souvent ;
- environ 3,5 h si la carte vision dort la plupart du temps.

Pour comparaison, Waveshare annonce environ 1 h à pleine luminosité et 3 à 4 h écran éteint avec 400 mAh sur sa carte
1,8" ([wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8)). Ce sera un point à travailler dans le
firmware : veille de la caméra, luminosité, mise en veille quand personne ne regarde. À mesurer dès la réception.

## Encombrement

Écran 10 mm + carte vision jusqu'à 15 mm + batterie 6 mm, plus les parois : environ 30 mm d'épaisseur si tout est
empilé, moins si la caméra est déportée en façade par sa nappe et la carte vision couchée à côté de la batterie. Le
boîtier se dessinera une fois les pièces reçues et mesurées.

## Déjà en ma possession

- **M5Stamp S3A** (M5Stack) : ESP32-S3FN8, 8 Mo de flash, pas de PSRAM, 26 × 18 × 4,7 mm, USB-C
  ([documentation](https://docs.m5stack.com/en/core/Stamp-S3A)). Même puce que la carte écran, mais sans PSRAM ni écran,
  donc elle ne remplace rien dans la commande. Sert à mesurer le moteur sur une vraie puce avant de recevoir le
  matériel, puis pourra simuler la carte vision sur la liaison série.

## Liste à commander

Prix relevés les 27 et 28 octobre 2026, TTC, livraison en France. Le moins cher : **AliExpress pour les deux cartes et
les câbles** (boutique de Seeed et revendeur agréé Waveshare, TVA incluse), **Amazon.fr pour la batterie** (expédiée par
Amazon, livrée en deux jours ; sur AliExpress elle revient plus cher avec le port et arrive deux semaines plus tard).

| Article                                                                         | Où                                                                                                            | Prix                 | Livraison    |
| ------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------- | -------------------- | ------------ |
| Waveshare ESP32-S3-Touch-AMOLED-1.32                                            | [AliExpress, FireChip Hardware Store](https://fr.aliexpress.com/item/1005012964258234.html) (revendeur agréé) | 22,19 € port compris | 4 au 11 nov. |
| Seeed XIAO ESP32S3 Sense (caméra OV3660)                                        | [AliExpress, Seeedstudio AI Hardware Store](https://fr.aliexpress.com/item/1005006988111963.html)             | 16,83 € port compris | 4 au 11 nov. |
| LiPo 602030 300 mAh, fiche 1,25 mm 2 broches, protection contre la surcharge    | [Amazon.fr](https://www.amazon.fr/dp/B0H32T667J) (vendu par mishao, expédié par Amazon)                       | 8,90 €               | 31 oct.      |
| 10 rallonges à fiche 1,25 mm (variante 2P), pour inverser la polarité si besoin | [AliExpress](https://fr.aliexpress.com/item/1005008976200506.html)                                            | environ 2,16 €       | non relevée  |

Total : environ 50,08 € (47,92 € sans les câbles). À vérifier au paiement :

- Depuis le 1er juillet 2026, un droit de douane forfaitaire de 3 € s'applique aux colis venant de hors UE sous 150 €
  ([Conseil de l'UE](https://www.consilium.europa.eu/en/press/press-releases/2025/12/12/customs-council-agrees-to-levy-customs-duty-on-small-parcels-as-of-1-july-2026/)).
  AliExpress l'affiche au paiement (« des frais d'importation s'appliquent aux envois hors UE ») : jusqu'à 9 € de
  plus pour trois vendeurs, chacun envoyant son colis. Le vrai total se lit dans le panier, avant de payer.
- Amazon offre la livraison sur la première commande ; sinon des frais peuvent s'ajouter (non vérifié).
- **La polarité de la batterie** n'est pas standard sur ces fiches : la vérifier au multimètre avant de la brancher.

Autres options comparées le même jour :

| Option                                | Total estimé                  | Remarques                                                                                                                                                  |
| ------------------------------------- | ----------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Tout sur AliExpress                   | environ 54,15 €               | Batterie 9,19 € + 3,78 € de port, livrée du 11 au 20 nov. ([annonce](https://fr.aliexpress.com/item/1005012369515535.html))                                |
| Tout sur Amazon.fr                    | 74,79 € (82,78 € avec câbles) | Écran 32,99 € ([vendu par Waveshare](https://www.amazon.fr/dp/B0G141GV48), 5 au 14 nov.), caméra 32,90 €                                                   |
| waveshare.com + Reichelt              | 55 à 65 €                     | Port, TVA et douane de Waveshare connus seulement au panier ; 1 à 3 semaines ([expédition](https://www.waveshare.com/shipping))                            |
| Eckstein (Allemagne), les deux cartes | 46,94 € + port                | Stock incertain pour les deux, caméra annoncée en OV2640 ([écran](https://eckstein-shop.de/esp32-s3-touch-amoled-132-development-board_1))                 |
| Opencircuit (Pays-Bas) + Gotronic     | 57 à 58 €                     | Pas de douane ; écran disponible le 11 nov., caméra de retour vers le 30 oct. ([Gotronic](https://www.gotronic.fr/art-carte-xiao-esp32s3-sense-37063.htm)) |
| DigiKey, Mouser ou Farnell (caméra)   | 14,50 à 15 € + port           | Port de 12 à 25 € sous 75 € de commande : intéressant seulement avec d'autres achats                                                                       |

Plus tard, avec le boîtier :

- Vibreur pastille 10 mm 3 V ([Gotronic](https://www.gotronic.fr/cat-vibreurs-1932.htm), environ 1,30 €) avec un
  transistor NPN (PN2222) ou un MOSFET, une diode de roue libre et une résistance : le moteur tire 60 à 120 mA, trop pour
  une broche de l'ESP32 ([Adafruit](https://www.adafruit.com/product/1201)).
- Petit module accéléromètre I2C, si on veut réagir aux secousses.
- Anneau de porte-clé et chaînette.
- Boîtier imprimé en 3D.

Outils, si tu ne les as pas : fer à souder à panne fine, multimètre (polarité de la batterie, courant de charge), câble
USB-C qui transmet les données.

## Alternative si on veut un écran plus grand

La 1,43" reste possible avec un galet d'au moins 55-58 mm. Elle apporte l'accéléromètre, l'horloge et la microSD, mais
pas de bouton marche/arrêt ni de signal TE relié. Le moteur ne change pas (même résolution).
