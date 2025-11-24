#include "password_policy.h"
#include <Preferences.h>
#include "settings.h"
#include "log_system.h"
#include "crypto_utils.h"
#include <WebServer.h>

extern WebServer server;
extern Settings settings;

// Global password policy değişkeni
PasswordPolicy passwordPolicy = {
    .firstLoginPasswordChange = true,
    .passwordExpiry = true,
    .passwordExpiryDays = 90,
    .minPasswordLength = 6,  // 6'ya düşürüldü
    .requireComplexPassword = true,
    .lastPasswordChange = 0,
    .isDefaultPassword = true,
    .passwordHistory = 3
};

// Parola politikasını yükle
void loadPasswordPolicy() {
    Preferences prefs;
    prefs.begin("pwd-policy", true);
    
    passwordPolicy.firstLoginPasswordChange = prefs.getBool("first_change", true);
    passwordPolicy.passwordExpiry = prefs.getBool("expiry", true);
    passwordPolicy.passwordExpiryDays = prefs.getInt("expiry_days", 90);
    passwordPolicy.minPasswordLength = prefs.getInt("min_length", 6);
    passwordPolicy.requireComplexPassword = prefs.getBool("complex", true);
    passwordPolicy.lastPasswordChange = prefs.getULong("last_change", 0);
    passwordPolicy.isDefaultPassword = prefs.getBool("is_default", true);
    passwordPolicy.passwordHistory = prefs.getInt("history", 3);
    
    prefs.end();
    
    addLog("Parola politikası yüklendi", INFO, "POLICY");
}

// Parola politikasını kaydet
void savePasswordPolicy() {
    Preferences prefs;
    prefs.begin("pwd-policy", false);
    
    prefs.putBool("first_change", passwordPolicy.firstLoginPasswordChange);
    prefs.putBool("expiry", passwordPolicy.passwordExpiry);
    prefs.putInt("expiry_days", passwordPolicy.passwordExpiryDays);
    prefs.putInt("min_length", passwordPolicy.minPasswordLength);
    prefs.putBool("complex", passwordPolicy.requireComplexPassword);
    prefs.putULong("last_change", passwordPolicy.lastPasswordChange);
    prefs.putBool("is_default", passwordPolicy.isDefaultPassword);
    prefs.putInt("history", passwordPolicy.passwordHistory);
    
    prefs.end();
}

// Parola karmaşıklık kontrolü - BASİTLEŞTİRİLDİ
bool isPasswordComplex(const String& password) {
    if (password.length() < passwordPolicy.minPasswordLength) {
        return false;
    }
    
    if (!passwordPolicy.requireComplexPassword) {
        return true;
    }
    
    bool hasUpper = false;
    bool hasLower = false;
    bool hasDigit = false;
    
    for (char c : password) {
        if (c >= 'A' && c <= 'Z') hasUpper = true;
        else if (c >= 'a' && c <= 'z') hasLower = true;
        else if (c >= '0' && c <= '9') hasDigit = true;
    }
    
    // Sadece büyük harf, küçük harf ve rakam yeterli (özel karakter zorunlu değil)
    return hasUpper && hasLower && hasDigit;
}

// Parola geçmişi kontrolü
bool isPasswordInHistory(const String& password) {
    Preferences prefs;
    prefs.begin("pwd-history", true);
    
    for (int i = 0; i < passwordPolicy.passwordHistory; i++) {
        String key = "pwd_" + String(i);
        String oldHash = prefs.getString(key.c_str(), "");
        
        if (oldHash.length() > 0) {
            String saltKey = "salt_" + String(i);
            String oldSalt = prefs.getString(saltKey.c_str(), "");
            
            if (sha256(password, oldSalt) == oldHash) {
                prefs.end();
                return true;
            }
        }
    }
    
    prefs.end();
    return false;
}

// Parola geçmişine ekle
void addPasswordToHistory(const String& passwordHash, const String& salt) {
    Preferences prefs;
    prefs.begin("pwd-history", false);
    
    for (int i = passwordPolicy.passwordHistory - 1; i > 0; i--) {
        String keyFrom = "pwd_" + String(i - 1);
        String keyTo = "pwd_" + String(i);
        String saltKeyFrom = "salt_" + String(i - 1);
        String saltKeyTo = "salt_" + String(i);
        
        String hash = prefs.getString(keyFrom.c_str(), "");
        String oldSalt = prefs.getString(saltKeyFrom.c_str(), "");
        
        if (hash.length() > 0) {
            prefs.putString(keyTo.c_str(), hash);
            prefs.putString(saltKeyTo.c_str(), oldSalt);
        }
    }
    
    prefs.putString("pwd_0", passwordHash);
    prefs.putString("salt_0", salt);
    
    prefs.end();
}

