# Matériel

Statut : choix arrêté le 2026-10-27, carte écran changée le 2026-11-17 pour la 1,75"
([#47](https://github.com/Nexlein/lilpet/issues/47)). Rien n'est encore commandé. Prix et stocks relevés le
2026-11-17.

## Deux cartes reliées en UART

Aucune carte ne combine un écran rond AMOLED et une caméra : une carte écran et une carte vision, reliées en UART
([décision 7](adr/0007-deux-cartes-uart.md)).

## Carte écran : Waveshare ESP32-S3-Touch-AMOLED-1.75

Comparaison des trois cartes rondes AMOLED de Waveshare, toutes en 466×466 :

| Carte                              | 1,32"                                    | 1,43"                                            | 1,75"                                     |
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

Pourquoi la 1,75" (choix du 2026-11-17) :

- **La v1 ne cherche pas à être petite** ([objet-v1.md](objet-v1.md)). Le 27 octobre, la 1,32" avait été choisie pour
  sa taille : même résolution, et un galet vers 48-50 mm avec la caméra en façade. Il lui manque l'accéléromètre, et sa
  charge est réglée trop fort (plus bas).
- **L'accéléromètre est sur la carte** : un QMI8658 (I2C 0x6B), avec INT2 sur GPIO21, une broche RTC, qui peut aussi
  réveiller la puce d'une veille profonde. INT1 passe par l'extenseur d'entrées-sorties TCA9554
  ([référence matérielle](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/main/HARDWARE_REFERENCE.md)).
  Pour la 1,32", il fallait un module : le seul BMI270 en vente (SparkFun) était à 12 semaines chez DigiKey
  ([DigiKey](https://www.digikey.at/en/products/detail/sparkfun-electronics/22398/21703921)), sur commande chez
  Robotshop.
- **La charge se règle par logiciel.** La puce d'alimentation AXP2101 charge de 100 à 1000 mA, par paliers
  ([pilote AXP2101 de Zephyr](https://pigweed.googlesource.com/third_party/github/zephyrproject-rtos/zephyr/+/7e74b833e8eeb4b0517d1285acaed833457c8aed/drivers/charger/charger_axp2101.c)),
  et mesure la batterie. La 1,32" charge à 2 A, fixés par une résistance 0402 à remplacer
  ([schéma](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.32/ESP32-S3-Touch-AMOLED-1.32-Schematic.pdf)).
- **Le bouton marche/arrêt** passe par l'AXP2101 ; son état se lit sur le TCA9554 (P4).
- **Le signal TE** arrive sur GPIO13 : on pourra envoyer les zones modifiées au bon moment, sans déchirure de l'image.
- **Le même contrôleur d'écran** que la 1,32", un CO5300 en QSPI, en 466 × 466 : le pilote déjà écrit
  (`firmware/main/display_co5300.c`) sert, avec d'autres broches.
- **16 Mo de flash** et 8 Mo de PSRAM.
- **Le micro et le haut-parleur ne sont pas nécessaires au départ** : deux micros (ES7210) et un codec (ES8311), sans
  haut-parleur branché. Les micros pourront servir plus tard à réagir aux bruits, comme l'original (voir
  [recherche.md](recherche.md)).

Ce qu'on perd : **la taille**. Le verre fait Ø48,96 mm au lieu de Ø36,4 mm, et le galet fera plutôt 55-60 mm avec la
caméra.

Les broches, d'après la
[référence matérielle](https://raw.githubusercontent.com/waveshareteam/ESP32-S3-Touch-AMOLED-1.75/main/HARDWARE_REFERENCE.md) :

| Usage                          | Broches                                                                       |
| ------------------------------ | ----------------------------------------------------------------------------- |
| Écran CO5300 en QSPI           | données GPIO4 à 7, horloge GPIO38, CS GPIO12, reset GPIO39, TE GPIO13         |
| Toucher CST9217 (I2C 0x5A)     | interruption GPIO11, reset GPIO40                                             |
| Bus I2C partagé                | SCL GPIO14, SDA GPIO15                                                        |
| Accéléromètre QMI8658 (0x6B)   | INT2 GPIO21 ; INT1 sur le TCA9554                                             |
| Extenseur TCA9554 (0x20)       | P3 horloge, P4 bouton marche/arrêt, P5 AXP2101, P6 QMI8658 INT1               |
| Autres sur le bus              | AXP2101 0x34, horloge PCF85063 0x51, codec ES8311 0x18, ADC audio ES7210 0x40 |
| Son, microSD                   | I2S GPIO8, 9, 10, 42, 45, ampli GPIO46 ; microSD GPIO1, 2, 3 (et 41)          |
| Connecteur 8 broches (2,54 mm) | VBUS, GND, 3V3, GPIO44 (RX de l'UART0), GPIO43 (TX), GPIO17, GPIO18, GPIO16   |

À la réception, dans cet ordre :

1. **Sans la batterie, flasher le firmware** (`BOARD=waveshare_175 mise run firmware:flash`). La démo d'usine de
   Waveshare charge à 400 mA, le double du maximum de la batterie choisie, et le réglage de départ de l'AXP2101 n'est
   pas publié : le firmware règle la charge à 200 mA à chaque démarrage, et son journal le dit (« AXP2101 charges at
   200 mA at most »). Sans cette ligne, la batterie reste débranchée.
2. **Le premier affichage** : le pilote de l'écran a été écrit d'après les démos, sans carte. Vérifier le sens de
   l'image, le décalage de colonnes, les couleurs et l'absence de déchirure (voir [firmware.md](firmware.md)). Une
   première image noire ou pleine de bruit peut venir d'une attente trop courte après la sortie de veille de l'écran :
   le pilote de la démo attend 600 ms, le nôtre 100.
3. **La polarité de la batterie**, au multimètre, avant de la brancher.
4. **Brancher la batterie, l'USB toujours branché**, puis mesurer le courant de charge au multimètre, en série sur son
   fil. L'AXP2101 oublie ce réglage quand il n'a plus ni USB ni batterie : entre le démarrage et ce réglage, environ une
   seconde, celui de départ s'applique. Le mesurer aussi.

Et aussi :

- **Ce que l'AXP2101 doit allumer** au démarrage pour l'écran et le toucher : la référence matérielle ne le dit pas,
  et la démo ESP-IDF de Waveshare n'en allume aucun (ses réglages des rails sont en commentaire) : ceux de départ
  suffisent sans doute.
- **Ce que le 3V3 de la carte écran peut fournir** à la carte vision, dont la caméra monte à 366 mA en pointe : la
  référence matérielle ne le dit pas.

## Carte vision : Seeed XIAO ESP32S3 Sense

- 21 × 17,8 × 15 mm avec sa carte caméra, ESP32-S3R8 (boîtier QFN56) avec 8 Mo de PSRAM et 8 Mo de flash, micro,
  chargeur de batterie intégré ([wiki Seeed](https://github.com/Seeed-Studio/wiki-documents/blob/docusaurus-version/sites/en/docs/Sensor/SeeedStudio_XIAO/SeeedStudio_XIAO_ESP32S3/XIAO_ESP32S3_Getting_Started.md)).
- **La caméra a changé** : l'OV2640 est arrêtée, la carte est livrée avec une OV3660 ou une OV5640 selon le stock
  ([fiche technique, octobre 2026](https://files.seeedstudio.com/Bazaar/product_pdf/113991115.pdf)). Les deux
  conviennent. Certains revendeurs affichent encore « OV2640 » dans le titre.
- Consommation mesurée par Seeed sous 3,8 V : 43 mA au repos, 5 mA en veille légère, 3 mA en veille profonde, 155 mA en
  moyenne en filmant ([wiki Seeed](https://github.com/Seeed-Studio/wiki-documents/blob/docusaurus-version/sites/en/docs/Sensor/SeeedStudio_XIAO/SeeedStudio_XIAO_ESP32S3/XIAO_ESP32S3_Getting_Started.md)).
- UART : D6 (TX, GPIO43) et D7 (RX, GPIO44), libres même caméra active
  ([brochage](https://www.espboards.dev/esp32/xiao-esp32s3-sense/)). À croiser avec GPIO44 et GPIO43 de la carte
  écran.
- Sa puce a le boîtier de la 1,75" et du M5Stamp S3A, dont les images pilotent les broches de ses sorties caméra : une
  telle image flashée dessus par erreur s'arrête au démarrage, sans toucher une broche, par la taille de la flash ou de
  la PSRAM ([décision 6](adr/0006-une-image-par-carte.md)).
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

Aucun de ces modèles ne lit les expressions du visage, que le projet vise aussi : il faudra en trouver ou en entraîner
un, à faire tourner sur le visage déjà détecté.

## Batterie

- **LiPo 1S 3,7 V avec circuit de protection, fiche 1,25 mm 2 broches (MX1.25)**, le connecteur de la carte écran. Les
  batteries Adafruit (JST-PH 2 mm) ne rentrent pas. **La polarité varie selon les vendeurs : la vérifier au multimètre
  avant de brancher.**
- Choisie le 2026-11-17 : **802525, 400 mAh** (25 × 25 × 8,6 mm, environ 10 g), avec protection et fiche 1,25 mm
  2 broches ([Amazon.fr](https://www.amazon.fr/dp/B0GJSQXXR2)). Son vendeur limite la charge à 200 mA. La 602030 de
  300 mAh prévue d'abord n'est plus en stock, et la 1,75" laisse plus de place derrière l'écran.
- Charger à 1C au plus ([guide Adafruit](https://cdn-learn.adafruit.com/downloads/pdf/li-ion-and-lipoly-batteries.pdf)),
  et pas plus que ce que le vendeur donne : **200 mA**, à régler dans l'AXP2101 avant le premier branchement (voir
  « À la réception, dans cet ordre », [plus haut](#carte-écran--waveshare-esp32-s3-touch-amoled-175)).
- Une seule batterie, un seul chargeur (celui de la carte écran) : ne pas relier aussi la batterie au chargeur de la
  carte vision. Elle prend son alimentation sur le 3V3 de la carte écran (plus bas, « Le câblage des deux cartes »), et
  pas sur son propre port USB tant qu'elle y est reliée.
- Ne pas percer ni écraser la batterie ; lui laisser un peu de marge dans le boîtier.

**Autonomie estimée : courte.** Estimation grossière, avec 60 mA supposés pour l'écran (non mesuré, yeux sur fond
noir). Ce chiffre date de la 1,32" : la 1,75" affiche environ 1,7 fois plus de surface, et ses yeux allumés prendront
sans doute plus. Les durées ci-dessous sont donc plutôt optimistes :

- environ 2 h avec 300 mAh si la caméra analyse souvent, donc environ 2 h 40 avec 400 mAh ;
- environ 3,5 h si la carte vision dort la plupart du temps, donc environ 4 h 40 avec 400 mAh.

La journée repose donc sur la veille ([objet-v1.md](objet-v1.md)).

Pour comparaison, Waveshare annonce environ 1 h à pleine luminosité et 3 à 4 h écran éteint avec 400 mAh sur sa carte
1,8" ([wiki](https://www.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.8)). Ce sera un point à travailler dans le
firmware : veille de la caméra, luminosité, mise en veille quand personne ne regarde. À mesurer dès la réception.

## Encombrement

Écran 10,4 mm + carte vision jusqu'à 15 mm + batterie 8,6 mm, plus les parois : environ 35 mm d'épaisseur si tout est
empilé, moins si la caméra est déportée en façade par sa nappe et la carte vision couchée à côté de la batterie. Le
boîtier se dessinera une fois les pièces reçues et mesurées.

## Déjà en ma possession

- **M5Stamp S3A** (M5Stack) : ESP32-S3FN8, 8 Mo de flash, pas de PSRAM, 26 × 18 × 4,7 mm, USB-C
  ([documentation](https://docs.m5stack.com/en/core/Stamp-S3A)). Même puce que la carte écran, mais sans PSRAM ni écran,
  donc elle ne remplace rien dans la commande. Sert à mesurer le moteur sur une vraie puce avant de recevoir le
  matériel, puis pourra simuler la carte vision sur la liaison série.
- **M5 AtomS3R** (M5Stack) et sa batterie : ESP32-S3 (8 Mo de flash, 8 Mo de PSRAM), écran couleur 0,85" de
  128 × 128, capteur de mouvement BMI270 + boussole BMM150, USB natif
  ([documentation](https://docs.m5stack.com/en/core/AtomS3R)). Le meilleur banc d'essai avant la commande : même puce
  que l'objet, avec un écran, pour voir un look envoyé par l'app et essayer l'inclinaison. L'image `atoms3r` y
  affiche les yeux en 128 px (`firmware/main/display_atoms3r.c`). M5Stack a changé la puce de l'écran en 2026 (GC9107,
  puis ST7735S) : le pilote lit l'identifiant et le donne au démarrage (le mien : GC9107). Rétroéclairage par une puce sur le bus I2C
  (adresse 0x30, broches 45 et 0), d'après la bibliothèque de M5Stack
  ([M5GFX](https://github.com/m5stack/M5GFX/blob/master/src/M5GFX.cpp)). Capteur de mouvement BMI270 sur le même bus
  (adresse 0x68) : il ne donne aucune mesure tant qu'on ne lui a pas chargé le fichier de configuration de Bosch
  (8 Ko, licence BSD-3, [BMI270_SensorAPI](https://github.com/boschsensortec/BMI270_SensorAPI)), données inchangées dans
  `firmware/third_party/bmi270/bmi270_config.c`. Axes mesurés sur la carte : son x pointe vers le port USB, son y vers
  la droite de l'écran.
- **M5StickC Plus2** : ESP32 classique (pas S3), écran 135 × 240
  ([documentation](https://docs.m5stack.com/en/core/M5StickC%20PLUS2)). Peu utile : pas la puce de l'objet.
- **Flipper Zero** : STM32 à 64 MHz, écran monochrome 128 × 64
  ([caractéristiques](https://docs.flipper.net/zero/development/hardware/tech-specs)). Pas adapté à des yeux en
  couleur.

## Liste à commander

Plan retenu le 2026-10-31, revu le 2026-11-17 pour la 1,75", prix TTC, livraison en France. Le principe : **éviter
les colis venant de hors UE**. Depuis le 1er juillet 2026, l'UE prend 3 € de droit de douane par catégorie d'article, et
2 € de frais de traitement s'y ajoutent à partir du 1er novembre 2026
([douane.gouv.fr](https://www.douane.gouv.fr/petits-colis-ce-qui-change-pour-vos-achats-en-ligne)). Sur AliExpress, cela
faisait environ 6 € par article : 30,28 € de frais d'import pour un panier de cinq articles à 43,87 €.

| Où       | Article                                                                                                                                                         | Prix       |
| -------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------- |
| Amazon   | [Waveshare ESP32-S3-Touch-AMOLED-1.75, version standard (sans boîtier ni GPS)](https://www.amazon.fr/dp/B0F7XTJ1ZL) (vendue par Waveshare, expédiée par Amazon) | 43,99 €    |
| Amazon   | [LiPo 802525 400 mAh, fiche 1,25 mm 2 broches, protection](https://www.amazon.fr/dp/B0GJSQXXR2) (expédiée par Amazon)                                           | 13,71 €    |
| Gotronic | [Seeed XIAO ESP32S3 Sense, caméra OV3660](https://www.gotronic.fr/art-carte-xiao-esp32s3-sense-37063.htm), en stock                                             | 16,90 €    |
| Gotronic | [Cordon PicoBlade 2 contacts 1,25 mm vers fils nus](https://www.gotronic.fr/art-cordon-picoblade-2-cts-1-25-mm-mx3922-35629.htm), en stock                      | 2 × 1,50 € |
| Gotronic | Livraison en point relais (relevée le 31 octobre)                                                                                                               | 4,50 €     |

Total : **82,10 €** (Amazon 57,70 €, livré gratuitement dès 45 € expédiés par Amazon, le 21 novembre au relevé ;
Gotronic 24,40 €). Revérifier les prix et les stocks au moment de commander. Pas de carte de rechange, ni de testeur
USB-C : le multimètre, en série sur le fil de la batterie par un cordon PicoBlade, mesure l'éveil, la charge et le
sommeil.

- Les cordons PicoBlade se branchent sur la fiche batterie de la carte écran (MX1.25 = Molex PicoBlade). Ils servent à
  corriger la polarité de la batterie si elle est inversée, et à mettre le multimètre en série sur son fil.
- **Le câblage des deux cartes**, sur le connecteur 8 broches de la carte écran : l'UART0 (GPIO43 TX vers le RX de la
  XIAO, D7 ; GPIO44 RX depuis son TX, D6), GND, et le 3V3 pour alimenter la XIAO, pour qu'un seul bouton coupe les
  deux. Proposé : la ligne de réveil de la carte vision sur GPIO16, reliée à une broche RTC de la XIAO, qui la
  réveille d'une veille profonde (D0 à D5, GPIO1 à 6, [brochage](https://www.espboards.dev/esp32/xiao-esp32s3-sense/)),
  à choisir au montage. La console de la XIAO quitte son UART0. Celle de la carte écran est déjà sur l'USB ; seul le
  message de démarrage de sa ROM peut passer sur l'UART0, et le protocole ignore toute ligne qui ne commence pas par
  `> ` ([protocole](protocole.md#côté-firmware)).
- Déjà en ma possession : fer à souder, multimètre, câble USB-C qui transmet les données.
- **La polarité de la batterie** n'est pas standard sur ces fiches : la vérifier au multimètre avant de la brancher.

Options écartées pour la 1,32", relevées du 27 au 31 octobre :

| Option                                                      | Total                            | Pourquoi                                                                                                                     |
| ----------------------------------------------------------- | -------------------------------- | ---------------------------------------------------------------------------------------------------------------------------- |
| Écran, caméra et câbles sur AliExpress, batterie sur Amazon | 88,65 €                          | 12,33 € de port et 24,04 € de frais d'import nets (30,28 €, dont 6,24 € pris en charge par AliExpress)                       |
| Tout sur Amazon                                             | 82,78 €                          | Caméra à 32,90 € et câbles à 7,99 €                                                                                          |
| Écran sur AliExpress au lieu d'Amazon                       | environ 67 €                     | [22,19 €](https://fr.aliexpress.com/item/1005012964258234.html) + 5,59 € de port + environ 6 € de frais d'import ; plus lent |
| AliExpress, articles expédiés depuis l'UE                   | -                                | Pas d'écran 1,32" ; XIAO à 29,99 €                                                                                           |
| Temu                                                        | -                                | Mêmes frais pour les colis venus de Chine, pas de boutique officielle pour ces cartes                                        |
| Eckstein (Allemagne), Opencircuit (Pays-Bas) pour l'écran   | -                                | En rupture chez Eckstein ; 32,50 € + port chez Opencircuit, disponible le 11 novembre                                        |
| waveshare.com + Reichelt                                    | 55 à 65 € avant frais d'import   | Port, TVA et frais de Waveshare connus seulement au panier ([expédition](https://www.waveshare.com/shipping))                |
| Reichelt (Allemagne) pour la caméra                         | 24,88 € sans les câbles          | 15,40 € + 9,48 € de port DPD vers la France ; en stock tout de suite                                                         |
| DigiKey, Mouser ou Farnell pour la caméra                   | 14,50 à 15 € + 12 à 25 € de port | Intéressant seulement avec d'autres achats                                                                                   |

Plus tard, avec le boîtier :

- Anneau de porte-clé et chaînette.
- Boîtier imprimé en 3D.

Écarté : le vibreur ([objet-v1.md](objet-v1.md)).

Outils, si tu ne les as pas : fer à souder à panne fine (le câblage des deux cartes), multimètre (polarité de la
batterie, courants), câble USB-C qui transmet les données.

## Alternative si on veut plus petit

La 1,32" donne un galet vers 48-50 mm avec la caméra en façade, pour le même écran (466 × 466, CO5300). Il lui faut
alors un module accéléromètre avec une broche d'interruption, branché sur son connecteur SH1.0 (GPIO1, 2, 5 ou 21 ;
GPIO0, 3 et 45 servent au démarrage), et une résistance R4 remplacée : sa puce de charge, une ETA6098, charge à 2 A
avec R4 = 82 kΩ ([schéma](https://files.waveshare.com/wiki/ESP32-S3-Touch-AMOLED-1.32/ESP32-S3-Touch-AMOLED-1.32-Schematic.pdf)).
Le firmware la connaît déjà (`firmware/boards/waveshare_132`), jamais essayée sur la carte. Le 17 novembre, elle était à
32,99 € sur Amazon.fr, plus 6 € de port, livrée le 17 décembre.

La 1,43" fait un galet d'au moins 55-58 mm, comme la 1,75", sans bouton marche/arrêt ni signal TE relié.
