# 5. Un protocole en lignes de texte, le même sur l'USB et l'UART

- Date : 2026-11-01
- Statut : accepté

## Contexte

Trois sources parlent à l'objet : l'app PC par USB, la future carte vision par UART, et celui qui essaie une commande au
moniteur série. Le port USB porte aussi le journal et les mesures du firmware.

## Décision

Des lignes de texte : les commandes commencent par `>`, les réponses par `<`, et toute autre ligne est du journal. Les
mêmes lignes passent sur l'USB et sur l'UART, lues par le même analyseur du moteur. Le détail est dans
[protocole.md](../protocole.md).

Du texte simple : lisible dans le moniteur série, lu en C sans bibliothèque, facile à tester des deux côtés.

## Conséquences

- Le journal et les commandes partagent le port : leur premier caractère suffit à les distinguer.
