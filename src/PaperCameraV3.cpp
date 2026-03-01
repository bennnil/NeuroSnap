#include <Arduino.h>
/*
 * Project: PaperCamera AI (ESP32-S3) - Webinterface Edition
 * Features:
 * - Captive Portal for WiFi Setup
 * - Camera, Thermal Printer & Mistral AI
 * - Webinterface (Port 80) for Gallery, Download, Delete, and Prompt-Edit
 * - V3.1: Prompt saved in text file, Security features, Cleanup logic.
 * License: CC BY-NC-SA 4.0
 */

#include "esp_camera.h"
#include "WiFi.h"
#include "SD_MMC.h"
#include "FS.h"
#include "Adafruit_Thermal.h"
#include "HTTPClient.h"
#include "ArduinoJson.h"
#include <Adafruit_NeoPixel.h>
#include <DNSServer.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <WiFiClientSecure.h>
#include <Update.h>

// ==========================================
// PROTOTYPES
// ==========================================
void setStatusLED(uint8_t r, uint8_t g, uint8_t b);
void configModeCallback(WiFiManager *myWiFiManager);
void sysLog(String msg);
String replace_all(String text);
String getValue(String data, char separator, int index);
void drucke(const char* text);
void saveTextForImage(String text, String usedPrompt);
void savePhotoToSD(camera_fb_t * fb);
void uploadAndAnalyze(camera_fb_t *fb);
void processImage();
void uploadFile(String imagePath, String textPath);
void uploadAllFilesToServer();

// ==========================================
// CONFIGURATION
// ==========================================

#define BUTTON_PIN      1
#define PRINTER_RX_PIN  2
#define PRINTER_TX_PIN  3
#define LED_PIN         21
#define NUM_LEDS        1
#define ZEICHEN_PRO_ZEILE 32

// Camera Pins (ESP32-S3 EYE)
#define PWDN_GPIO_NUM -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 15
#define SIOD_GPIO_NUM 4
#define SIOC_GPIO_NUM 5
#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16
#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM 7
#define PCLK_GPIO_NUM 13

// ==========================================
// GLOBAL OBJECTS
// ==========================================

String API_KEY   = "";
String API_URL   = "";
String API_MODEL = "";
String UPLOAD_HOST = "example.com";
String UPLOAD_PATH = "/neurosnap/upload.php";
String promptFromFile = "";
String adminPassword = "admin"; // Default password
String lastImageFileName = "";
bool loggingEnabled = false;

HardwareSerial mySerial(2);
Adafruit_Thermal printer(&mySerial);
Adafruit_NeoPixel pixels(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);
AsyncWebServer server(80);

// Status variable
bool isBusy = false;

