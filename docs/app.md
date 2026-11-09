# App PC

La page web qui choisit les yeux et les envoie à l'objet ([décision 4](adr/0004-app-web-serial.md)), au protocole de
[protocole.md](protocole.md).

État : version 1 écrite le 2026-11-01 (galerie, aperçu, envoi), testée dans le navigateur, en CI et sur l'AtomS3R. Les
commandes `emotion`, `react` et `face` répondent sur l'AtomS3R depuis le 6 novembre 2026.

## Choix

- **Style soigné**, avec une identité propre à lilpet : ludique, fond sombre, l'écran rond au centre. Pas de copie du
  site officiel.
- Hébergement possible sur GitHub Pages, à décider au moment de publier : la page serait publique.

## Version 1

1. **Galerie et aperçu**
    - Aperçu animé par le vrai moteur (WebAssembly) dans l'écran rond de 466 × 466 : le regard suit la souris, et des
      boutons d'émotion et de clignement font vivre le look.
    - Les 100 couleurs en pastilles, avec une recherche par nom et des filtres (teinte, pupilles différentes à gauche
      et à droite).
    - Les 4 types de pupille.
    - Des favoris, gardés dans le navigateur.
2. **Envoi à l'objet**
    - Bouton « Brancher lilpet » (Web Serial), puis les infos de l'objet : carte, versions du firmware et du moteur.
    - Bouton « Envoyer à lilpet » : l'objet affiche le look tout de suite et le garde en mémoire flash.
    - Une humeur choisie (`> emotion`) ou une caresse (`> react tap`) passent aussi sur l'objet, sans rien garder. La
      dernière humeur repart après `hello`. Après un rechargement de la page, l'app montre Neutre, et l'objet garde la
      sienne : il faudrait une commande `emotion?` pour la lire.
    - Journal de l'objet (les mesures qu'il écrit déjà) dans un panneau repliable.

Version 1 testée sur l'AtomS3R (voir [materiel.md](materiel.md)) : le look envoyé est gardé (`> look?`,
`firmware:check-device`), et l'AtomS3R l'affiche sur son petit écran. Le moteur sait aussi dessiner en petit
(`lp_renderer_set_size`), de quoi faire plus tard des vignettes rendues par le vrai moteur dans la galerie.

Hors version 1, dans l'ordre probable :

- looks perso (couleurs libres, forme des yeux) ;
- réglages de l'objet (luminosité, mise en veille, suivi du regard) ;
- sauvegarde et import des réglages ;
- PWA hors ligne ;
- éditeur d'animations ;
- mode marionnette (piloter les yeux en direct) ;
- mise à jour du firmware depuis le navigateur.
