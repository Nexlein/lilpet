# lilpet

Porte-clé « animal numérique » perso, inspiré du Starboy de lilguy / CREATURE.
Projet strictement personnel, non commercial.

## Objectif

- Écran rond de haute qualité (AMOLED 466×466 visé).
- Réactions aux mouvements et à ce que voit une caméra : le visage, les gestes de la main, les expressions de la
  personne. Pas la voix. Un modèle d'IA dans l'objet traite l'image et choisit les réactions.
- Yeux dans le même esprit que le Starboy : 4 types de pupilles, des variantes de couleur, beaucoup d'animations.
- Forme simple : galet rond d'environ 45-50 mm (à affiner).
- Pas de rareté ni de tirage au sort : c'est moi qui choisis. À terme, une **petite app sur PC** pour changer ses
  yeux et lui ajouter des animations.

## Où on en est

- [x] Recherche sur le Starboy → [notes/recherche.md](notes/recherche.md)
- [x] Analyse des yeux → [notes/yeux.md](notes/yeux.md)
- [x] Prototype des yeux dans le navigateur, référence de design jusqu'à ce que le moteur C reprenne tout (retiré en
      novembre 2026)
- [x] Architecture choisie → [notes/architecture.md](notes/architecture.md)
- [x] Moteur des yeux en C, compilé pour le PC, en WebAssembly et pour l'ESP32 (`engine/`)
- [x] Choix du matériel → [notes/materiel.md](notes/materiel.md)
- [ ] Commande du matériel
- [ ] Firmware ESP32 (`firmware/`) : une image par carte, yeux en direct, look, émotions et réactions reçus par USB ;
      écran rond à tester sur la carte
- [ ] Vision : modèle dans l'objet (gestes, visage, expressions) qui choisit les réactions ; le firmware sait déjà les
      recevoir
- [ ] Boîtier 3D (`case/`, créé le moment venu)
- [x] App PC, version 1 : choisir les yeux et les envoyer à l'objet → `app/`, [notes/app.md](notes/app.md)
- [ ] App PC : looks perso, réglages, animations

## Installer

