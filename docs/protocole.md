# Protocole avec l'objet

Des lignes de texte, les mêmes sur le port USB de l'ESP32-S3 (celui du journal et des mesures) et, à l'arrivée de la
carte vision, sur sa liaison UART. Pourquoi du texte : [décision 5](adr/0005-protocole-texte.md).

## Les lignes

- Les commandes commencent par `>` et les réponses par `<` ; toute autre ligne est du journal. Chaque réponse part sur
  une ligne à elle (le journal, coupé quand personne ne lit le port, ne s'y colle pas). Une ligne fait au plus 94
  caractères : au-delà, `< err line too long`, et le reste est jeté.
- `> hello` répond
  `< lilpet fw=<version> engine=<version> board=<carte> camera=<0|1> motion=<0|1> reset=<raison> storage=<état>
state=<repos> battery=<mV|none>`.
  `reset` dit pourquoi la puce a démarré : le nom de `esp_reset_reason_t` d'ESP-IDF en minuscules, sans `ESP_RST_`
  (`poweron` à la mise sous tension, `usb` après un flash ou un reset par le port USB, `sw`, `panic`, `task_wdt`,
  `int_wdt`, `brownout`…). `storage` dit ce que la carte a trouvé de sa mémoire flash au démarrage : `ok`, `erased`
  quand elle a dû l'effacer, pleine ou dans un autre format (le look gardé avec), ou `none` quand elle ne peut pas s'en
  servir (les looks ne sont alors pas gardés). `state` dit où en sont les yeux : `awake`, `drowsy` (l'air ensommeillé
  qui précède le sommeil) ou `asleep` (écran éteint). `battery` donne la tension de la batterie en millivolts, lue
  toutes les 10 s, ou `none` sans batterie ou sur une carte qui ne sait pas la lire. Comme toute commande, `hello`
  réveille les yeux, mais sa réponse dit encore l'état d'avant.
- `> look AE0002 FF5252 FF5252 dot` (œil, pupille gauche, pupille droite, type) répond `< ok`, `< err <raison>`, ou
  `< err busy` si la file de la boucle de dessin est pleine (comme les commandes des yeux, plus bas). Le look s'affiche
  même si la mémoire flash refuse de le garder (`< err shown, but cannot save the look in flash`). Un nouveau look est
  écrit en flash tout de suite, puis au plus toutes les 2 s, pour ne pas l'user : un look qui arrive plus tôt répond
  `< ok` et s'écrit 2 s après le précédent, sauf si un autre le remplace d'ici là : le dernier envoyé est celui que la
  carte garde, un peu plus de 2 s plus tard au pire. Si cette écriture-là échoue, la carte a déjà répondu `< ok` : seul
  son journal le dit. Qu'une écriture échoue tout de suite ou plus tard, la carte réessaie toutes les 2 s. Un look qui
  attend son tour se perd si la carte perd l'alimentation ou redémarre avant. Seul un look envoyé s'écrit : le look par
  défaut qu'une carte montre après un crash ne remplace pas celui gardé, mais le même look, envoyé, le remplace.
- `> look?` répond le look actuel, au même format : de quoi vérifier sans écran (`firmware:check-device` ; l'app ne
  s'en sert pas).
- `> imu?` répond la gravité lue par le capteur de mouvement, en g, dans le repère de l'écran (x vers la droite, y vers
  le bas, z vers celui qui regarde) : `< imu 0.000 1.000 0.000` tenu droit, ou `< err no motion sensor` sans capteur.
  Sert au calage des axes et à `firmware:check-device`.
- `> emotion <nom> [secondes]` : une émotion pour ce temps (plus de 0, 3600 au plus), ou la nouvelle humeur sans durée.
- `> react <événement>` : la réaction à un événement, par son nom dans `eyes/emotions.json` (`tap`, `gesture_like`,
  `expression_happy`…). Une ligne par changement, pas par image de la caméra. L'émotion change aussitôt, et les
  étincelles partent au plus toutes les 0,5 s, quelle que soit l'émotion : un modèle qui répète un événement ne noie
  pas l'écran.
- `> face <x> <y>` : un visage en x, y de -1 à 1, à renvoyer tant qu'il est vu : sans nouvelle position pendant 0,5 s,
  les yeux le lâchent, pour qu'une source qui s'arrête ne les laisse pas figés. Un visage absent depuis 3 s compte
  comme nouveau et déclenche `face_seen`. `> face off` dit qu'il est parti.

Ces trois dernières répondent `< ok`, `< err <raison>` pour une entrée fausse, ou `< err busy` tout de suite si la file
de la boucle de dessin est pleine (32 commandes entre deux images) : le firmware n'attend pas, sinon le port USB
jetterait les octets qui suivent et collerait des bouts de lignes. Sur la carte qui joue l'animation de mesure
(`stamp_s3a`), elles sont refusées, et un look y est gardé sans s'afficher.

`> test hang`, pour les tests seulement, répond `< ok` puis bloque la boucle de dessin pour de bon, comme une attente
qui ne finirait pas. Les commandes ont encore leur réponse, mais les yeux se figent, jusqu'à ce que le watchdog de la
boucle redémarre la puce : `CONFIG_ESP_TASK_WDT_TIMEOUT_S` (5 s) plus tard, `hello` dit `reset=task_wdt`, et la carte
montre le look par défaut, comme après tout crash (voir [firmware.md](firmware.md)). `firmware:check-device` s'en sert
pour vérifier ce watchdog et ce démarrage ; l'app ne l'envoie jamais.

`> test sleep`, pour les tests aussi, répond `< ok` puis endort les yeux tout de suite, sans l'air ensommeillé : écran
éteint, boucle de dessin en pause, et le journal le dit (`asleep: screen off`). La commande suivante les réveille
(`awake: screen on`), sauf `> test sleep` et `> test hang`, qui ne réveillent rien. Toute autre ligne qui commence par
`> `, même refusée, compte comme quelqu'un qui s'occupe de la carte : elle la réveille, ou la garde éveillée.

## Qui s'en sert

- **L'app PC** n'utilise que `hello`, `look`, `emotion` sans durée et `react`, et ne lit de `hello` que les versions et
  `board`. Elle attend 2 s la réponse à un look, 500 ms pour le reste, et 1 s pour `hello`, qu'elle pose trois fois
  avant de conclure que ce n'est pas lilpet. Une écriture refusée ferme la connexion. Elle associe une réponse à une
  demande par ordre d'arrivée ([limites connues](architecture.md#limites-connues-à-reprendre-quand-elles-gêneront)).
- **Le modèle de la carte vision** enverra `react`, `emotion` et `face` ([décision 7](adr/0007-deux-cartes-uart.md)).
  Sans cette carte, les mêmes commandes se testent depuis le PC, sur le port USB de l'AtomS3R.
- **Les scripts de carte** : `firmware:flash` vérifie la carte par `hello` avant de la flasher
  ([décision 6](adr/0006-une-image-par-carte.md)), `firmware:check-device` essaie toutes les commandes.

## Côté firmware

`lp_command_parse` (moteur, testé sur PC) lit les lignes et ignore tout ce qui ne commence pas par `> `, donc aussi le
journal de démarrage de l'autre puce sur la liaison UART. `firmware/main/commands.c` les met en file pour la boucle de
dessin, le moteur n'étant pas fait pour plusieurs tâches. Le look reçu est appliqué et gardé dans la mémoire flash (NVS
d'ESP-IDF), puis rechargé au démarrage. À l'arrivée de la XIAO, une seconde tâche donnera les lignes de l'UART au même
analyseur et répondra sur son propre flux, avec un verrou autour du look courant.