// ==========================================
// WEB INTERFACE (HTML/JS)
// ==========================================
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>NeuroSnap Gallery</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
    h1, h2 { text-align: center; color: #bb86fc; }
    .container { max-width: 800px; margin: auto; }
    .card { background-color: #1e1e1e; padding: 20px; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    input[type=text] { width: 100%; padding: 10px; margin: 10px 0; border-radius: 5px; border: 1px solid #333; background: #2c2c2c; color: white; }
    button { background-color: #bb86fc; color: black; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; }
    button:hover { background-color: #9955e8; }
    button.del { background-color: #cf6679; margin-left: 5px; }
    button.down { background-color: #03dac6; }
    .gallery { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 20px; }
    .photo-card { background: #1e1e1e; border-radius: 8px; overflow: hidden; border: 1px solid #333; }
    .photo-card img { width: 100%; height: auto; display: block; }
    .content { padding: 15px; }
    .ai-text { font-style: italic; color: #aaa; font-size: 0.9em; background: #252525; padding: 10px; border-radius: 5px; margin-bottom: 10px; white-space: pre-wrap; }
    .actions { display: flex; justify-content: space-between; }
    ul { padding-left: 20px; }
    ul li { margin-bottom: 5px; }
    label { display: block; margin-top: 10px; font-weight: bold; color: #bb86fc; }
  </style>
</head>
<body>
  <div class="container">
    <h1>NeuroSnap</h1>
    <div class="card">
      <h2>KI Prompt</h2>
      <input type="text" id="promptInput" placeholder="Lade Prompt...">
      <button onclick="savePrompt()">Speichern</button>
    </div>
    <div class="card">
      <h2>Galerie</h2>
      <div id="gallery" class="gallery">Loading...</div>
    </div>
    <div class="card">
      <h2>Anleitung</h2>
      <p><strong><u>Geräte-Taste:</u></strong></p>
      <ul>
        <li><strong>Kurz (&lt;2s):</strong> Foto aufnehmen & analysieren.</li>
        <li><strong>Mittel (5-10s):</strong> Alle Bilder zum Cloud-Server hochladen.</li>
        <li><strong>Lang (&gt;10s):</strong> WLAN-Einstellungen zurücksetzen.</li>
      </ul>
      <p><strong><u>LED-Status:</u></strong></p>
      <ul>
        <li><span style="color:#0000FF;">&#9632;</span> Blau: Bereit</li>
        <li><span style="color:#FFFF00;">&#9632;</span> Gelb: Foto wird verarbeitet</li>
        <li><span style="color:#00FF00;">&#9632;</span> Grün: KI-Analyse erfolgreich</li>
        <li><span style="color:#FF4500;">&#9632;</span> Orange: Fehler bei KI/API</li>
        <li><span style="color:#A020F0;">&#9632;</span> Lila: Cloud-Upload läuft</li>
        <li><span style="color:#00FFFF;">&#9632;</span> Cyan: WLAN-Setup Modus aktiv</li>
        <li><span style="color:#FF0000;">&#9632;</span> Rot: Systemfehler (z.B. SD-Karte)</li>
      </ul>
      <p><strong><u>Web-Funktionen:</u></strong></p>
      <p>Hier können Sie den KI-Prompt anpassen, Bilder ansehen, herunterladen, löschen und die Gerätesoftware (Firmware) aktualisieren.</p>
    </div>
    <div class="card">
      <h2>System</h2>
      <a href="/firmwareupdate"><button>Firmware Update</button></a>
      <button onclick="showSettings()">Einstellungen</button>
    </div>
    <div id="settingsCard" class="card" style="display:none;">
      <h2>Einstellungen</h2>
      <label>Mistral AI API Key:</label>
      <input type="text" id="apiKeyInput" placeholder="API Key">
      <label>API Basis-URL (Mistral):</label>
      <input type="text" id="apiUrlInput" placeholder="API URL">
      <label>KI-Modell Name (z.B. pixtral-12b-2409):</label>
      <input type="text" id="apiModelInput" placeholder="API Model">
      <label>Upload Server Host (ohne https://):</label>
      <input type="text" id="uploadHostInput" placeholder="Upload Host (z.B. example.com)">
      <label>Pfad zum PHP-Script auf dem Server:</label>
      <input type="text" id="uploadPathInput" placeholder="Upload Path (z.B. /neurosnap/upload.php)">
      <label style="display:flex; align-items:center; margin-top:10px;"><input type="checkbox" id="loggingInput" style="width:auto; margin-right:10px;"> Logging auf SD-Karte aktivieren</label>
      <button onclick="saveSettings()">Speichern</button>
      <hr>
      <h3>System Log</h3>
      <textarea id="logOutput" rows="10" style="background:#333; color:#0f0; font-family:monospace; font-size:12px;" readonly></textarea>
      <button onclick="fetchLog()">Log aktualisieren</button>
      <button onclick="deleteLog()" style="background:#cf6679; margin-top:5px;">Log Datei löschen</button>
    </div>
  </div>
<script>
  fetch('/getPrompt').then(r => r.text()).then(t => document.getElementById('promptInput').value = t);
  
  function savePrompt() {
    const val = document.getElementById('promptInput').value;
    fetch('/setPrompt?val=' + encodeURIComponent(val), {method: 'POST'})
      .then(r => alert('Prompt gespeichert!'));
  }

  function loadGallery() {
    fetch('/list').then(response => response.json()).then(files => {
      const g = document.getElementById('gallery');
      g.innerHTML = '';
      files.sort((a, b) => b.name.localeCompare(a.name, undefined, {numeric: true, sensitivity: 'base'}));
      files.forEach(file => {
        if(!file.name.endsWith('.jpg')) return; 
        const name = file.name;
        const txtName = name.replace('.jpg', '.txt');
        let html = `
          <div class="photo-card" id="card-${name.replace(/\W/g,'')}">
            <img src="/sd${name}" loading="lazy">
            <div class="content">
              <div class="ai-text" id="text-${name.replace(/\W/g,'')}">Lade Text...</div>
              <div class="actions">
                <a href="/sd${name}" download><button class="down">Download</button></a>
                <button class="del" onclick="delFile('${name}')">Löschen</button>
              </div>
            </div>
          </div>`;
        g.innerHTML += html;
        
        // Text laden
        fetch('/sd' + txtName).then(r => {
            if(r.ok) return r.text();
            throw new Error('No text');
        }).then(t => {
            // Nur die Antwort anzeigen (optional: split bei PROMPT:)
            document.getElementById('text-' + name.replace(/\W/g,'')).innerText = t;
        }).catch(() => {
            document.getElementById('text-' + name.replace(/\W/g,'')).innerText = "(Kein Text)";
        });
      });
    });
  }

  function delFile(name) {
    if(!confirm('Bild wirklich löschen?')) return;
    
    // Passwort Abfrage
    let pass = prompt("Bitte Passwort eingeben zum Löschen:");
    if(pass == null || pass == "") return;

    fetch('/delete?name=' + encodeURIComponent(name) + '&pass=' + encodeURIComponent(pass), {method: 'POST'})
    .then(r => {
        if(r.status === 200) {
            // ID Sanitisierung für Selektor
            const safeId = name.replace(/\W/g,'');
            const el = document.getElementById('card-' + safeId);
            if(el) el.remove();
        } else if (r.status === 401) {
            alert("Falsches Passwort!");
        } else {
            alert("Fehler beim Löschen.");
        }
    });
  }

  function showSettings() {
    let pass = prompt("Bitte Passwort eingeben:");
    if(pass == null || pass == "") return;

    fetch('/getSettings?pass=' + encodeURIComponent(pass))
      .then(r => {
        if(r.status === 401) {
          alert("Falsches Passwort!");
          return;
        }
        if(r.ok) {
          return r.json();
        }
      })
      .then(data => {
        if(data) {
          document.getElementById('apiKeyInput').value = data.api_key;
          document.getElementById('apiUrlInput').value = data.api_url;
          document.getElementById('apiModelInput').value = data.api_model;
          document.getElementById('uploadHostInput').value = data.upload_host;
          document.getElementById('uploadPathInput').value = data.upload_path;
          document.getElementById('loggingInput').checked = data.logging_enabled;
          document.getElementById('settingsCard').style.display = 'block';
          fetchLog();
        }
      });
  }

  function saveSettings() {
    let pass = prompt("Bitte Passwort eingeben:");
    if(pass == null || pass == "") return;
    
    const apiKey = document.getElementById('apiKeyInput').value;
    const apiUrl = document.getElementById('apiUrlInput').value;
    const apiModel = document.getElementById('apiModelInput').value;
    const uploadHost = document.getElementById('uploadHostInput').value;
    const uploadPath = document.getElementById('uploadPathInput').value;
    const logging = document.getElementById('loggingInput').checked ? '1' : '0';

    fetch('/setSettings?pass=' + encodeURIComponent(pass) + 
          '&api_key=' + encodeURIComponent(apiKey) + 
          '&api_url=' + encodeURIComponent(apiUrl) + 
          '&api_model=' + encodeURIComponent(apiModel) +
          '&upload_host=' + encodeURIComponent(uploadHost) +
          '&upload_path=' + encodeURIComponent(uploadPath) +
          '&logging=' + encodeURIComponent(logging), {method: 'POST'})
      .then(r => {
        if(r.status === 401) {
          alert("Falsches Passwort!");
          return;
        }
        if(r.ok) {
          alert('Einstellungen gespeichert!');
          document.getElementById('settingsCard').style.display = 'none';
        } else {
          alert('Fehler beim Speichern.');
        }
      });
  }

  function fetchLog() {
    fetch('/getLog?t=' + Date.now()).then(r => r.text()).then(t => {
        const log = document.getElementById('logOutput');
        log.value = t;
        log.scrollTop = log.scrollHeight;
    });
  }

  function deleteLog() {
    if(!confirm('Log Datei wirklich löschen?')) return;
    let pass = prompt("Bitte Passwort eingeben:");
    if(pass == null || pass == "") return;
    
    fetch('/deleteLog?pass=' + encodeURIComponent(pass), {method: 'POST'})
      .then(r => {
         if(r.ok) { alert('Log gelöscht'); fetchLog(); }
         else alert('Fehler');
      });
  }

  loadGallery();
</script>
</body>
</html>
)rawliteral";

// ==========================================
// UPDATE PAGE (HTML)
// ==========================================
const char update_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta charset="UTF-8">
  <title>NeuroSnap Firmware Update</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: 'Segoe UI', sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
    h1 { text-align: center; color: #bb86fc; }
    .container { max-width: 500px; margin: auto; }
    .card { background-color: #1e1e1e; padding: 20px; border-radius: 12px; margin-bottom: 20px; box-shadow: 0 4px 6px rgba(0,0,0,0.3); }
    input { width: 100%; padding: 10px; margin: 10px 0; border-radius: 5px; border: 1px solid #333; background: #2c2c2c; color: white; box-sizing: border-box; }
    button { background-color: #bb86fc; color: black; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; width:100%;}
    button:hover { background-color: #9955e8; }
    .footer { text-align:center; margin-top: 20px; }
    .footer a { color: #03dac6; }
    #prg { margin-top: 10px; text-align: center; }
  </style>
</head>
<body>
  <div class="container">
    <h1>NeuroSnap Firmware Update</h1>
    <div class="card">
      <form method='POST' action='/update' enctype='multipart/form-data' id='upload_form'>
        <input type='password' name='pass' placeholder='Passwort' required>
        <input type='file' name='update' accept='.bin' required>
        <button type='submit'>Firmware aktualisieren</button>
      </form>
      <div id='prg' style='display:none;'>Update läuft... Das Gerät startet danach neu.</div>
    </div>
     <div class='footer'><a href='/'>Zurück zur Galerie</a></div>
  </div>
  <script>
    document.getElementById('upload_form').addEventListener('submit', function(e) {
      var password = this.querySelector("input[name='pass']").value;
      var fileInput = this.querySelector("input[name='update']");
      if(password.trim() === '' || fileInput.files.length === 0) {
        alert("Bitte Passwort und eine Datei auswählen.");
        e.preventDefault();
        return;
      }
      this.style.display = 'none';
      document.getElementById('prg').style.display = 'block';
    });
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// HELPER FUNCTIONS
// ==========================================

void setStatusLED(uint8_t r, uint8_t g, uint8_t b) {
  pixels.setPixelColor(0, pixels.Color(r, g, b));
  pixels.show();
}

void configModeCallback (WiFiManager *myWiFiManager) {
  setStatusLED(0, 255, 255); // CYAN: AP Mode
}

void sysLog(String msg) {
    Serial.println(msg);
    if(loggingEnabled) {
        File logFile = SD_MMC.open("/system.log", FILE_APPEND);
        if(logFile) {
            String timeStr = String(millis() / 1000) + "s: ";
            logFile.println(timeStr + msg);
            logFile.close();
        }
    }
}

// Map German umlauts to PC437
String replace_all(String text){
  text.replace("Ä", "Ae");
  text.replace("ä", "ae");
  text.replace("Ö", "Oe");
  text.replace("ö", "oe");
  text.replace("Ü", "Ue");
  text.replace("ü", "ue");
  text.replace("ß", "ss");
  return text;
}

String getValue(String data, char separator, int index) {
  int found = 0; int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;
  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1; strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }
  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void drucke(const char* text) {
    String originalText = String(text); String currentLine = "";
    int wordIndex = 0;
    String nextWord = "";
    do {
        nextWord = getValue(originalText, ' ', wordIndex++);
        if (nextWord.length() > 0) {
            if (currentLine.length() + nextWord.length() + 1 > ZEICHEN_PRO_ZEILE) {
                printer.println(replace_all(currentLine));
                currentLine = "";
            }
            currentLine += nextWord + " ";
        }
    } while (nextWord.length() > 0);
    if (currentLine.length() > 0) { printer.println(replace_all(currentLine)); }
    printer.feed(3);
}

void saveTextForImage(String text, String usedPrompt) {
    if (lastImageFileName == "") return;
    String txtPath = lastImageFileName;
    txtPath.replace(".jpg", ".txt");
    
    File file = SD_MMC.open(txtPath.c_str(), FILE_WRITE);
    if(file){
        file.print("PROMPT: ");
        file.println(usedPrompt);
        file.println("----------------");
        file.println("AI: ");
        file.print(text);
        file.close();
    }
}

// ==========================================
// CAMERA & UPLOAD LOGIC
// ==========================================

void savePhotoToSD(camera_fb_t * fb) {
  int n = 0;
  String path;
  do {
    path = "/bild_" + String(n++) + ".jpg";
  } while (SD_MMC.exists(path));
  
  File file = SD_MMC.open(path.c_str(), FILE_WRITE);
  if (file) {
      file.write(fb->buf, fb->len);
      file.close();
      lastImageFileName = path;
      sysLog("Bild gespeichert: " + path);
  } else {
      sysLog("Fehler: Bild speichern fehlgeschlagen!");
  }
}

void uploadAndAnalyze(camera_fb_t *fb) {
  if(WiFi.status() != WL_CONNECTED) {
      setStatusLED(0, 255, 255);
      drucke("Kein WLAN!");
      return;
  }

  sysLog("RAM vor Upload: " + String(ESP.getFreeHeap()));
  
  // --- 1. Upload to dropoff.php ---
  sysLog("Lade Bild zu Dropoff-Server...");
  
  WiFiClientSecure client;
  client.setInsecure(); // Required for HTTPS
  
  String dropoffUrl = "";
  String dropoffScript = UPLOAD_PATH;
  dropoffScript.replace("upload.php", "dropoff.php");
  
  if (client.connect(UPLOAD_HOST.c_str(), 443)) {
      String boundary = "------------------------" + String(millis());
      String head = "--" + boundary + "\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"capture.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
      String tail = "\r\n--" + boundary + "--\r\n";
      
      size_t totalLen = head.length() + fb->len + tail.length();
      
      client.println("POST " + dropoffScript + " HTTP/1.1");
      client.println("Host: " + UPLOAD_HOST);
      client.println("Content-Length: " + String(totalLen));
      client.println("Content-Type: multipart/form-data; boundary=" + boundary);
      client.println();
      client.print(head);
      
      // Send image data in chunks
      uint8_t *fbBuf = fb->buf;
      size_t fbLen = fb->len;
      size_t chunkSize = 1024;
      for (size_t i = 0; i < fbLen; i += chunkSize) {
          size_t toSend = (fbLen - i < chunkSize) ? (fbLen - i) : chunkSize;
          client.write(fbBuf + i, toSend);
      }
      
      client.print(tail);
      
      // Read response (Expects plain text URL)
      unsigned long timeout = millis();
      bool headersEnded = false;
      while (client.connected() && millis() - timeout < 10000) {
          if (client.available()) {
              String line = client.readStringUntil('\n');
              if (line == "\r") { headersEnded = true; continue; }
              if (headersEnded) {
                  dropoffUrl = line;
                  dropoffUrl.trim();
                  if (dropoffUrl.startsWith("http")) break; 
              }
          }
      }
      client.stop();
  } else {
      sysLog("Fehler: Verbindung zu Dropoff-Server fehlgeschlagen.");
      setStatusLED(255, 0, 0);
      return;
  }

  if (!dropoffUrl.startsWith("http")) {
      sysLog("Fehler: Keine gültige URL erhalten. Antwort: " + dropoffUrl);
      setStatusLED(255, 0, 0);
      return;
  }
  sysLog("Bild URL: " + dropoffUrl);

  // --- 2. Mistral API Request ---
  HTTPClient http;
  JsonDocument doc;
  doc["model"] = API_MODEL;
  doc["temperature"] = 0.7;
  
  JsonArray messages = doc["messages"].to<JsonArray>();
  JsonObject userMsg = messages.add<JsonObject>();
  userMsg["role"] = "user";
  
  JsonArray content = userMsg["content"].to<JsonArray>();
  
  JsonObject textPart = content.add<JsonObject>();
  textPart["type"] = "text";
  textPart["text"] = promptFromFile;
  
  JsonObject imagePart = content.add<JsonObject>();
  imagePart["type"] = "image_url";
  imagePart["image_url"]["url"] = dropoffUrl; // Use URL instead of Base64
  
  String payload;
  serializeJson(doc, payload);
  
  // Send Request
  sysLog("Sende Request an KI: " + API_URL);
  
  WiFiClientSecure clientMistral;
  clientMistral.setInsecure();
  http.begin(clientMistral, API_URL);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + API_KEY);
  http.setTimeout(60000); // Extended timeout for vision tasks
  
  int httpCode = http.POST(payload);
  
  if (httpCode == 200) {
    String response = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, response);
    
    if (!error && !doc["choices"].isNull()) {
      const char* text = doc["choices"][0]["message"]["content"];
      sysLog("Antwort erhalten: " + String(text).substring(0, 50) + "...");
      
      // Save prompt with image
      saveTextForImage(String(text), promptFromFile);

      setStatusLED(0, 255, 0);
      printer.boldOn(); printer.println("Mistral AI:"); printer.boldOff();
      printer.feed(1);
      drucke(text);
      
    } else {
      sysLog("Fehler: JSON Parse oder keine Choices.");
      sysLog("Response: " + response);
      setStatusLED(255, 69, 0); drucke("Fehler: KI-Format.");
    }
  } else {
    sysLog("API Error Response: " + http.getString());
    setStatusLED(255, 69, 0); drucke("Fehler: API.");
  }
  http.end();
}

void processImage() {
    if (isBusy) {
        sysLog("DEBUG: System is Busy, ignore button.");
        return;
    }
    
    isBusy = true; // Block new requests
    
   
    setStatusLED(255, 255, 0);

    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      sysLog("DEBUG: Camera Capture Failed");
      isBusy = false;
      return;
    }

    savePhotoToSD(fb);
    uploadAndAnalyze(fb);
    
    esp_camera_fb_return(fb);

    // Reset Status
    uint32_t c = pixels.getPixelColor(0);
    if (c != pixels.Color(255, 69, 0) && c != pixels.Color(255, 0, 0)) {
        setStatusLED(0, 0, 255);
    }
    
    isBusy = false;
    sysLog("DEBUG: Prozess beendet, System bereit.");
}


// ==========================================
// SYSTEM SETUP
// ==========================================
void setup() {
  Serial.begin(115200);
  sysLog("\n--- NeuroSnap V3.1 FIXED START ---");

  pixels.begin(); pixels.setBrightness(50);
  setStatusLED(255, 165, 0);
  
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  // Init SD
  SD_MMC.setPins(39, 38, 40);
  if (!SD_MMC.begin("/sdcard", true, true, SDMMC_FREQ_DEFAULT, 5)){
    sysLog("SD Mount Failed");
    setStatusLED(255, 0, 0); while(1);
  }
  
  // Load Config
  File configFile = SD_MMC.open("/config.txt");
  if (configFile) {
    API_KEY = configFile.readStringUntil('\n'); API_KEY.trim();
    API_URL = configFile.readStringUntil('\n'); API_URL.trim();
    API_MODEL = configFile.readStringUntil('\n'); API_MODEL.trim();
    UPLOAD_HOST = configFile.readStringUntil('\n'); UPLOAD_HOST.trim();
    UPLOAD_PATH = configFile.readStringUntil('\n'); UPLOAD_PATH.trim();
    String logStr = configFile.readStringUntil('\n'); logStr.trim();
    loggingEnabled = (logStr == "1");
    configFile.close();
    
    // Set defaults if empty
    if (API_URL.length() == 0) {
        API_URL = "https://api.mistral.ai/v1/chat/completions";
    }
    
    if (API_MODEL.length() == 0) API_MODEL = "pixtral-12b-2409";
    
    if (UPLOAD_HOST.length() == 0) UPLOAD_HOST = "example.com";
    if (UPLOAD_PATH.length() == 0) UPLOAD_PATH = "/neurosnap/upload.php";
  }

  // Load Password
  File passFile = SD_MMC.open("/password.txt");
  if(passFile){
    adminPassword = passFile.readString(); passFile.close(); adminPassword.trim();
  }
  // Fallback if file is empty or missing
  if(adminPassword.length() == 0) {
    adminPassword = "neurosnap";
    File f = SD_MMC.open("/password.txt", FILE_WRITE);
    if(f) { f.print("neurosnap"); f.close(); }
  }

  // Load Prompt
  File promptFile = SD_MMC.open("/prompt.txt");
  if(promptFile){
    promptFromFile = promptFile.readString(); promptFile.close(); promptFromFile.trim();
  } else {
    promptFromFile = "Beschreibe dieses Bild.";
  }

  // WiFi Setup
  WiFiManager wm;
  wm.setAPCallback(configModeCallback);
  if(!wm.autoConnect("NeuroSnap_Setup")) {
    ESP.restart();
  }
  
  setStatusLED(255, 165, 0);

  // ==========================
  // WEBSERVER
  // ==========================
  
  server.serveStatic("/sd", SD_MMC, "/");
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", index_html);
  });
  
  server.on("/list", HTTP_GET, [](AsyncWebServerRequest *request){
    if(isBusy) { request->send(503, "text/plain", "Camera busy"); return; }
    
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->print("[");
    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    bool first = true;
    while(file){
        if(!file.isDirectory()){
            if(!first) response->print(",");
            response->print("{\"name\":\"");
            response->print(String("/") + file.name());
            response->print("\"}");
            first = false;
        }
        file = root.openNextFile();
    }
    response->print("]");
    request->send(response);
  });

  server.on("/getPrompt", HTTP_GET, [](AsyncWebServerRequest *request){
     request->send(200, "text/plain", promptFromFile);
  });

  server.on("/setPrompt", HTTP_POST, [](AsyncWebServerRequest *request){
     if (request->hasParam("val")) {
        String newVal = request->getParam("val")->value();
        newVal.trim();
        if(newVal.length() > 0) {
            promptFromFile = newVal;
            File f = SD_MMC.open("/prompt.txt", FILE_WRITE);
            if(f) { f.print(newVal); f.close(); }
        }
        request->send(200, "text/plain", "OK");
     } else {
        request->send(400);
     }
  });

  server.on("/getLog", HTTP_GET, [](AsyncWebServerRequest *request){
     if(SD_MMC.exists("/system.log")) {
        request->send(SD_MMC, "/system.log", "text/plain");
     } else {
        request->send(200, "text/plain", "Kein Log vorhanden oder Logging deaktiviert.");
     }
  });

  // DELETE WITH PASSWORD CHECK
  server.on("/delete", HTTP_POST, [](AsyncWebServerRequest *request){
    if(isBusy) { request->send(503, "text/plain", "Camera busy"); return; }
    
    if (request->hasParam("name") && request->hasParam("pass")) {
        String filename = request->getParam("name")->value();
        String pass = request->getParam("pass")->value();
        
        // 1. Check Password
        if (pass != adminPassword) {
            request->send(401, "text/plain; charset=utf-8", "Falsches Passwort");
            return;
        }

        // 2. Delete
        if(filename.indexOf("bild_") >= 0) {
            SD_MMC.remove(filename);
            String txt = filename;
            txt.replace(".jpg", ".txt");
            SD_MMC.remove(txt); // Löscht Antwort + Prompt File
            request->send(200, "text/plain", "Deleted");
        } else {
             request->send(403, "text/plain", "Forbidden");
        }
    } else {
        request->send(400, "text/plain", "Missing params");
    }
  });

  server.on("/deleteLog", HTTP_POST, [](AsyncWebServerRequest *request){
      if (request->hasParam("pass") && request->getParam("pass")->value() == adminPassword) {
          SD_MMC.remove("/system.log");
          sysLog("Log Datei gelöscht.");
          request->send(200, "text/plain", "Deleted");
      } else {
          request->send(401, "text/plain", "Unauthorized");
      }
  });

  server.on("/getSettings", HTTP_GET, [](AsyncWebServerRequest *request){
    if (request->hasParam("pass") && request->getParam("pass")->value() == adminPassword) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        response->print("{\"api_key\":\"");
        response->print(API_KEY);
        response->print("\",\"api_url\":\"");
        response->print(API_URL);
        response->print("\",\"api_model\":\"");
        response->print(API_MODEL);
        response->print("\",\"upload_host\":\"");
        response->print(UPLOAD_HOST);
        response->print("\",\"upload_path\":\"");
        response->print(UPLOAD_PATH);
        response->print("\",\"logging_enabled\":");
        response->print(loggingEnabled ? "true" : "false");
        response->print("\"}");
        request->send(response);
    } else {
        request->send(401, "text/plain", "Unauthorized");
    }
  });

  server.on("/setSettings", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("pass") && request->getParam("pass")->value() == adminPassword) {
        if (request->hasParam("api_key") && request->hasParam("api_url") && request->hasParam("api_model") &&
            request->hasParam("upload_host") && request->hasParam("upload_path") && request->hasParam("logging")) {
            API_KEY = request->getParam("api_key")->value();
            API_URL = request->getParam("api_url")->value();
            API_MODEL = request->getParam("api_model")->value();
            UPLOAD_HOST = request->getParam("upload_host")->value();
            UPLOAD_PATH = request->getParam("upload_path")->value();
            loggingEnabled = (request->getParam("logging")->value() == "1");

            File configFile = SD_MMC.open("/config.txt", FILE_WRITE);
            if (configFile) {
                configFile.println(API_KEY);
                configFile.println(API_URL);
                configFile.println(API_MODEL);
                configFile.println(UPLOAD_HOST);
                configFile.println(UPLOAD_PATH);
                configFile.print(loggingEnabled ? "1" : "0");
                configFile.close();
                request->send(200, "text/plain", "OK");
            } else {
                request->send(500, "text/plain", "Failed to write to config file");
            }
        } else {
            request->send(400, "text/plain", "Missing params");
        }
    } else {
        request->send(401, "text/plain", "Unauthorized");
    }
  });

  // --- FIRMWARE UPDATE ROUTES ---
  server.on("/firmwareupdate", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", update_html);
  });

  server.on("/update", HTTP_POST, 
    [](AsyncWebServerRequest *request){
        // This is called when the upload is complete, but ONLY if the upload handler did not send a response.
        // Should not happen with the logic below, but as a fallback...
        if(Update.hasError()){
             request->send(500, "text/plain; charset=utf-8", "UPDATE FEHLGESCHLAGEN NACH UPLOAD");
        } else {
             AsyncWebServerResponse *response = request->beginResponse(200, "text/plain; charset=utf-8", "Update ERFOLGREICH! Das Gerät startet jetzt neu.");
             response->addHeader("Refresh", "10;url=/");
             request->send(response);
             delay(200);
             ESP.restart();
        }
    }, 
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final){
        // 1. On first chunk, check password and start update
        if(index == 0){
            // Authenticate
            if(!request->hasParam("pass", true) || request->getParam("pass", true)->value() != adminPassword) {
                // Abort with 401 Unauthorized
                return request->send(401, "text/plain; charset=utf-8", "Falsches Passwort!");
            }
            
            Serial.printf("Update Start: %s\n", filename.c_str());
            // Start update process
            if(!Update.begin(UPDATE_SIZE_UNKNOWN)){ // Start with max available size
                Update.printError(Serial);
                 // Abort with 500 Internal Server Error
                return request->send(500, "text/plain; charset=utf-8", "Konnte Update nicht starten.");
            }
        }

        // 2. If we have data, write it to flash
        if(len > 0){
            if(Update.write(data, len) != len){
                Update.printError(Serial);
                // Abort with 500
                return request->send(500, "text/plain; charset=utf-8", "Fehler beim Schreiben des Updates.");
            }
        }

        // 3. If it's the last packet, finish and restart
        if(final){
            if(Update.end(true)){ // true to set the size to the current progress
                Serial.printf("Update Success: %u bytes\n", index+len);
                // Don't send response here. Let the final handler do it.
            } else {
                Update.printError(Serial);
                 // Abort with 500
                return request->send(500, "text/plain; charset=utf-8", "Update fehlgeschlagen.");
            }
        }
    }
  );

  server.begin();
  sysLog("Webserver gestartet.");

  // Init Printer
  mySerial.begin(9600, SERIAL_8N1, PRINTER_RX_PIN, PRINTER_TX_PIN);
  printer.begin();
  printer.setCodePage(CODEPAGE_CP437);
  
  printer.println("IP Adresse:");
  printer.println(WiFi.localIP().toString().c_str());
  printer.feed(1);

  // Init Camera
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0; config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM; config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM; config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM; config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM; config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM; config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM; config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM; config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM; config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 10000000; config.pixel_format = PIXFORMAT_JPEG;
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_QHD;
    config.jpeg_quality = 10;
    config.fb_count = 2; config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12; config.fb_count = 1;
  }
  esp_camera_init(&config);
  
  sensor_t * s = esp_camera_sensor_get();
  s->set_vflip(s, 0); s->set_brightness(s, 1); s->set_saturation(s, 0);

  sysLog("System Bereit.");
  setStatusLED(0, 0, 255);
}

// ==========================================
// SERVER UPLOAD LOGIC
// ==========================================
void uploadFile(String imagePath, String textPath) {
    sysLog("Upload Start: " + imagePath);
    
    File imageFile = SD_MMC.open(imagePath);
    File textFile = SD_MMC.open(textPath);

    if (!imageFile || !textFile) {
        sysLog("Fehler: Dateien nicht gefunden.");
        if (imageFile) imageFile.close();
        if (textFile) textFile.close();
        return;
    }

    size_t imageSize = imageFile.size();
    Serial.printf("[DEBUG UPLOAD] Bildgröße: %d Bytes\n", imageSize);

    // Read text file
    String textContent = "";
    while(textFile.available()){
        textContent += (char)textFile.read();
    }
    textFile.close();
    Serial.printf("[DEBUG UPLOAD] Textinhalt eingelesen (%d Zeichen)\n", textContent.length());

    // Host & URL Konfiguration
    const char* host = UPLOAD_HOST.c_str();
    const int port = 443; // HTTPS
    String urlPath = UPLOAD_PATH;

    WiFiClientSecure client;
    client.setInsecure(); // Ignore certificate validation

    if (!client.connect(host, port)) {
        sysLog("Fehler: Verbindung fehlgeschlagen zu " + String(host));
        imageFile.close();
        return;
    }

    String boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
    
    // Part 1: Text content
    String bodyStart = "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"textFileContent\"\r\n\r\n";
    bodyStart += textContent + "\r\n";

    // Part 2: Image Header
    bodyStart += "--" + boundary + "\r\n";
    bodyStart += "Content-Disposition: form-data; name=\"imageFile\"; filename=\"" + imagePath.substring(1) + "\"\r\n";
    bodyStart += "Content-Type: image/jpeg\r\n\r\n";

    // Part 3: Footer
    String bodyEnd = "\r\n--" + boundary + "--\r\n";

    // Content-Length must be exact
    size_t totalLength = bodyStart.length() + imageSize + bodyEnd.length();

    // HTTP Header manuell senden
    client.print(String("POST ") + urlPath + " HTTP/1.1\r\n");
    client.print(String("Host: ") + host + "\r\n");
    client.print("Connection: close\r\n");
    client.print("Content-Type: multipart/form-data; boundary=" + boundary + "\r\n");
    client.print("Content-Length: " + String(totalLength) + "\r\n");
    client.print("\r\n"); // Leere Zeile trennt Header vom Body

    // 1. Send Text and Header
    client.print(bodyStart);

    // 2. Stream image in chunks (prevents RAM issues)
    uint8_t buffer[1024]; 
    size_t bytesRead = 0;
    size_t totalUploaded = 0;
    
    while (imageFile.available()) {
        bytesRead = imageFile.read(buffer, sizeof(buffer));
        client.write(buffer, bytesRead);
        totalUploaded += bytesRead;
        
        // Log progress every ~20KB
        if (totalUploaded % (1024 * 20) < sizeof(buffer)) {
             Serial.printf("[DEBUG UPLOAD] ... %d / %d Bytes gesendet\n", totalUploaded, imageSize);
        }
    }
    imageFile.close();
    
    // 3. Send closing boundary
    client.print(bodyEnd);

    // Parse Server Response
    int httpCode = 0;
    unsigned long timeout = millis();
    while (client.connected() || client.available()) {
        // Timeout nach 15 Sekunden
        if (millis() - timeout > 15000) {
            sysLog("Fehler: Server Timeout.");
            break;
        }
        
        if (client.available()) {
            String line = client.readStringUntil('\n');
            line.trim(); // Entfernt \r
            
            // Read HTTP Status Code
            if (line.startsWith("HTTP/1.1 ")) {
                httpCode = line.substring(9, 12).toInt();
            }
            
            // Body starts after empty line
            if (line.length() == 0) {
                String responseBody = client.readString();
                break;
            }
        }
        delay(10);
    }
    client.stop();

    // Cleanup if successful
    if (httpCode == 200) {
        sysLog("Upload OK. Lösche lokale Dateien.");
        SD_MMC.remove(imagePath);
        SD_MMC.remove(textPath);
    } else {
        sysLog("Upload Fehler: " + String(httpCode));
    }
}

void uploadAllFilesToServer() {
    if (isBusy) {
        sysLog("System beschäftigt, kein Upload möglich.");
        return;
    }
    isBusy = true;
    setStatusLED(100, 0, 255); // Purple for uploading

    File root = SD_MMC.open("/");
    File file = root.openNextFile();
    while(file){
        String fileName = file.name();
        if (fileName.startsWith("bild_") && fileName.endsWith(".jpg")) {
            String imagePath = "/" + fileName;
            String textPath = imagePath;
            textPath.replace(".jpg", ".txt");

            if (SD_MMC.exists(textPath)) {
                uploadFile(imagePath, textPath);
            }
        }
        file = root.openNextFile();
    }
    root.close();

    setStatusLED(0, 0, 255); // Blue when done
    isBusy = false;
    sysLog("Alle Uploads beendet.");
}


// ==========================================
// MAIN LOOP
// ==========================================
void loop() {
  if (digitalRead(BUTTON_PIN) == LOW) {
    unsigned long pressTime = millis();
    while(digitalRead(BUTTON_PIN) == LOW) { delay(10); }
    unsigned long duration = millis() - pressTime;
    
    if (duration < 2000) { // Short press: ~0-2s
       processImage();
    } else if (duration >= 5000 && duration < 10000) { // Medium press: 5-10s
       uploadAllFilesToServer();
    } else if (duration >= 10000) { // Long press: >10s
       setStatusLED(255, 0, 0);
       WiFiManager wm;
       wm.resetSettings();
       ESP.restart();
    }
  }
}
