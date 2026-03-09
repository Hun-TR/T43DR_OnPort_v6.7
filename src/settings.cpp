#include "settings.h"
#include "log_system.h"
#include "crypto_utils.h"
#include <Preferences.h>

WebServer server(80);
Settings settings;

void loadSettings() {
    Preferences prefs;
    prefs.begin("app-settings", true);

    settings.local_IP.fromString("192.168.1.160");
    settings.gateway.fromString("192.168.1.1");
    settings.subnet.fromString("255.255.255.0");
    settings.primaryDNS.fromString("8.8.8.8");

    settings.deviceName = prefs.getString("dev_name", "TEİAŞ EKLİM");
    settings.transformerStation = prefs.getString("tm_name", "Ankara TM");
    settings.username = prefs.getString("username", "admin");
    settings.currentBaudRate = 250000; // Sabit değer
    settings.passwordSalt = prefs.getString("p_salt", "");
    settings.passwordHash = prefs.getString("p_hash", "");

    // İlk kurulum: Eğer hiç parola kaydedilmemişse varsayılan parolayı ayarla
    if (settings.passwordSalt.length() == 0 || settings.passwordHash.length() == 0) {
        String newSalt = generateRandomToken(16); // Rastgele bir tuz üret
        settings.passwordSalt = newSalt;
        // Varsayılan parola "1234"
        settings.passwordHash = sha256("1234", newSalt);
        
        // Bu değişiklikleri kalıcı hale getirmek için yazma modunda açıp kaydedelim.
        Preferences writePrefs;
        writePrefs.begin("app-settings", false);
        writePrefs.putString("p_salt", settings.passwordSalt);
        writePrefs.putString("p_hash", settings.passwordHash);
        writePrefs.end();
    }

    prefs.end();

    // Oturum başlangıçta geçersiz
    settings.sessionToken = "";
    settings.sessionStartTime = 0;
    settings.isAdminSession = false;  // BU SATIRI EKLEYİN
    settings.SESSION_TIMEOUT = 7200000; // 120 dakika

    addLog("Ayarlar yüklendi", INFO, "SETTINGS");
}

bool saveSettings(const String& newDevName, const String& newTmName, 
                  const String& newUsername, const String& newPassword) {
    
    if (newDevName.length() < 3 || newDevName.length() > 50) return false;
    
    // Kullanıcı adı validasyonu
    if (newUsername.length() < 3 || newUsername.length() > 30) return false;
    
    // Kullanıcı adı karakter kontrolü
    for (char c : newUsername) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || 
              (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }

    Preferences prefs;
    prefs.begin("app-settings", false);

    if (newDevName != settings.deviceName) {
        settings.deviceName = newDevName;
        prefs.putString("dev_name", newDevName);
    }

    if (newTmName != settings.transformerStation) {
        settings.transformerStation = newTmName;
        prefs.putString("tm_name", newTmName);
    }

    if (newUsername != settings.username) {
        settings.username = newUsername;
        prefs.putString("username", newUsername);
    }

    // Şifre değişikliği
    if (newPassword.length() >= 4) {
        String newSalt = generateRandomToken(16); // Her zaman yeni, rastgele bir tuz
        settings.passwordSalt = newSalt;
        settings.passwordHash = sha256(newPassword, newSalt);
        prefs.putString("p_salt", settings.passwordSalt);
        prefs.putString("p_hash", settings.passwordHash);
        
        // Şifre değiştiğinde oturumu sonlandır
        settings.sessionToken = "";
        
        addLog("Şifre değiştirildi, oturum sonlandırıldı.", INFO, "SETTINGS");
    }

    prefs.end();
    addLog("Ayarlar kaydedildi", SUCCESS, "SETTINGS");
    return true;
}

void initEthernet() {
    // WT32-ETH01 için sabit pinler
    ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT);
    
    // Statik IP kullan
    ETH.config(settings.local_IP, settings.gateway, settings.subnet, settings.primaryDNS);

    // Bağlantı kontrolü
    unsigned long start = millis();
    while (!ETH.linkUp() && millis() - start < 3000) {  // 3 saniye bekle
        delay(100);
    }
    
    if (ETH.linkUp()) {
        addLog("Ethernet OK: " + ETH.localIP().toString(), SUCCESS, "ETH");
    } else {
        addLog("Ethernet kablosu takılı değil", WARN, "ETH");
    }
}