# 5. Un protocole en lignes de texte, le même sur l'USB et l'UART

- Date : 2026-11-01
- Statut : accepté

## Contexte

Trois sources parlent à l'objet : l'app PC par USB, la future carte caméra par UART, et celui qui essaie une commande au
moniteur série. Le port USB porte aussi le journal et les mesures du firmware.

## Décision

Des lignes de texte : les commandes commencent par `>`, les réponses par `<`, et toute autre ligne est du journal. Les
mêmes lignes passent sur l'USB et sur l'UART, lues par le même analyseur du moteur (`lp_command_parse`). Le détail est
dans [protocole.md](../protocole.md).

Du texte simple : lisible dans le moniteur série, lu en C sans bibliothèque, facile à tester des deux côtés.

## Conséquences

- Le journal et les commandes partagent le port sans se gêner : le firmware ignore toute ligne qui ne commence pas par
  `> `, l'app toute ligne qui ne commence pas par `< `.
- L'aperçu web, l'AtomS3R et l'objet final jouent les mêmes réactions, quelle que soit la source.
- Le protocole n'a pas d'identifiants : l'app associe une réponse à une demande par ordre d'arrivée.
