# 14. Une seule branche, `main`, et des tags pour les versions

- Date : 2026-11-23
- Statut : accepté

## Contexte

Le dépôt avait deux branches : `dev`, par défaut, où arrivait chaque pull request, et `main`, qui ne recevait que des
fusions de `dev`. En deux semaines, 8 versions (v0.7.0 à v0.8.1) ont demandé 8 pull requests de `dev` vers `main`,
chacune avec son tour de CI. Or ce qui est publié part des tags, pas de `main` : `release.yml` publie la Release d'un
tag annoté, et GitHub Pages ne se déploie que depuis un tag `v*`. `main` ne servait qu'à montrer la dernière version.
Le projet est mené seul.

## Options étudiées

- **Garder `dev` et `main`** : une pull request de plus par version, pour une branche que rien ne lit.
- **`dev` seule** : pas de renommage, mais un nom que les outils ne supposent pas (Renovate, Dependabot et GitHub
  partent de la branche par défaut, d'ordinaire `main`).
- **`main` seule, et des tags pour les versions.**

## Décision

- **`main` est la seule branche**, et la branche par défaut. Tout y arrive par une pull request, fusionnée par un
  commit de fusion quand la CI est verte (`.github/rulesets/main.json`).
- **Une version est un tag annoté** sur le commit de fusion de sa pull request de version (`docs/recettes.md`) ;
  `release.yml` vérifie toujours qu'il est sur `main`.
- **Un tag de version ne se déplace ni ne s'efface** (`.github/rulesets/tags.json`) : une Release désigne pour toujours
  le commit d'où viennent ses fichiers. Une erreur se corrige par la version suivante.
- **Le passage** : l'ancienne `main` est supprimée, puis `dev` est renommée en `main` sur GitHub, qui redirige les
  liens vers l'ancien nom et reporte la base des pull requests ouvertes
  ([GitHub](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/managing-branches-in-your-repository/renaming-a-branch)).
  Les commits de fusion de l'ancienne `main` restent atteignables par leurs tags, de v0.7.0 à v0.8.1.

## Conséquences

- Une pull request et un tour de CI de moins par version.
- Plus de branche qui montre la dernière version publiée : ce sont les tags et la page des Releases.
- Corriger une version publiée alors que `main` porte du travail non publié demandera une branche partie de son tag, le
  jour où le cas se présentera.
- Le cache du firmware en CI, rangé par branche, se refait une fois sur `main`.
- Un clone fait avant le renommage se met à jour avec `git branch -D main` (son commit reste celui de v0.8.1), puis
  `git branch -m dev main`, `git fetch origin --prune`, `git branch -u origin/main main` et
  `git remote set-head origin -a`.
