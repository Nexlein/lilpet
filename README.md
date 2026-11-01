# lilpet

Porte-clé « animal numérique » perso, inspiré du Starboy de lilguy / CREATURE.
Projet strictement personnel, non commercial.

## Objectif

- Écran rond de haute qualité (AMOLED 466×466 visé).
- Réactions grâce à une caméra (visage, gestes de la main).
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
- [ ] Firmware ESP32 (`firmware/`) : yeux en direct, look reçu de l'app et gardé ; écran et caméra à tester sur la carte
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
Les scènes de référence, que le futur moteur C devra reproduire, sont sur
http://127.0.0.1:8466/prototype/reference.html.

## Lancer l'app PC

```bash
npm run app:dev
```

Puis ouvrir http://localhost:8467/ dans Chrome ou Edge (le moteur doit être compilé : `mise run engine:wasm`). On y
choisit une couleur et une pupille, avec l'aperçu animé par le vrai moteur. « Brancher lilpet » ouvre la liste des ports
USB : choisir la carte flashée avec le firmware, puis « Envoyer à lilpet ». La carte garde le look même
éteinte, et après un nouveau flash. Fermer `mise run firmware:monitor` avant : un port USB ne s'ouvre qu'une fois.

## Vérifier avant de commiter

```bash
npm install
npm run check
```

Vérifie le formatage (Prettier), le code (oxlint), `eyes/looks.json` et `prototype/scenes.json`, puis les types et
les tests de l'app PC.

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

La CI GitHub lance tout ça à chaque push, plus la compilation du firmware ESP32-S3.

## Tester sur un ESP32-S3

Le firmware affiche les yeux en direct sur l'écran 1,32" et écrit ses temps de dessin et d'envoi sur le port USB. Il
reconnaît aussi l'AtomS3R de M5Stack et y affiche les yeux sur son écran de 128 px ; quand on le tourne, le visage
reste à l'horizontale grâce à son capteur de mouvement. Sans
écran, par exemple sur une M5Stamp S3A, il tourne quand même et mesure la vitesse du moteur sur la puce. La compilation
passe par Docker et le flash par `uv`, sans installer ESP-IDF :

```bash
mise run firmware:build
```

```bash
mise run firmware:flash
```

```bash
mise run firmware:monitor
```

Les temps s'affichent toutes les 10 secondes (quitter avec Ctrl+]). Pour vérifier qu'une carte flashée répond bien à
l'app (réponse, look gardé après un redémarrage ; son look est remis à la fin) :

```bash
mise run firmware:check-device
```

Sous Linux, l'utilisateur doit être dans le groupe
`dialout` pour accéder au port USB. Si le flash ne trouve pas la carte : la débrancher, maintenir son bouton BOOT, puis
la rebrancher. La CI garde aussi le binaire (`firmware-esp32s3` dans les artefacts du job ESP32-S3).

## Arborescence

| Dossier           | Contenu                                                                                 |
| ----------------- | --------------------------------------------------------------------------------------- |
| `notes/`          | Notes de recherche, matériel, architecture, yeux                                        |
| `references/`     | Captures d'inspiration                                                                  |
| `eyes/looks.json` | Les 100 couleurs officielles ; un look = une couleur + un type de pupille               |
| `prototype/`      | Référence de design : rendu des yeux (`eyes.js`), page interactive, scènes de référence |
| `engine/`         | Moteur des yeux en C, compilé pour le PC, en WebAssembly et pour l'ESP32                |
| `firmware/`       | Application ESP32-S3 : pilote de l'écran, yeux en direct, look reçu de l'app, mesures   |
| `app/`            | App PC (React, TypeScript, Vite) : choisir les yeux, les envoyer à l'objet par USB      |
| `scripts/`        | Validation des données                                                                  |
