# 8. Les mêmes outils, aux mêmes versions, en local et en CI

- Date : 2026-10-27, complété le 2026-11-03, le 2026-11-17 et le 2026-11-23
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
- **Chaque nouvelle version arrive en pull request**, que la CI vérifie avant qu'on l'accepte, toutes par Renovate : les
  outils de `mise.toml`, la version de mise de la CI, pyserial, npm, les images Docker et les actions. Les actions
  sont épinglées par commit et les images par tag et digest, puisqu'un tag peut être déplacé ; les paquets npm, par
  `package-lock.json`, que `npm ci` suit à la lettre. Renovate regroupe ses mises à jour en une pull request par
  semaine (les versions majeures ensemble dans une seconde), avec un tableau de bord (une issue) qui montre ce qu'il
  attend. Rien ne se fusionne seul : on choisit le moment. Jusqu'au 2026-11-23, Dependabot gérait npm, les images et
  les actions : il ne lit pas `mise.toml`, ne suit pas les versions correctives d'ESP-IDF depuis un tag à deux
  chiffres (`v6.2`), et laissait monter `@types/node` à la main avec Node. Il ne garde que ses alertes de sécurité et
  ses correctifs de sécurité, réglés dans les paramètres du dépôt : Renovate les lui laisse, pour qu'une alerte ne
  donne pas deux pull requests.
- **Les règles de GitHub et les versions publiées passent par le dépôt.** Les rulesets de `main` et des tags de version
  sont dans `.github/rulesets/` : un test compare les checks requis de `main` aux jobs de la CI, et
  `mise run github:rulesets` les applique ([décision 14](0014-une-seule-branche.md) : une seule branche depuis le
  2026-11-23). Un tag de version publie le firmware de chaque carte dans une Release (`release.yml`), après avoir
  relancé la CI sur ce commit, au lieu de dépendre des artefacts de la CI, gardés 30 jours.
- **Un seul point d'entrée** : `mise run check` lance tout ce qu'un changement doit passer (`npm run check`,
  `engine:check`, les tests Playwright). La CI lance ces trois commandes, chacune dans son job pour aller plus vite, au
  lieu de recopier leurs étapes : une étape ajoutée au contrôle ne peut pas manquer en CI. `npm run check`, la partie
  rapide (environ 10 s), tourne aussi avant chaque push ; ce qui demande Docker reste à la CI.

## Conséquences

- Un échec en CI se reproduit en local avec la même commande.
- La compilation du firmware passe par ccache, gardé dans `firmware/.ccache` et entre deux runs en CI : une
  recompilation complète passe d'environ 1 min à 26 s en local.
