# App PC

Choix faits le 2026-11-01. Complète la section « App PC » de [architecture.md](architecture.md).

État : version 1 écrite le 2026-11-01 (galerie, aperçu, envoi), testée dans le navigateur et en CI. Reste à l'essayer
avec une vraie carte branchée (`> look?` après un débranchement).

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
    - Bouton « Connecter » (Web Serial), puis les infos de l'objet : versions du firmware et du moteur.
    - Bouton « Envoyer » : l'objet affiche le look tout de suite et le garde en mémoire flash.
    - Journal de l'objet (les mesures qu'il écrit déjà) dans un panneau repliable.

Version 1 testée avec un ESP32-S3 sans écran (M5Stamp S3A) : `> look?` montre que le look est gardé. Juste après,
l'**AtomS3R** (voir [materiel.md](materiel.md)) affichera les yeux sur son petit écran, une fois que le moteur saura
dessiner à une autre taille que 466 px. Cela donnera aussi des vignettes rendues par le vrai moteur dans la galerie.

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

## Protocole avec l'objet (version 1)

Des lignes de texte sur le port USB de l'ESP32-S3, le même que celui des mesures :

- les commandes commencent par `>` et les réponses par `<` ; toute autre ligne est du journal ;
- `> hello` répond `< lilpet fw=0.1.0 engine=0.6.0` ;
- `> look AE0002 FF5252 FF5252 dot` (œil, pupille gauche, pupille droite, type) répond `< ok` ou `< err <raison>` ;
- `> look?` répond le look gardé, au même format : de quoi vérifier sans écran.

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

- Dans la galerie, on choisit une couleur et une pupille, et l'aperçu montre le même rendu que le prototype (même
  moteur).
- Sur le M5Stamp S3A : connecter, envoyer un look, débrancher puis rebrancher, et `> look?` rend toujours ce look.
- `npm run check`, les tests Vitest, le build de l'app et la CI passent.

Ordre de travail prévu :

1. squelette de l'app et aperçu ;
2. galerie ;
3. protocole et mémoire côté firmware ;
4. panneau de l'objet, essai sur le M5Stamp ;
5. CI (build de l'app, Vitest).
