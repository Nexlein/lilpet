# App PC

Choix faits le 2026-11-01. Complète la section « App PC » de [architecture.md](architecture.md).

État : version 1 écrite le 2026-11-01 (galerie, aperçu, envoi), testée dans le navigateur, en CI et sur l'AtomS3R. Les
commandes `emotion`, `react` et `face` répondent sur l'AtomS3R depuis le 6 novembre 2026 (`BOARD=atoms3r mise run firmware:check-device`).

## Choix

- **Page web**, ouverte dans Chrome ou Edge sur ordinateur (Web Serial). Rien à installer. Installable comme une app
  (PWA) plus tard.
- **React + TypeScript strict + Vite.** Tests : Vitest pour la logique, Playwright (déjà dans le dépôt) pour un test de
  bout en bout.
- **Style soigné**, avec une identité propre à lilpet : ludique, fond sombre, l'écran rond au centre. Pas de copie du
  site officiel.
- Hébergement possible sur GitHub Pages, à décider au moment de publier : la page serait publique.

## Version 1

1. **Galerie et aperçu**
    - Aperçu animé par le vrai moteur (WebAssembly) dans l'écran rond de 466 × 466 : le regard suit la souris, et des
      boutons d'émotion et de clignement font vivre le look.
    - Les 100 couleurs en pastilles, avec une recherche par nom et des filtres (teinte, pupilles différentes à gauche
      et à droite).
    - Les 4 types de pupille.
    - Des favoris, gardés dans le navigateur.
2. **Envoi à l'objet**
    - Bouton « Brancher lilpet » (Web Serial), puis les infos de l'objet : carte, versions du firmware et du moteur.
    - Bouton « Envoyer à lilpet » : l'objet affiche le look tout de suite et le garde en mémoire flash.
    - Une humeur choisie (`> emotion`) ou une caresse (`> react tap`) passent aussi sur l'objet, sans rien garder. La
      dernière humeur repart après `hello`. Après un rechargement de la page, l'app montre Neutre, et l'objet garde la
      sienne : il faudrait une commande `emotion?` pour la lire.
    - Journal de l'objet (les mesures qu'il écrit déjà) dans un panneau repliable.

Version 1 testée sur l'AtomS3R (voir [materiel.md](materiel.md)) : le look envoyé est gardé (`> look?`,
`BOARD=<carte> mise run firmware:check-device`), et l'AtomS3R l'affiche sur son petit écran. Le moteur sait aussi dessiner en petit
(`lp_renderer_set_size`), de quoi faire plus tard des vignettes rendues par le vrai moteur dans la galerie.

Hors version 1, dans l'ordre probable :

- looks perso (couleurs libres, forme des yeux) ;
- réglages de l'objet (luminosité, mise en veille, suivi du regard) ;
- sauvegarde et import des réglages ;
- PWA hors ligne ;
- éditeur d'animations ;
- mode marionnette (piloter les yeux en direct) ;
- mise à jour du firmware depuis le navigateur.

## Organisation du code

`app/` est un paquet npm du dépôt (workspace), avec son `package.json`, sa config Vite et ses tests.

| Dossier       | Rôle                                                                                           |
| ------------- | ---------------------------------------------------------------------------------------------- |
| `src/engine/` | Charge le moteur WebAssembly (`engine/wasm/engine.mjs`), boucle d'animation, aperçu sur canvas |
| `src/data/`   | Lit `eyes/looks.json` et construit les looks, avec la même règle que `makeLook`                |
| `src/device/` | Web Serial : connexion, lecture ligne par ligne, protocole (testé sans matériel)               |
| `src/ui/`     | Composants React : galerie, aperçu, panneau de l'objet                                         |

