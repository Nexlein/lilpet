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
- [x] Prototype des yeux dans le navigateur, figé comme référence de design → `prototype/`
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

## Lancer le prototype

```bash
npm run serve
```

Puis ouvrir http://127.0.0.1:8466/prototype/ (un serveur est nécessaire pour charger les modules et les données).
Le sélecteur « Rendu » affiche le prototype, le moteur C (après `mise run engine:wasm`) ou les deux côte à côte, pilotés
par les mêmes commandes.
Les scènes de référence, que le moteur C reproduit, sont sur
http://127.0.0.1:8466/prototype/reference.html.

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
npm install
npm run check
```

Vérifie le formatage (Prettier), le code (oxlint), les données (`eyes/` et `prototype/scenes.json`), que les fichiers
générés sont à jour (`engine/tests/js_vectors.h`, `engine/src/emotions_table.h`), puis les types et les tests de l'app
PC.

Pour le moteur C (outils installés par `mise install`, Docker nécessaire pour le WebAssembly) :

```bash
mise run engine:check
```

Vérifie le formatage du C, lance les tests avec les détecteurs d'erreurs mémoire, l'analyse clang-tidy et la
compilation WebAssembly. Ensuite, pour comparer le moteur C au prototype scène par scène et ouvrir l'app PC compilée
(`npm run app:build` avant ; première fois : `npx playwright install --only-shell chromium`) :

```bash
npm run test:parity
```

La même comparaison est visible sur http://127.0.0.1:8466/prototype/reference.html. Pour mesurer la vitesse du rendu
(temps par image, à comparer sur une même machine) :

```bash
mise run engine:bench
```

La CI GitHub lance tout ça sur `main`, `dev` et chaque pull request, plus la compilation du firmware pour chaque carte.

## Tester sur un ESP32-S3

Il y a une image de firmware par carte, car rien sur la puce ne les distingue à coup sûr : l'AtomS3R et l'écran rond
Waveshare ont le même boîtier. Flasher l'image d'une autre carte piloterait les mauvaises broches. `BOARD` vaut :

- `atoms3r` : l'AtomS3R de M5Stack, écran de 128 px. Quand on le tourne, le visage reste à l'horizontale, et il réagit :
  fâché à l'envers, étourdi quand on le secoue, content quand on appuie sur son écran.
- `waveshare_132` : l'écran rond 1,32", pas encore essayé sur la vraie carte.
- `stamp_s3a` : une M5Stamp S3A sans écran, qui mesure la vitesse du moteur à 466 px sur la puce.

La compilation passe par Docker, sans installer ESP-IDF :

```bash
BOARD=atoms3r mise run firmware:build
```

```bash
BOARD=atoms3r mise run firmware:flash
```

```bash
mise run firmware:monitor
```

Les temps de dessin et d'envoi s'affichent toutes les 780 images, soit environ 12 s (quitter avec Ctrl+]). Pour vérifier
qu'une carte flashée répond bien à l'app (réponse, look gardé après un redémarrage, commandes, entrées refusées ; son
look est remis à la fin) :

```bash
mise run firmware:check-device
```

Sous Linux, l'utilisateur doit être dans le groupe
`dialout` pour accéder au port USB. Si le flash ne trouve pas la carte : la débrancher, maintenir son bouton BOOT, puis
la rebrancher. La CI garde aussi les binaires (`firmware-<carte>` dans les artefacts des jobs ESP32-S3).

## Arborescence

| Dossier       | Contenu                                                                                  |
| ------------- | ---------------------------------------------------------------------------------------- |
| `notes/`      | Notes de recherche, matériel, architecture, yeux                                         |
| `references/` | Captures d'inspiration                                                                   |
| `eyes/`       | Les 100 couleurs officielles et les émotions et réactions, en données                    |
| `prototype/`  | Référence de design : état et dessin des yeux, page interactive, scènes de référence     |
| `engine/`     | Moteur des yeux en C, compilé pour le PC, en WebAssembly et pour l'ESP32                 |
| `firmware/`   | Application ESP32-S3, une image par carte (`boards/`) : pilotes, yeux en direct, mesures |
| `app/`        | App PC (React, TypeScript, Vite) : choisir les yeux, les envoyer à l'objet par USB       |
| `scripts/`    | Validation des données, fichiers générés, contrôle d'une carte branchée                  |
| `tests/`      | Tests Playwright : parité avec le prototype, données, app PC                             |