Prérequis : [mise](https://mise.jdx.dev), Docker (compilations WebAssembly et firmware), clang 21 ou plus récent (tests
du moteur sur le PC) et Chrome ou Edge (Web Serial, pour l'app PC).

```bash
mise trust
mise run setup
```

`mise run setup` installe les outils de `mise.toml` (Node, Python, uv, CMake, clang-format…), les paquets npm avec
`npm ci`, qui active aussi les hooks git, et le navigateur des tests Playwright. À refaire dans chaque nouveau worktree
git : sans `npm ci`, git n'y lance aucun hook.

## Lancer l'app PC

```bash
npm run app:dev
```

Puis ouvrir http://localhost:8467/ dans Chrome ou Edge (le moteur doit être compilé : `mise run engine:wasm`). On y
choisit une couleur et une pupille, avec l'aperçu animé par le vrai moteur. « Brancher lilpet » ouvre la liste des ports
USB : choisir la carte flashée avec le firmware, puis « Envoyer à lilpet ». La carte garde le look même
éteinte, et après un nouveau flash. Une humeur choisie ou une caresse passent aussi sur la carte. Fermer `mise run firmware:monitor` avant : un port USB ne s'ouvre qu'une fois.

## Vérifier avant de commiter

```bash
mise run check
```

Lance tout ce qu'un changement doit passer, comme la CI : `npm run check`, les contrôles du moteur C avec ses rendus de
référence, et les tests Playwright. Pour un changement du firmware, ajouter `mise run firmware:build-all`.

- `npm run check`, la partie rapide (une dizaine de secondes), tourne aussi avant chaque `git push` : formatage
  (Prettier sur les fichiers suivis par git, ruff pour le Python), linters (oxlint, shellcheck pour les hooks, actionlint
  pour la CI, ruff), données (`eyes/` et `engine/tests/scenes.json`), fichiers générés à jour
  (`engine/src/emotions_table.h`, `engine/tests/scenes_table.h`), scripts de carte sur un faux port, carte du code
  (`.claude/codemap.md`), puis types et tests de l'app PC.
- `mise run engine:check` vérifie le formatage du C, lance les tests avec les détecteurs d'erreurs mémoire, l'analyse
  clang-tidy et la compilation WebAssembly, puis compare au pixel près les rendus de référence de
  `engine/tests/goldens/` : chaque scène de `engine/tests/scenes.json`, et une animation scriptée relevée toutes les 16
  étapes. Ce qui diffère est écrit dans `engine/build/goldens/`. Quand les yeux changent exprès, `npm run goldens` réécrit ces images ; les
  regarder avant de les commiter.
- `npm run test:playwright` ouvre l'app PC compilée avec une fausse carte et essaie les scripts de données sur des
  données cassées.

Avant chaque commit, le hook formate les fichiers indexés, vérifie les fichiers générés quand leurs sources changent et
régénère la carte du code (`npm run codemap`), qui liste chaque fichier avec son rôle, ce qu'il exporte, ce qu'il
utilise et qui l'utilise.

Pour mesurer la vitesse du rendu (temps par image, à comparer sur une même machine) :

```bash
mise run engine:bench
```

La CI GitHub lance tout ça sur `main`, `dev` et chaque pull request, plus la compilation du firmware pour chaque carte.
Tout changement passe par une pull request vers `dev`, la branche par défaut, et n'est mergé qu'avec la CI verte ;
`main` ne reçoit que des merges de `dev`.

## Tester sur un ESP32-S3

Il y a une image de firmware par carte, car rien sur la puce ne les distingue à coup sûr : l'AtomS3R et l'écran rond
Waveshare ont le même boîtier. Flasher l'image d'une autre carte piloterait les mauvaises broches. `BOARD` vaut :

- `atoms3r` : l'AtomS3R de M5Stack, écran de 128 px. Quand on le tourne, le visage reste à l'horizontale, et il réagit :
  fâché à l'envers, étourdi quand on le secoue, content quand on appuie sur son écran.
- `waveshare_132` : l'écran rond 1,32", pas encore essayé sur la vraie carte.
- `stamp_s3a` : une M5Stamp S3A sans écran, qui mesure la vitesse du moteur à 466 px sur la puce.

La compilation passe par Docker, sans installer ESP-IDF. Une seule commande s'occupe d'un objet : compiler, flasher,
vérifier.

```bash
BOARD=atoms3r mise run firmware:deploy
```

Elle prend la seule carte branchée, ou celle de `PORT`. Elle refuse une carte qui répond sous un autre nom (`FORCE=1`
pour passer outre), et chaque image s'arrête au démarrage sur une puce d'un autre boîtier, avant de toucher une broche.
Les étapes existent aussi une par une : `firmware:build`, `firmware:flash`, `firmware:check-device`. Le contrôle parle à
la carte comme l'app (réponse, look gardé après un redémarrage, commandes, entrées refusées) et remet son look à la fin.

Pour lire ce que la carte écrit, dont les temps de dessin et d'envoi toutes les 1080 images, soit environ 17 s (quitter
avec Ctrl+]) :

```bash
mise run firmware:monitor
```

Pour ajouter une carte dont les composants existent : créer `firmware/boards/<carte>/` avec `board.cmake` et
`board_config.h`. Rien d'autre à toucher, la CI la compile d'elle-même.

Sous Linux, l'utilisateur doit être dans le groupe
`dialout` pour accéder au port USB. Si le flash ne trouve pas la carte : la débrancher, maintenir son bouton BOOT, puis
la rebrancher. La CI garde aussi les binaires (`firmware-<carte>` dans les artefacts des jobs ESP32-S3).

## Arborescence

| Dossier     | Contenu                                                                                  |
| ----------- | ---------------------------------------------------------------------------------------- |
| `notes/`    | Notes de recherche, matériel, architecture, yeux                                         |
| `eyes/`     | Les 100 couleurs officielles et les émotions et réactions, en données                    |
| `engine/`   | Moteur des yeux en C, compilé pour le PC, en WebAssembly et pour l'ESP32                 |
| `firmware/` | Application ESP32-S3, une image par carte (`boards/`) : pilotes, yeux en direct, mesures |
| `app/`      | App PC (React, TypeScript, Vite) : choisir les yeux, les envoyer à l'objet par USB       |
| `scripts/`  | Validation des données, fichiers générés, contrôle d'une carte branchée                  |
| `tests/`    | Tests Playwright : app PC, scripts de données                                            |
