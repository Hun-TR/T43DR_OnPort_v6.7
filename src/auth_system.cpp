#include "auth_system.h"
#include "settings.h"
#include "log_system.h"
#include "crypto_utils.h"
#include "password_policy.h"
#include <WebServer.h>
#include <ArduinoJson.h>

extern Settings settings;
extern WebServer server;
extern PasswordPolicy passwordPolicy;

static int loginAttempts = 0;
static unsigned long lockoutTime = 0;
const int MAX_LOGIN_ATTEMPTS = 5;
const unsigned long LOCKOUT_DURATION = 300000; // 5 dakika

// Yönetici bilgileri (sabit tanımlı)
const String ADMIN_USERNAME = "eklim";
const String ADMIN_PASSWORD = "mdhc06*";

// Oturumu jeton ile kontrol et
bool checkSession() {
    String token = "";
    if (server.hasHeader("Authorization")) {
        // "Bearer a1b2c3d4..." formatını bekliyoruz
        String authHeader = server.header("Authorization");
        if (authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }
    }

    if (token.length() == 0 || settings.sessionToken.length() == 0 || token != settings.sessionToken) {
        return false;
    }

    if (millis() - settings.sessionStartTime > settings.SESSION_TIMEOUT) {
        settings.sessionToken = ""; // Jetonu geçersiz kıl
        addLog("Oturum zaman aşımına uğradı", INFO, "AUTH");
        return false;
    }
    
    // Aktivite olduğunda oturum süresini yenile
    settings.sessionStartTime = millis();
    return true;
}

void handleUserLogin() {
    // Rate limiting kontrolü
    if (lockoutTime > 0 && millis() < lockoutTime) {
        unsigned long remainingTime = (lockoutTime - millis()) / 1000;
        addLog("Çok fazla başarısız giriş denemesi. Kalan süre: " + String(remainingTime) + "s", WARN, "AUTH");
        server.send(429, "application/json", 
            "{\"error\":\"Çok fazla başarısız deneme. " + String(remainingTime) + " saniye sonra tekrar deneyin.\"}");
        return;
    }

    String u = server.arg("username");
    String p = server.arg("password");

    // Input validation
    if (u.length() == 0 || p.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"Kullanıcı adı ve şifre boş olamaz.\"}");
        return;
    }

    // Kullanıcı adı ve şifre uzunluk kontrolü
    if (u.length() > 50 || p.length() > 100) {
        addLog("Aşırı uzun giriş denemesi", WARN, "AUTH");
        server.send(400, "application/json", "{\"error\":\"Geçersiz giriş bilgileri.\"}");
        return;
    }

    // YÖNETİCİ GİRİŞİ KONTROLÜ
    if (u == ADMIN_USERNAME && p == ADMIN_PASSWORD) {
        // Yönetici girişi başarılı
        settings.sessionToken = generateRandomToken(32);
        settings.sessionStartTime = millis();
        settings.isAdminSession = true; // Yönetici oturumu işareti
        loginAttempts = 0;
        lockoutTime = 0;
        
        addLog("✅ YÖNETİCİ girişi başarılı: " + u, SUCCESS, "AUTH");
        
        // Yönetici için şifre değiştirme zorunluluğu YOK
        String response = "{";
        response += "\"success\":true,";
        response += "\"token\":\"" + settings.sessionToken + "\",";
        response += "\"mustChangePassword\":false,"; // Her zaman false
        response += "\"isAdmin\":true,"; // Yönetici işareti
        response += "\"redirectUrl\":\"/\"";
        response += "}";
        
        server.send(200, "application/json", response);
        return;
    }

    // NORMAL KULLANICI GİRİŞİ KONTROLÜ
    if (u == settings.username) {
        String hashedAttempt = sha256(p, settings.passwordSalt);
        if (hashedAttempt == settings.passwordHash) {
            settings.sessionToken = generateRandomToken(32);
            settings.sessionStartTime = millis();
            settings.isAdminSession = false; // Normal kullanıcı oturumu
            loginAttempts = 0;
            lockoutTime = 0;
            
            addLog("✅ Başarılı giriş: " + u, SUCCESS, "AUTH");
            
            // Parola değiştirme kontrolü - LOGIN SONRASI
            bool mustChange = mustChangePassword();
            
            addLog("Parola değiştirme zorunlu mu? " + String(mustChange ? "EVET" : "HAYIR"), INFO, "AUTH");
            
            // JSON response
            String response = "{";
            response += "\"success\":true,";
            response += "\"token\":\"" + settings.sessionToken + "\",";
            response += "\"mustChangePassword\":" + String(mustChange ? "true" : "false");
            response += ",\"isAdmin\":false"; // Normal kullanıcı
            
            if (mustChange) {
                response += ",\"redirectUrl\":\"/password_change.html\"";
                if (passwordPolicy.isDefaultPassword) {
                    response += ",\"reason\":\"İlk girişinizde varsayılan kullanıcı adı ve parolayı değiştirmeniz gerekmektedir.\"";
                    addLog("Sebep: Varsayılan parola", INFO, "AUTH");
                } else if (isPasswordExpired()) {
                    response += ",\"reason\":\"Parolanızın süresi dolmuştur. Yeni kullanıcı adı ve parola belirlemeniz gerekmektedir.\"";
                    addLog("Sebep: Parola süresi dolmuş", INFO, "AUTH");
                }
            } else {
                response += ",\"redirectUrl\":\"/\"";
                addLog("Normal ana sayfaya yönlendirme", INFO, "AUTH");
            }
            response += "}";
            
            addLog("JSON yanıt hazırlandı: " + response.substring(0, 50) + "...", INFO, "AUTH");
            server.send(200, "application/json", response);
            return;
        }
    }

    // Başarısız giriş işlemi
    loginAttempts++;
    addLog("❌ Başarısız giriş denemesi (#" + String(loginAttempts) + "): " + u, ERROR, "AUTH");

    // Maksimum deneme sayısına ulaşıldı mı?
    if (loginAttempts >= MAX_LOGIN_ATTEMPTS) {
        lockoutTime = millis() + LOCKOUT_DURATION;
        addLog("🔒 IP adresi " + String(LOCKOUT_DURATION/1000) + " saniye kilitlendi", WARN, "AUTH");
        server.send(429, "application/json", 
            "{\"error\":\"Çok fazla başarısız deneme. " + String(LOCKOUT_DURATION/1000) + " saniye sonra tekrar deneyin.\"}");
        return;
    }

    server.send(401, "application/json", "{\"success\":false, \"error\":\"Kullanıcı adı veya şifre hatalı!\"}");
}

void handleUserLogout() {
    settings.sessionToken = "";
    settings.isAdminSession = false; // Yönetici oturumunu temizle
    addLog("🚪 Çıkış yapıldı", INFO, "AUTH");
    server.send(200, "application/json", "{\"success\":true}");
}

// Sadece gelen jetonun geçerli olup olmadığını kontrol eder (WebSocket için)
bool isTokenValid(const String& token) {
    if (token.length() == 0 || settings.sessionToken.length() == 0 || token != settings.sessionToken) {
        return false;
    }

    if (millis() - settings.sessionStartTime > settings.SESSION_TIMEOUT) {
        return false;
    }

    // WebSocket aktivitesi için oturum süresini yenilemeyelim,
    // bu sadece API isteklerinde olmalı.
    return true;
}

// Yönetici oturumu kontrolü
bool isAdminSession() {
    return settings.isAdminSession;
}