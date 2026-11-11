# 9. Le rendu sur les deux cœurs de la puce, depuis la mémoire interne

- Date : 2026-11-08
- Statut : accepté

## Contexte

Sur un seul cœur, l'ESP32-S3 dessine l'écran de 466 px à environ 30 images/s, loin des 60 visées. La puce a deux
cœurs, qui partagent un cache d'instructions de 16 Ko, ou de 32 Ko au prix de 16 Ko de mémoire
([réglage d'ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/kconfig-reference.html#config-esp32s3-instruction-cache-size)).

## Décision

- **Les deux cœurs se partagent chaque bande, une ligne sur deux** : le cœur 0 dessine les lignes paires, une tâche
  épinglée sur le cœur 1 les impaires, donc chacun a la moitié de chaque forme. Jusqu'au 11 novembre 2026, le cœur 0
  dessinait le haut de la bande et le cœur 1 le bas, deux moitiés de coût inégal : la bande attendait la plus lente
  (1,87 fois la vitesse d'un seul cœur, contre 1,98 une ligne sur deux, en simulation d'après les temps par ligne). Les
  deux lisent la même image, préparée une fois (`lp_render_begin`), et chacun compose ses lignes dans son propre
  brouillon (`lp_row_scratch`, 12 Ko ; `lp_render_rows_with` pour le second cœur), au lieu d'un second moteur de rendu
  entier (31 Ko).
- Deux sémaphores passent ses lignes au second cœur et les reprennent. Tout est statique, donc rien ne peut échouer au
  démarrage, et une carte qui mettrait `CONFIG_FREERTOS_UNICORE` ne compile pas.
- **Une seule bande part à la fois**, donc l'envoi reste caché derrière le dessin de la suivante (une bande sur deux par
  cœur attendait l'envoi de l'autre : 37 images/s au lieu de 44).
- **Le code du rendu (`render.c`, `raster.c`) tourne depuis la mémoire interne** (IRAM,
  `firmware/components/lilpet/linker.lf`) : depuis la flash, les deux cœurs se disputaient le cache d'instructions.

## Conséquences

- Sur la M5Stamp S3A, à 466 px, le dessin passe de 30,7 à 51-52 images/s ([mesures](../firmware.md#sur-la-puce)).
- Un test du moteur vérifie que deux brouillons qui se partagent les lignes donnent la même image.
- Un cache d'instructions de 32 Ko n'apporte que 1,5 % de plus : écarté.
