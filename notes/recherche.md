# Recherche : le Starboy

Recherche faite le 2026-10-27.

## Le produit

- Animal numérique en forme d'étoile, V2 du projet Lil Guy de Daniel Kuntz. Vendu par lilguy / CREATURE.
  Sources : [lilguy.net](https://lilguy.net/), [BGR](https://www.bgr.com/2291250/starboy-keychain-digital-pet-tamagotchi/)
- Prix : environ 140 à 600 $ selon la finition (aluminium, acier, laiton, cristal). Au lancement : 249 à 439 $.
  Sources : [BGR](https://www.bgr.com/2291250/starboy-keychain-digital-pet-tamagotchi/), [Gizmodo](https://gizmodo.com/can-we-interest-you-in-a-400-ai-keychain-that-behaves-like-a-real-pet-2000732919)
- Pas d'appli, pas de connexion au téléphone, pas de LLM. Mises à jour par USB.
  Sources : [Gizmodo (test)](https://gizmodo.com/starboy-ai-keychain-is-pointless-extravagant-and-weirdly-lovable-2000740949), [Sources](https://sources.news/p/starboy-anti-ai-wearable)
- Bluetooth prévu entre Starboy (échange d'yeux, de comportements). Ne marchait pas encore lors du test Gizmodo.

## Matériel connu

- Écran : OLED 400×400, verre avec traitement antireflet, 60 fps.
- Capteurs : caméra, micro, capteur de température, accéléromètre. Vibreur (haptique).
- Puce, batterie et dimensions : non publiées.
- La version « cristal » (transparente) montre un PCB jaune, l'écran rond au centre et la caméra en haut à gauche.
  Source : [lilguy.net](https://lilguy.net/)

## Logiciel et comportement

- Moteur maison « Lark » : plus de 500 animations faites par d'anciens animateurs Disney.
- Yeux : « plus de 5 000 looks » au lancement, « plus de 400 looks » sur le site actuel. Chaque look a une rareté et il est tiré au hasard.
- Reconnaissance par machine learning des visages et d'environ 30 gestes de la main.
- Réactions citées :
    - pouce levé : affiche la batterie ;
    - doigt d'honneur : il se met en colère ;
    - secoué : il a le tournis ;
    - froid prolongé : il grelotte ;
    - musique forte : il devient anxieux.
- Défaut constaté : il a souvent du mal à voir les visages et les gestes, avec un temps de retard.
  Source : [Gizmodo (test)](https://gizmodo.com/starboy-ai-keychain-is-pointless-extravagant-and-weirdly-lovable-2000740949)

## Projets open source utiles

- [pikoo](https://github.com/skiyo/pikoo) : animal sur ESP32-S3 avec deux yeux expressifs sur écrans ronds, et un simulateur sur PC.
- [Dual_Display_ESP32](https://github.com/intellar/Dual_Display_ESP32) : yeux réalistes sur deux écrans ronds GC9A01.
- [ESP32-CAM-EYES](https://github.com/Natali-ops/ESP32-CAM-EYES) : yeux qui suivent une source de lumière vue par la caméra.
- [esp32s3-gesture-recognition](https://github.com/DenisZheng/esp32s3-gesture-recognition) : reconnaissance de 10 gestes sur ESP32-S3.
- [Workshop Edge-AI ESP32-S3 d'Espressif](https://developer.espressif.com/workshops/edge-ai-with-esp32-s3/assignment-5/) : détection de la main, puis classification en 10 gestes, directement sur la puce.
