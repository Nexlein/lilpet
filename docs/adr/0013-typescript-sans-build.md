# 13. Les scripts en TypeScript, que Node lance tel quel

- Date : 2026-11-23
- Statut : accepté

## Contexte

L'app PC était en TypeScript, mais les scripts du dépôt (générateurs des tables C, rendus de référence, validateur des
données, carte du code, tests), le côté JavaScript du moteur WebAssembly et les fichiers de configuration étaient en
JavaScript (`.mjs`), sans aucune vérification de types. Les types du moteur vivaient à part, dans un `engine.d.mts`
écrit à la main, qu'un test comparait à la liste des pupilles pour qu'ils ne divergent pas.

Node lance un fichier `.ts` sans étape de compilation : il efface les types et exécute le reste, sans les vérifier
([Node.js](https://nodejs.org/api/typescript.html)). Cela ne marche que pour la syntaxe qui s'efface : pas d'`enum`, de
`namespace` ni de paramètres de constructeur promus en propriétés, que l'option `erasableSyntaxOnly` de TypeScript
refuse ([TypeScript 5.8](https://devblogs.microsoft.com/typescript/announcing-typescript-5-8/)). Les imports relatifs
doivent nommer le fichier `.ts`, et les types s'importent avec `import type`. lint-staged lit `.lintstagedrc.ts`
([lint-staged](https://github.com/lint-staged/lint-staged#configuration)), Vite et Playwright lisent leur configuration
en TypeScript.

## Options étudiées

- **Rester en JavaScript**, éventuellement avec des types en JSDoc : rien à changer, mais des types que rien ne vérifie,
  ou une syntaxe de commentaires lourde.
- **TypeScript compilé** (tsc vers `dist/`, ou un lanceur comme tsx) : une étape ou une dépendance de plus avant chaque
  script.
- **TypeScript lancé tel quel par Node, vérifié par `tsc`.**

## Décision

- **Plus de JavaScript suivi par git.** Scripts, tests, configurations et côté JavaScript du moteur sont en TypeScript
  (`.ts`, `.tsx` pour l'app) ; `scripts/files.test.ts` échoue sur un fichier `.js`, `.mjs` ou `.cjs`, et sur `.mts` ou
  `.cts`, que les linters et la carte du code ne lisent pas. Seule la sortie d'Emscripten,
  `engine/build/wasm/lilpet.mjs`, reste en JavaScript : elle est construite, jamais suivie.
- **Node lance les fichiers tels quels**, sans build. `tsc` les vérifie dans `npm run check:types`, avec trois
  configurations : `tsconfig.json` pour ce que Node lance (`scripts/`, `engine/wasm/`, `.lintstagedrc.ts`, en
  `nodenext`), `app/tsconfig.app.json` pour l'app, `app/tsconfig.node.json` pour ses configurations et ses tests dans
  le navigateur ; `app/tsconfig.json` ne fait que les nommer, pour les éditeurs et pour Vite, qui le lisent d'abord.
- **`engine/wasm/engine.ts` porte ses propres types** : `engine.d.mts` disparaît, et avec lui le test qui le comparait
  au code.
- **Les types de Node (`@types/node`) suivent la version majeure de Node de `mise.toml`** : Renovate monte les deux,
  chacun à la date LTS d'une nouvelle version majeure, si bien qu'elles arrivent d'ordinaire dans la même pull request ;
  `scripts/mise.test.ts` échoue si elles diffèrent.
- **Les scripts Python restent en Python** : ils parlent aux cartes avec pyserial et esptool, l'outillage d'Espressif.
  En TypeScript, il leur faudrait un module natif (`serialport`), pour rien de plus.

## Conséquences

- Une annotation fausse fait échouer `npm run check`, et donc la CI, avant que le script ne tourne.
- Écrire un script demande de suivre les règles de la syntaxe effaçable (rappelées dans `CLAUDE.md`) ; `tsc` refuse
  l'essentiel de ce que Node ne saurait pas lancer : `enum`, `namespace`, un import relatif sans son extension.
- Le jour où Renovate propose une nouvelle version majeure de Node, `@types/node` monte d'ordinaire dans la même pull
  request (décision 8, complétée le 2026-11-23 : Renovate gère aussi npm) ; sinon le test la bloque jusqu'à son
  passage suivant.
