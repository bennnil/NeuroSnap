<?php
// Datei: dropoff.php
// Funktion: Nimmt ein Bild vom ESP32 entgegen, speichert es und gibt die URL zurück.

header('Content-Type: text/plain');

// Ordner für die temporären Bilder
$target_dir = "dropoff/";

// Ordner erstellen, falls er noch nicht existiert
if (!file_exists($target_dir)) {
    mkdir($target_dir, 0755, true);
}

// Prüfen, ob ein Bild gesendet wurde
if ($_SERVER['REQUEST_METHOD'] === 'POST' && isset($_FILES['imageFile'])) {
    
    // Dateinamen generieren: Zeitstempel + Zufallszahl (verhindert Überschreiben)
    $extension = "jpg";
    $filename = time() . "_" . rand(1000, 9999) . "." . $extension;
    $target_file = $target_dir . $filename;

    // Datei verschieben
    if (move_uploaded_file($_FILES['imageFile']['tmp_name'], $target_file)) {
        
        // Die öffentliche URL zusammenbauen
        $protocol = (!empty($_SERVER['HTTPS']) && $_SERVER['HTTPS'] !== 'off' || $_SERVER['SERVER_PORT'] == 443) ? "https://" : "http://";
        $domain = $_SERVER['HTTP_HOST'];
        
        // Pfad zum aktuellen Script (ohne Dateinamen)
        $path = dirname($_SERVER['REQUEST_URI']);
        // Windows/Linux Slash-Korrektur
        $path = str_replace('\\', '/', $path);
        $path = rtrim($path, '/');
        
        // Finale URL
        $fullUrl = $protocol . $domain . $path . "/" . $target_file;
        
        // URL als Antwort zurückgeben (das liest der ESP32)
        echo $fullUrl;
        
    } else {
        http_response_code(500);
        echo "Error: Upload failed";
    }
} else {
    http_response_code(400);
    echo "Error: No file provided";
}
?>