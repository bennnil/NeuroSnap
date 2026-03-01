<?php
header('Content-Type: text/plain');

$uploadDir = 'uploads/';

// Create upload directory if it doesn't exist
if (!is_dir($uploadDir)) {
    if (!mkdir($uploadDir, 0755, true)) {
        http_response_code(500);
        die("Error: Failed to create upload directory.");
    }
}

// Check if files and text content are sent
if (isset($_FILES['imageFile']) && isset($_POST['textFileContent'])) {
    
    $imageFile = $_FILES['imageFile'];
    $textContent = $_POST['textFileContent'];
    
    // --- Security: Sanitize filename ---
    $originalName = basename($imageFile['name']);
    $fileExtension = strtolower(pathinfo($originalName, PATHINFO_EXTENSION));
    $fileNameWithoutExt = pathinfo($originalName, PATHINFO_FILENAME);
    
    // Allow only specific characters and jpg extension
    if ($fileExtension !== 'jpg' || !preg_match('/^bild_[0-9]+$/', $fileNameWithoutExt)) {
        http_response_code(400);
        die("Error: Invalid filename format.");
    }
    
    $counter = (int)str_replace('bild_', '', $fileNameWithoutExt);
    
    $safeImageName = 'bild_' . $counter . '.jpg';
    $safeTextName = 'bild_' . $counter . '.txt';
    $imagePath = $uploadDir . $safeImageName;
    $textPath = $uploadDir . $safeTextName;
    
    // Solange eine der Dateien schon existiert, zählen wir hoch
    while (file_exists($imagePath) || file_exists($textPath)) {
        $counter++;
        $safeImageName = 'bild_' . $counter . '.jpg';
        $safeTextName = 'bild_' . $counter . '.txt';
        $imagePath = $uploadDir . $safeImageName;
        $textPath = $uploadDir . $safeTextName;
    }
    // --------------------------------------
    
    // Check for upload errors
    if ($imageFile['error'] !== UPLOAD_ERR_OK) {
        http_response_code(500);
        die("Error: Image upload error code: " . $imageFile['error']);
    }

    // Move the uploaded image file
    if (move_uploaded_file($imageFile['tmp_name'], $imagePath)) {
        // Save the text content to a corresponding .txt file
        if (file_put_contents($textPath, $textContent) !== false) {
            http_response_code(200);
            // Wir geben den finalen Namen zurück, das ist gut fürs Debugging am ESP32
            echo "Success: Files uploaded and saved as " . $safeImageName;
        } else {
            // If text saving fails, delete the orphaned image
            unlink($imagePath); 
            http_response_code(500);
            die("Error: Failed to save text file.");
        }
    } else {
        http_response_code(500);
        die("Error: Failed to move uploaded image file.");
    }
    
} else {
    http_response_code(400);
    die("Error: Required data (imageFile, textFileContent) not provided.");
}
?>