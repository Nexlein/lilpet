# lilpet

Porte-clé « animal numérique » perso, inspiré du Starboy de lilguy / CREATURE.
Projet strictement personnel, non commercial.

## Objectif

- Écran rond de haute qualité (AMOLED 466×466 visé).
- Réactions grâce à une caméra (visage, gestes de la main).
- Yeux dans le même esprit que le Starboy : 4 types de pupilles, des variantes de couleur, beaucoup d'animations.
- Forme simple : galet rond d'environ 45-50 mm (à affiner).

## Où on en est

- [x] Recherche sur le Starboy → [notes/recherche.md](notes/recherche.md)
- [x] Analyse des yeux → [notes/yeux.md](notes/yeux.md)
- [ ] Prototype des yeux dans le navigateur → `prototype/`
- [ ] Choix final du matériel et commande → [notes/materiel.md](notes/materiel.md)
- [ ] Firmware ESP32 (`firmware/`, créé le moment venu)
- [ ] Boîtier 3D (`case/`, créé le moment venu)

## Lancer le prototype

```bash
python3 -m http.server 8466 --bind 127.0.0.1
```

Depuis la racine du repo, puis ouvrir http://127.0.0.1:8466/prototype/ (un serveur est nécessaire pour charger `eyes/looks.json`).

## Arborescence

| Dossier | Contenu |
|---|---|
| `notes/` | Notes de recherche, matériel, yeux |
| `references/` | Captures d'inspiration |
| `eyes/looks.json` | Définition des looks (couleur, type de pupille, couleurs des pupilles) |
| `prototype/` | Rendu des yeux dans le navigateur (466×466) |
