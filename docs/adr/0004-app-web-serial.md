# 4. App PC : une page web qui parle à l'objet par Web Serial

- Date : 2026-10-27, précisé le 2026-11-01
- Statut : accepté

## Contexte

L'app PC sert à choisir les yeux, et plus tard à les éditer et à leur ajouter des animations, puis à les envoyer à
l'objet par USB. Elle doit montrer les yeux tels que l'objet les dessine.

## Décision

- **Une page web**, ouverte dans Chrome ou Edge sur ordinateur : rien à installer, installable comme une app (PWA) plus
  tard. Elle fait tourner le moteur WebAssembly ([décision 1](0001-moteur-c-partage.md)) et servira d'éditeur.
- **React + TypeScript strict + Vite.** Vitest pour la logique, Playwright pour l'app compilée.
- **Communication avec l'objet par Web Serial.** Marche sous Chrome et Edge, pas sous Safari, Firefox incertain
  ([MDN](https://developer.mozilla.org/en-US/docs/Web/API/Web_Serial_API)). Espressif s'en sert déjà pour flasher
  depuis le navigateur ([esptool-js](https://github.com/espressif/esptool-js)).
- Les looks et les animations restent des **données** (JSON côté PC), envoyées à l'objet.

## Conséquences

- Seuls `app/src/ui` et `main.tsx` dépendent de React : le moteur, les données et la connexion se
  testent sans lui et resserviront ailleurs (éditeur d'animations, outils). Une règle d'oxlint le vérifie.
- La spec de l'app est dans [app.md](../app.md), le protocole dans [protocole.md](../protocole.md).
