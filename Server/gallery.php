<?php
session_start();

// --- CONFIGURATION ---
$password = 'admin'; // WICHTIG: Ändern Sie dieses Passwort!
$uploadDir = 'uploads/';
$pageUrl = 'gallery.php';

// --- LOGOUT ---
if (isset($_GET['logout'])) {
    session_destroy();
    header('Location: ' . $pageUrl);
    exit;
}

// --- LOGIN HANDLING ---
$isLoggedIn = isset($_SESSION['loggedin']) && $_SESSION['loggedin'] === true;
$loginError = '';

if (isset($_POST['password'])) {
    if ($_POST['password'] === $password) {
        $_SESSION['loggedin'] = true;
        header('Location: ' . $pageUrl);
        exit;
    } else {
        $loginError = 'Falsches Passwort.';
    }
}

// --- DELETE HANDLING ---
if ($isLoggedIn && isset($_GET['delete'])) {
    // Security: Use basename to prevent directory traversal
    $fileNameToDelete = basename($_GET['delete']);
    $imagePath = $uploadDir . $fileNameToDelete;
    $textPath = $uploadDir . pathinfo($fileNameToDelete, PATHINFO_FILENAME) . '.txt';

    if (file_exists($imagePath)) {
        unlink($imagePath);
    }
    if (file_exists($textPath)) {
        unlink($textPath);
    }
    
    // Redirect to clean the URL
    header('Location: ' . $pageUrl);
    exit;
}
?>
<!DOCTYPE html>
<html lang="de">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>PaperCamera Galerie</title>
    <style>
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; background-color: #121212; color: #e0e0e0; margin: 0; padding: 20px; }
        .container { max-width: 900px; margin: auto; }
        h1, h2 { text-align: center; color: #bb86fc; }
        .login-box { background: #1e1e1e; padding: 30px; border-radius: 12px; max-width: 300px; margin: 50px auto; text-align: center; }
        input[type=password] { width: 90%; padding: 10px; margin: 10px 0 20px 0; border-radius: 5px; border: 1px solid #333; background: #2c2c2c; color: white; }
        button { background-color: #bb86fc; color: black; border: none; padding: 10px 20px; border-radius: 5px; cursor: pointer; font-weight: bold; }
        button:hover { background-color: #9955e8; }
        .error { color: #cf6679; }
        .logout { position: fixed; top: 15px; right: 20px; }
        .logout a { color: #bb86fc; text-decoration: none; }
        .gallery { display: grid; grid-template-columns: repeat(auto-fill, minmax(350px, 1fr)); gap: 20px; margin-top: 30px; }
        .photo-card { background: #1e1e1e; border-radius: 8px; overflow: hidden; border: 1px solid #333; display: flex; flex-direction: column; }
        .photo-card img { width: 100%; height: auto; display: block; }
        .content { padding: 15px; flex-grow: 1; }
        .ai-text { font-style: italic; color: #aaa; font-size: 0.9em; background: #252525; padding: 10px; border-radius: 5px; margin-bottom: 10px; white-space: pre-wrap; }
        .actions { text-align: right; padding: 0 15px 15px 15px; }
        .actions a { color: #cf6679; text-decoration: none; font-size: 0.9em; }
    </style>
</head>
<body>

<div class="container">
    <h1>PaperCamera Galerie</h1>

    <?php if ($isLoggedIn): ?>
        <div class="logout"><a href="?logout=true">Logout</a></div>
        
        <div class="gallery">
            <?php
            $files = glob($uploadDir . 'bild_*.jpg');
            // Sort files by number, descending
            usort($files, function($a, $b) {
                $num_a = (int)filter_var($a, FILTER_SANITIZE_NUMBER_INT);
                $num_b = (int)filter_var($b, FILTER_SANITIZE_NUMBER_INT);
                return $num_b <=> $num_a;
            });

            if (empty($files)) {
                echo "<p style='text-align: center;'>Noch keine Bilder hochgeladen.</p>";
            }

            foreach ($files as $image) {
                $imageName = basename($image);
                $textfile = $uploadDir . pathinfo($imageName, PATHINFO_FILENAME) . '.txt';
                $textContent = file_exists($textfile) ? htmlspecialchars(file_get_contents($textfile)) : '(Kein Text gefunden)';
                ?>
                <div class="photo-card">
                    <img src="<?= htmlspecialchars($image) ?>" alt="<?= htmlspecialchars($imageName) ?>" loading="lazy">
                    <div class="content">
                        <div class="ai-text"><?= $textContent ?></div>
                    </div>
                    <div class="actions">
                        <a href="?delete=<?= urlencode($imageName) ?>" onclick="return confirm('Soll dieser Eintrag wirklich gelöscht werden?');">Löschen</a>
                    </div>
                </div>
                <?php
            }
            ?>
        </div>

    <?php else: ?>
        
        <div class="login-box">
            <h2>Login</h2>
            <form method="POST" action="<?= $pageUrl ?>">
                <input type="password" name="password" placeholder="Passwort" required>
                <button type="submit">Anmelden</button>
            </form>
            <?php if ($loginError): ?>
                <p class="error"><?= $loginError ?></p>
            <?php endif; ?>
        </div>

    <?php endif; ?>

</div>

</body>
</html>
