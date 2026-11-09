# 7. Deux cartes reliées en UART : le modèle décide, le moteur exécute

- Date : 2026-10-27
- Statut : accepté

## Contexte

L'objet doit réagir à ce que voit une caméra : le visage, les gestes de la main, les expressions de la personne. Aucune
carte trouvée ne combine un écran rond AMOLED et une caméra (recherche d'octobre 2026 chez Waveshare, Spotpear, Seeed,
LilyGO, M5Stack, Viewe et Tailor Pixels), et la carte écran n'a pas assez de broches libres pour une caméra (il en faut
une douzaine).

## Décision

- **Deux cartes** : la carte écran dessine les yeux et porte la batterie et le bouton ; la carte vision (Seeed XIAO
  ESP32S3 Sense) porte la caméra et un modèle d'IA qui reconnaît le visage, les gestes et les expressions. Chaque puce
  garde toute sa puissance pour une seule tâche. Le matériel est détaillé dans [materiel.md](../materiel.md).
- **La carte vision envoie ce qu'elle voit par UART**, avec les lignes du protocole
  ([décision 5](0005-protocole-texte.md)) : une réaction par son nom (`> react gesture_like`), dont
  `eyes/emotions.json` donne les valeurs, ou une émotion et sa durée (`> emotion happy 2`), et où regarder
  (`> face 0.2 -0.1`).
- **Le modèle décide, le moteur exécute.** La carte vision a son propre projet, `vision/`, créé à l'achat de la XIAO :
  elle n'a pas besoin du moteur, et ses broches de caméra sont celles de l'écran sur l'autre carte.

## Conséquences

- Changer de modèle ne touche pas au moteur, et l'aperçu web, l'AtomS3R et l'objet final jouent les mêmes réactions.
- Le moteur protège l'écran d'un modèle bavard : les étincelles partent au plus toutes les 0,5 s, et un visage sans
  nouvelle position est lâché.
- Le firmware de la carte écran aura une seconde tâche pour l'UART, qui répondra sur son propre flux.