`engine`, `data` et `device` ne dépendent pas de React : Vitest les teste seuls, et ils resservent ailleurs (éditeur
d'animations, outils). Seul `ui` dépend de React.

## Protocole avec l'objet

Des lignes de texte sur le port USB de l'ESP32-S3, le même que celui des mesures. La carte caméra enverra les mêmes
lignes par UART :

- les commandes commencent par `>` et les réponses par `<` ; toute autre ligne est du journal ; chaque réponse part
  sur une ligne à elle (le journal, coupé quand personne ne lit le port, ne s'y colle pas) ; une ligne fait au plus 94
  caractères : au-delà, `< err line too long`, et le reste est jeté ;
- `> hello` répond `< lilpet fw=<version> engine=<version> board=<carte> camera=<0|1> motion=<0|1>` ;
- `> look AE0002 FF5252 FF5252 dot` (œil, pupille gauche, pupille droite, type) répond `< ok` ou `< err <raison>` ; le
  look s'affiche même si la mémoire flash refuse de le garder (`< err shown, but cannot save the look in flash`) ;
- `> look?` répond le look actuel, au même format : de quoi vérifier sans écran (`BOARD=<carte> mise run firmware:check-device` ;
  l'app ne s'en sert pas) ;
- `> imu?` répond la gravité lue par le capteur de mouvement, en g, dans le repère de l'écran (x vers la droite, y vers
  le bas, z vers celui qui regarde) : `< imu 0.000 1.000 0.000` tenu droit, ou `< err no motion sensor` sans capteur.
  Sert au calage des axes et à `BOARD=<carte> mise run firmware:check-device` ;
- `> emotion <nom> [secondes]` : une émotion pour ce temps (plus de 0, 3600 au plus), ou la nouvelle humeur sans durée ;
- `> react <événement>` : la réaction à un événement, par son nom dans `eyes/emotions.json` (`tap`, `gesture_like`,
  `expression_happy`…). Une ligne par changement, pas par image de la caméra. L'émotion change aussitôt, et les étincelles
  partent au plus toutes les 0,5 s, quelle que soit l'émotion ;
- `> face <x> <y>` : un visage en x, y de -1 à 1, à renvoyer tant qu'il est vu : sans nouvelle position pendant 0,5 s,
  les yeux le lâchent. Un visage absent depuis 3 s compte comme nouveau et déclenche `face_seen`. `> face off` dit
  qu'il est parti.

Ces trois dernières répondent `< ok`, `< err <raison>` pour une entrée fausse, ou `< err busy` si la boucle de dessin
ne suit plus. Sur la carte qui joue le benchmark, elles sont refusées.

L'app n'utilise que `hello`, `look`, `emotion` sans durée et `react`, et ne lit de `hello` que les versions et `board`. Le
reste sert au modèle d'IA et aux essais au port série. Elle attend 2 s la réponse à un look, 500 ms pour
le reste, et 1 s pour `hello`, qu'elle pose trois fois avant de conclure que ce n'est pas lilpet. Une écriture refusée
ferme la connexion.

Du texte simple : lisible dans le moniteur série, lu en C sans bibliothèque, facile à tester des deux côtés. Côté
firmware, le look reçu est appliqué et gardé dans la mémoire flash (NVS d'ESP-IDF), puis rechargé au démarrage.

## Dépendances à valider

| Paquet                             | Pourquoi                                                    |
| ---------------------------------- | ----------------------------------------------------------- |
| `react`, `react-dom`               | L'interface (choisi le 2026-11-01)                          |
| `vite`, `@vitejs/plugin-react`     | Serveur de développement et build, prévu par l'architecture |
| `typescript`, `@types/react(-dom)` | Typage strict                                               |
| `vitest`                           | Tests de `data`, `device` et `engine`, avec la config Vite  |

## Fini quand (version 1)

- Dans la galerie, on choisit une couleur et une pupille, et l'aperçu montre les yeux dessinés par le moteur C,
  celui de l'objet.
- Sur l'AtomS3R : connecter, envoyer un look, débrancher puis rebrancher, et `> look?` rend toujours ce look.
- `npm run check`, les tests Vitest, le build de l'app et la CI passent.

Ordre de travail prévu :

1. squelette de l'app et aperçu ;
2. galerie ;
3. protocole et mémoire côté firmware ;
4. panneau de l'objet, essai sur la carte ;
5. CI (build de l'app, Vitest).
