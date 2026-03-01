# PaperCamera AI 📷✨

Eine Sofortbildkamera basierend auf dem ESP32-S3, die Fotos nicht ausdruckt, sondern sie vorher von einer KI (Mistral) analysieren lässt.
Das Projekt ist im Rahmen des Unterrichts entstanden und wurde hier dokumentiert: http://www.tinkeredu.de/informatik/ki-und-klimaschutz-widerspruch-oder-werkzeug/

![Status](https://img.shields.io/badge/Status-Stable-green) [![License: CC BY-NC-SA 4.0](https://img.shields.io/badge/License-CC%20BY--NC--SA%204.0-lightgrey.svg)](https://creativecommons.org/licenses/by-nc-sa/4.0/)

## Funktionen
* **Snapshot & AI:** Macht ein Foto und sendet es an Mistral.
* **Thermal Print:** Druckt die Beschreibung, einen Witz oder ein Gedicht über das Bild aus.
* **Smart Feedback:** Eine farbige LED zeigt genau an, was die Kamera gerade macht (oder welcher Fehler vorliegt).
* **SD-Card Config:** WLAN und API-Keys werden sicher auf der SD-Karte gespeichert, nicht im Code.
* **Webinterface:** Einstellungen, Logs und Firmware-Updates direkt im Browser.
* **Geführter Modus:** Der Drucker gibt Hilfestellungen beim Setup und Datenschutzhinweise.
* **Sicherheit:** Passwortschutz für kritische Funktionen.

Inspiriert von der Initiative **[di.day](https://di.day/)**, vollzieht NeuroSnap mit Version 2 einen wichtigen Schritt in Richtung digitaler Souveränität und KI-Kompetenz. 

Anstatt Daten blind an geschlossene Systeme zu senden, macht dieses Update den Datenfluss transparent und kontrollierbar. Durch die Implementierung eines eigenen "Dropoff"-Servers und die Integration offenerer Modelle (wie Mistral AI) lernen Nutzer nicht nur, wie KI "sieht", sondern auch, wie Daten verarbeitet werden.


## Hardware-Liste
1. **ESP32-S3 Board:** z.B. https://de.aliexpress.com/item/1005008589091526.html?spm=a2g0o.order_list.order_list_main.63.5aa75c5fESzsB8&gatewayAdapt=glo2deu 
2. **Kamera:** OV2640 im Kit enthalten (es bietet sich an, eine Kamera mit langem Kabel zu nutzen, um die Verkabelung zu vereinfachen)
3. **Thermodrucker:** TTL Serial Printer z.B. https://de.aliexpress.com/item/1005007402905573.html?spm=a2g0o.order_list.order_list_main.125.5aa75c5fESzsB8&gatewayAdapt=glo2deu 
4. **MicroSD Karte:** Max 16GB empfohlen (FAT32) Ich habe eine Verlängerung genutzt z.B. https://de.aliexpress.com/item/4001200431510.html?spm=a2g0o.order_list.order_list_main.83.5aa75c5fESzsB8&gatewayAdapt=glo2deu
5. **Button & NeoPixel:** Ein Taster und eine WS2812B LED z.B. https://www.alibaba.com/product-detail/16mm-Stainless-Steel-Waterproof-Wired-Ws2812_1601446043870.html?spm=a2756.order-detail-ta-bn-b.0.0.3e61f19cDmsLW4

Ich habe ein PCB erstellt, um den Button und den Drucker leichter verbinden zu können.


## Installation

### 1. Webserver (PHP)
Lade `upload.php` und `dropoff.php` auf deinen Webserver (Ordner z.B. `/neurosnap/`). Stelle sicher, dass der Ordner Schreibrechte hat (`chmod 755`).

### 2. Mistral API Key besorgen
console.mistral.ai
Achtung: Im kostenlosen Modus werden die hochgeladenen Bilder und prompts für das Training neuer KIs verwendet. Nutzt man die Bezahlversion ist das nicht der Fall. Weitere Informationen dazu wie man das abstellt: https://help.mistral.ai/en/articles/455207-can-i-opt-out-of-my-input-or-output-data-being-used-for-training und https://help.mistral.ai/en/articles/347617-do-you-use-my-user-data-to-train-your-artificial-intelligence-models

### 3. SD-Karte (Konfiguration)
Lade die Dateien aus dem Ordner SD Karte in das Hauptverzeichnis deiner SD Karte. 
Die Funktionen sind die folgenden:
`config.txt` 
```text
DEIN_MISTRAL_API_KEY
https://api.mistral.ai/v1/chat/completions
pixtral-12b-2409
dein-server.de
/neurosnap/upload.php
0 (0=Aus, 1=An für Logging)
/neurosnap/dropoff.php
0
```
*(Zeile 6 & 8: 0=Aus, 1=An für Logging bzw. Geführten Modus)*

`password.txt` (Admin-Passwort) und `prompt.txt` (KI-Befehl).

### 4. Hotspot einrichten
Die Kamera benötigt eine Internetverbindung. Ein Hotspot auf dem Smartphone reicht vollkommen aus. 

### 5. Geh raus in die Welt und mache Fotos. Die Fotos werden auch auf der SD Karte gespeichert.

## 📖 Bedienung

*   **Kurz drücken:** Foto aufnehmen & analysieren.
*   **Mittel drücken (5s):** Upload aller Bilder zur Galerie.
*   **Lang drücken (10s):** WLAN Reset.

## Troubeshooting
Der Drucker benötigt viel Strom, sodass die Versorgung über eine Powerbank (abhängig von der Powerbank) nicht funktioniert. Ich habe eine 18650 Lipo mit einem entsprechenden Ladeboard das 2A bereitstellen kann genutzt.


## 📄 Lizenz

Dieses Werk ist lizenziert unter einer **Creative Commons Namensnennung - Nicht-kommerziell - Weitergabe unter gleichen Bedingungen 4.0 International Lizenz** (CC BY-NC-SA 4.0).