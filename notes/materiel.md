# Matériel

Statut : rien n'est commandé. Architecture proposée, pas encore validée.

## Architecture proposée : deux cartes reliées en UART

Aucune carte grand public ne combine un écran rond AMOLED et une caméra. On sépare donc les rôles.

1. **Carte écran** : rendu des yeux et animations, accéléromètre, batterie.
2. **Carte vision** : caméra, détection du visage et des gestes. Elle envoie des événements à la carte écran (`face_seen`, `thumbs_up`…).

Avantage : chaque puce garde toute sa puissance pour une seule tâche.

## Candidats

### Carte écran (Waveshare ESP32-S3 AMOLED, 466×466)

| Carte                      | Points clés                                                                    | Lien                                                                      |
| -------------------------- | ------------------------------------------------------------------------------ | ------------------------------------------------------------------------- |
| ESP32-S3-Touch-AMOLED-1.43 | IMU QMI8658, connecteur batterie, RTC, carte SD. **Favori pour un porte-clé.** | [waveshare.com](https://www.waveshare.com/esp32-s3-touch-amoled-1.43.htm) |
| ESP32-S3-Touch-AMOLED-1.32 | Micro, gestion d'alimentation, 8 Mo PSRAM                                      | [waveshare.com](https://www.waveshare.com/esp32-s3-touch-amoled-1.32.htm) |
| ESP32-S3-Touch-AMOLED-1.75 | Deux micros, IMU, AXP2101. Plus grand.                                         | [waveshare.com](https://www.waveshare.com/esp32-s3-touch-amoled-1.75.htm) |

Prix à vérifier.

### Carte vision

- **Seeed XIAO ESP32S3 Sense** : 13,99 $, 21×17,5×15 mm. ESP32-S3 avec 8 Mo PSRAM, caméra OV2640 (OV5640 compatible), micro, BLE 5.
  Consommation de la caméra : environ 138 mA en continu, donc à mettre en veille intelligemment.
  Sources : [Seeed](https://www.seeedstudio.com/XIAO-ESP32S3-Sense-p-5639.html), [CNX Software](https://www.cnx-software.com/2023/03/29/xiao-esp32s3-sense-camera-and-a-microphone-round-touchscreen-display/)

### Plan B : une seule carte, écran moins beau

- XIAO ESP32S3 Sense + Round Display for XIAO (LCD tactile 1,28", 240×240, 18 $).

### À ajouter

- Petite LiPo
- Vibreur
- Capteur de température (ou celui de l'IMU)
- Boîtier imprimé en 3D

## Logiciel vision

- Modèles Espressif `espressif/hand_detect` et `espressif/hand_gesture_recognition` (ESP-DL).
  10 gestes : one, two, three, four, five, like, ok, no_gesture, call, dislike.
- Le doigt d'honneur n'en fait pas partie : il faudra entraîner un modèle perso si on le veut.