// Parola süresi dolmuş mu?
bool isPasswordExpired() {
    if (!passwordPolicy.passwordExpiry) {
        return false;
    }
    
    if (passwordPolicy.lastPasswordChange == 0) {
        return true;
    }
    
    unsigned long daysSinceChange = (millis() - passwordPolicy.lastPasswordChange) / 86400000;
    return daysSinceChange >= passwordPolicy.passwordExpiryDays;
}

// Parola değiştirme zorunluluğu kontrolü
bool mustChangePassword() {
    if (passwordPolicy.isDefaultPassword && passwordPolicy.firstLoginPasswordChange) {
        return true;
    }
    
    if (isPasswordExpired()) {
        return true;
    }
    
    return false;
}

// API handler - Parola değiştirme (KULLANICI ADI İLE)
void handlePasswordChangeAPI() {
    // Yeni kullanıcı adı ve parola değiştirme
    String username = server.arg("username");
    String newPassword = server.arg("newPassword");
    String confirmPassword = server.arg("confirmPassword");
    
    // Kullanıcı adı validasyonu
    if (username.length() < 3 || username.length() > 30) {
        server.send(400, "application/json", "{\"success\":false, \"error\":\"Kullanıcı adı 3-30 karakter arasında olmalıdır\"}");
        return;
    }
    
    // Kullanıcı adı karakter kontrolü (sadece harf, rakam, tire, alt çizgi)
    bool validUsername = true;
    for (char c : username) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            validUsername = false;
            break;
        }
    }
    
    if (!validUsername) {
        server.send(400, "application/json", "{\"success\":false, \"error\":\"Kullanıcı adı sadece harf, rakam, tire ve alt çizgi içerebilir\"}");
        return;
    }
    
    // Yeni parolaların eşleşme kontrolü
    if (newPassword != confirmPassword) {
        server.send(400, "application/json", "{\"success\":false, \"error\":\"Yeni parolalar eşleşmiyor\"}");
        return;
    }
    
    // Parola karmaşıklık kontrolü
    if (!isPasswordComplex(newPassword)) {
        server.send(400, "application/json", "{\"success\":false, \"error\":\"Parola gereksinimleri karşılanmıyor. En az 6 karakter, büyük/küçük harf ve rakam içermelidir\"}");
        return;
    }
    
    // Parola geçmişi kontrolü
    if (isPasswordInHistory(newPassword)) {
        server.send(400, "application/json", "{\"success\":false, \"error\":\"Bu parola daha önce kullanılmış\"}");
        return;
    }
    
    // Yeni parolayı kaydet
    String newSalt = generateRandomToken(16);
    String newHash = sha256(newPassword, newSalt);
    
    // Geçmişe ekle
    addPasswordToHistory(newHash, newSalt);
    
    // Settings'i güncelle (hem kullanıcı adı hem parola)
    settings.username = username;
    settings.passwordSalt = newSalt;
    settings.passwordHash = newHash;
    
    // Preferences'a kaydet
    Preferences prefs;
    prefs.begin("app-settings", false);
    prefs.putString("username", username);
    prefs.putString("p_salt", newSalt);
    prefs.putString("p_hash", newHash);
    prefs.end();
    
    // Politikayı güncelle
    passwordPolicy.isDefaultPassword = false;
    passwordPolicy.lastPasswordChange = millis();
    savePasswordPolicy();
    
    addLog("✅ Kullanıcı adı (" + username + ") ve parola başarıyla değiştirildi", SUCCESS, "AUTH");
    
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Kullanıcı adı ve parola değiştirildi. Giriş sayfasına yönlendiriliyorsunuz...\"}");
}

// API - Parola değiştirme session kontrolü (soft check)
void handleCheckPasswordSession() {
    // Token kontrolü opsiyonel
    bool hasValidToken = false;
    
    if (server.hasHeader("Authorization")) {
        String authHeader = server.header("Authorization");
        if (authHeader.startsWith("Bearer ")) {
            String token = authHeader.substring(7);
            // isTokenValid fonksiyonu auth_system.cpp'de tanımlı olmalı
            // hasValidToken = isTokenValid(token);
        }
    }
    
    String response = "{";
    response += "\"validSession\":" + String(hasValidToken ? "true" : "false") + ",";
    response += "\"message\":\"" + String(hasValidToken ? 
        "Oturum geçerli" : 
        "Oturum bulunamadı veya geçersiz ama kullanıcı adı ile devam edebilirsiniz") + "\"";
    response += "}";
    
    server.send(200, "application/json", response);
}