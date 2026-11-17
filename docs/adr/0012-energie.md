# 12. Les yeux s'endorment, et la veille légère attend que le PC soit débranché

- Date : 2026-11-17
- Statut : accepté

## Contexte

L'objet v1 doit tenir une journée sur une batterie de 400 mAh, endormi presque tout le temps
([objet-v1.md](../objet-v1.md#autonomie--une-journée-endormi-presque-tout-le-temps)). En attendant la carte écran,
l'AtomS3R sert d'objet d'essai, posé sur l'Atomic Battery Base de M5Stack (200 mAh, sans jauge : un pont de 1 MΩ/1 MΩ
sur G8, [M5Stack](https://docs.m5stack.com/en/atom/Atomic_Battery_Base)).

- **La veille légère coupe le port USB.** Sur l'ESP32-S3, ESP-IDF éteint la broche USB du port série et JTAG à chaque
  veille légère (`sleep_console_usj_pad_backup_and_disable`, dans `esp_hw_support/sleep_modes.c`) : le PC perd le port
  ([Espressif](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-guides/usb-serial-jtag-console.html)).
  `usb_serial_jtag_is_connected()` dit si un hôte est là : il envoie une trame de début (SOF) chaque milliseconde, et
  sans SOF pendant 3 ms, le port compte comme débranché (`usb_serial_jtag_connection_monitor.c`).
- **Pas de veille profonde sur l'AtomS3R.** Seules les broches RTC 0 à 21 réveillent de la veille profonde, et le bouton
  est sur la broche 41. La veille légère, elle, se réveille sur n'importe quelle broche (`gpio_wakeup_enable`).
- **Le watchdog.** La boucle de dessin est sous le task watchdog (étape 13) : une boucle qui attend son réveil doit en
  sortir, sinon il redémarre la puce.

## Décision

- **Trois états, une règle pure.** `firmware/main/rest.h`, testé sur PC avec ce que les yeux montrent : éveillé ;
  après 2 min sans rien (proposé), l'air ensommeillé (`sleepy`) pendant 10 s, l'écran s'éteignant en fondu sur les 2
  dernières ; puis endormi. Durées choisies le 2026-11-17 après l'essai sur l'AtomS3R : 5 s et un écran coupé net
  faisaient un endormissement trop bref.
- **Ce qui le garde éveillé ou le réveille** : une commande (toute ligne qui commence par `> `, sauf `> test sleep` et
  `> test hang`), le bouton, ou un mouvement : la gravité s'écarte de plus de 0,15 g, sur un axe, de celle du dernier
  mouvement. Mesurer depuis ce dernier mouvement, et non depuis l'image d'avant, compte aussi une inclinaison lente.
  L'animation de mesure (`stamp_s3a`) ne s'endort jamais seule : ses mesures s'arrêteraient. Une carte sans bouton ni
  capteur de mouvement non plus : rien ne la réveillerait (les Waveshare, jusqu'au pilote de l'accéléromètre de la
  1,75", [#54](https://github.com/Nexlein/lilpet/issues/54)). `> test sleep` l'endort quand même : elle attend alors
  une commande, sans veille légère.
- **Endormi** : le rétroéclairage à 0, puis l'écran en veille (`DISPOFF`, `SLPIN`) ; la boucle de dessin s'arrête et
  sort du watchdog.
    - Branchée à un PC, la carte garde son port : elle regarde les commandes, le bouton et le mouvement toutes les 20
      ms, sans veille légère.
    - Seule (sur batterie, ou sur un chargeur sans données), elle passe en veille légère, au plus 1 s à la fois : le
      bouton la réveille aussitôt, et le minuteur, chaque seconde, le temps de lire le mouvement.
    - En veille légère, ESP-IDF laisse flotter toutes les broches (`CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND`), sauf
      celles qu'on lui nomme : la carte garde le maintien de son alimentation, le reset et la sélection de l'écran, et
      le bus I2C.
- **Au réveil** : l'écran se rallume en fondu (0,5 s), l'image est redessinée en entier, et les yeux restent
  ensommeillés 2 s avant de reprendre leur humeur, sauf si une réaction l'a remplacée. L'appui qui réveille ne compte
  pas comme une caresse tant qu'il n'est pas relâché : sans cela, chaque réveil au bouton finissait dans la joie et les
  étincelles.
- **La batterie** est lue toutes les 10 s. Sous 3,5 V, les yeux prennent l'air fatigué (`tired`) jusqu'à ce qu'elle
  remonte au-dessus de 3,6 V (proposé) ; ils reprennent alors leur humeur, sauf si l'app en a choisi une autre
  entre-temps. `hello` dit `battery=<mV>`, ou `none`, et `state=awake|drowsy|asleep` ([protocole](../protocole.md)).
- **Plus tard**, d'après les mesures du banc d'essai ([#53](https://github.com/Nexlein/lilpet/issues/53)) :
    - le réveil au mouvement par l'interruption du BMI270 (any-motion, sur INT1) au lieu du minuteur ;
    - moins d'images par seconde quand rien ne bouge, et la fréquence du processeur qui baisse avec (`esp_pm`) ;
    - la luminosité qui baisse, et l'image qui se décale un peu, contre le marquage de l'AMOLED ;
    - la carte vision, en veille profonde tant que la carte écran dort, et qui cherche le pouce levé moins souvent quand
      personne n'est là ([#55](https://github.com/Nexlein/lilpet/issues/55)) ;
    - sur la carte écran, la 1,75" ([#47](https://github.com/Nexlein/lilpet/issues/47)), la veille profonde réveillée
      par son accéléromètre (GPIO21, une broche RTC), et l'arrêt par sa puce d'alimentation AXP2101
      ([#54](https://github.com/Nexlein/lilpet/issues/54)).

## Conséquences

- Branchée au PC, la carte ne fait qu'éteindre son écran : son courant endormi se mesure sur batterie, sans câble de
  données.
- Un câble branché sur un PC pendant la veille légère : le PC ne voit la carte qu'une fois réveillée, par le bouton ou
  en la prenant en main.
- Sans PC, une prise en main se voit au plus 1 s plus tard, et un geste plus bref qu'une seconde, qui la repose comme
  elle était, peut passer inaperçu, jusqu'à l'interruption du BMI270.
- `> test sleep` endort la carte tout de suite, et `firmware:check-device` s'en sert : endormie, l'écran éteint, puis
  réveillée par une commande. Le réveil au mouvement et au bouton, et la veille légère, s'essaient à la main, sur
  batterie.
