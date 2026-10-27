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
- [ ] Choix final du matériel et commande → [notes/materiel.md](notes/materiel.md)
- [ ] Firmware ESP32 (`firmware/`, créé le moment venu)
- [ ] Boîtier 3D (`case/`, créé le moment venu)
- [ ] App PC pour changer les yeux et les animations

## Lancer le prototype

```bash
npm run serve
```

Puis ouvrir http://127.0.0.1:8466/prototype/ (un serveur est nécessaire pour charger les modules et les données).
Les scènes de référence, que le futur moteur C devra reproduire, sont sur
http://127.0.0.1:8466/prototype/reference.html.

## Vérifier avant de commiter

```bash
npm install
npm run check
```

Vérifie le formatage (Prettier), le code (oxlint), `eyes/looks.json` et `prototype/scenes.json`.

Pour le moteur C (outils installés par `mise install`, Docker nécessaire pour le WebAssembly) :

```bash
mise run engine:check
```

Vérifie le formatage du C, lance les tests avec les détecteurs d'erreurs mémoire, l'analyse clang-tidy et la
compilation WebAssembly. Ensuite, pour comparer le moteur C au prototype scène par scène (première fois :
`npx playwright install chromium`) :

```bash
npm run test:parity
```

La même comparaison est visible sur http://127.0.0.1:8466/prototype/reference.html. Pour mesurer la vitesse du rendu
(temps par image, à comparer sur une même machine) :

```bash
mise run engine:bench
```

La CI GitHub lance tout ça à chaque push, plus la compilation du firmware ESP32-S3.

## Arborescence

| Dossier           | Contenu                                                                                 |
| ----------------- | --------------------------------------------------------------------------------------- |
| `notes/`          | Notes de recherche, matériel, architecture, yeux                                        |
| `references/`     | Captures d'inspiration                                                                  |
| `eyes/looks.json` | Définition des looks (couleur, type de pupille, couleurs des pupilles)                  |
| `prototype/`      | Référence de design : rendu des yeux (`eyes.js`), page interactive, scènes de référence |
| `engine/`         | Moteur des yeux en C, compilé pour le PC, en WebAssembly et pour l'ESP32                |
| `firmware/`       | Application ESP32-S3 (pour l'instant : vérifie seulement que le moteur compile)         |
| `scripts/`        | Validation des données                                                                  |
