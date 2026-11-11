# 8. Les mêmes outils, aux mêmes versions, en local et en CI

- Date : 2026-10-27, complété le 2026-11-03
- Statut : accepté

## Contexte

Avec `latest`, la CI tournait déjà sous Node 26 et le poste sous Node 24 (novembre 2026), et un nouveau clang-format
aurait pu la faire échouer sur du code inchangé. Installé par mise, emsdk ajoutait son propre clang et son propre node
au PATH : ils masquaient ceux du système et faisaient boucler ccache. Le tag `latest` de l'image officielle d'ESP-IDF
suit la branche de développement (`master`), trop instable pour flasher une vraie carte
([doc de l'image](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/tools/idf-docker-image.html)).

## Décision

- **Toutes les versions sont figées.** mise installe les outils de `mise.toml` à leur version exacte (Node, Python, uv,
  CMake, clang-format et clang-tidy depuis leurs paquets PyPI, les linters du shell, du workflow et du Python, esptool),
  en local comme en CI ; mise lui-même est figé (`min_version`, et la version de la CI). La CI tourne sur l'image
  `ubuntu-26.04`. uv utilise le Python de mise au lieu d'en télécharger un.
- **Les chaînes de compilation tournent dans Docker** : Emscripten dans l'image figée par `engine/wasm/Dockerfile`,
  ESP-IDF dans celle de `firmware/Dockerfile` (la dernière version publiée), avec la même commande en local et en CI.
  Rien à installer d'ESP-IDF : flash et port USB passent par esptool et pyserial (`scripts/requirements.txt`, par uv).
- **Le compilateur C du PC reste celui du système**, mais `engine/CMakeLists.txt` refuse un clang plus ancien que celui
  de la CI (21), qui n'est pas testé.
- **Chaque nouvelle version arrive en pull request**, que la CI vérifie avant qu'on l'accepte : Renovate pour les outils
  de `mise.toml`, que Dependabot ne sait pas lire, Dependabot pour npm, les images Docker et les actions. Les actions
  sont épinglées par commit et les images par tag et digest, puisqu'un tag peut être déplacé ; les paquets npm, par
  `package-lock.json`, que `npm ci` suit à la lettre.
- **Un seul point d'entrée** : `mise run check` lance tout ce qu'un changement doit passer (`npm run check`,
  `engine:check`, les tests Playwright). La CI lance ces trois commandes, chacune dans son job pour aller plus vite, au
  lieu de recopier leurs étapes : une étape ajoutée au contrôle ne peut pas manquer en CI. `npm run check`, la partie
  rapide (environ 10 s), tourne aussi avant chaque push ; ce qui demande Docker reste à la CI.

## Conséquences

- Un échec en CI se reproduit en local avec la même commande.
- La compilation du firmware passe par ccache, gardé dans `firmware/.ccache` et entre deux runs en CI : une
  recompilation complète passe d'environ 1 min à 26 s en local.
