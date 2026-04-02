// web_routes.cpp - Düzeltilmiş ve Temizlenmiş Routing
#include "web_routes.h"
#include "auth_system.h"
#include "settings.h"
#include "ntp_handler.h"
#include "uart_handler.h"
#include "log_system.h"
#include "backup_restore.h"
#include "password_policy.h"
#include <LittleFS.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ESPmDNS.h>
#include "datetime_handler.h"
#include "fault_parser.h"
#include <vector>  // std::vector için

extern DateTimeData datetimeData;

// UART istatistikleri - extern olarak kullan (uart_handler.cpp'de tanımlı)
extern UARTStatistics uartStats;  // DÜZELTME: Burada tanımlama değil, extern kullanım

// Log sistemi - YENİ YAPIYLA
extern std::vector<LogEntry> logStorage;  // Artık vector kullanıyoruz

// Rate limiting için global değişkenler
struct RateLimitData {
    IPAddress clientIP;
    unsigned long requests[20];
    int requestIndex = 0;
    unsigned long lastReset = 0;
};
RateLimitData rateLimitData;

// Diğer extern tanımlamalar
extern String getCurrentDateTime();
extern String getUptime();
extern bool isTimeSynced();
extern WebServer server;
extern Settings settings;
extern bool ntpConfigured;
extern PasswordPolicy passwordPolicy;

// Arıza kayıtları için global array
static FaultRecord faultRecords[100]; // Maksimum 100 arıza kaydı
static int faultCount = 0;


// Security headers ekle
void addSecurityHeaders() {
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("X-Frame-Options", "DENY");
    server.sendHeader("X-XSS-Protection", "1; mode=block");
    server.sendHeader("Referrer-Policy", "strict-origin-when-cross-origin");
    server.sendHeader("Content-Security-Policy", "default-src 'self' 'unsafe-inline'");

    // Gzip desteği bildirimi
    server.sendHeader("Accept-Encoding", "gzip, deflate");
}

// Rate limiting kontrolü
bool checkRateLimit() {
    IPAddress clientIP = server.client().remoteIP();
    unsigned long now = millis();
    
    // Farklı IP veya 1 dakika geçmişse sıfırla
    if (clientIP != rateLimitData.clientIP || now - rateLimitData.lastReset > 60000) {
        rateLimitData.clientIP = clientIP;
        rateLimitData.requestIndex = 0;
        rateLimitData.lastReset = now;
    }
    
    // 1 dakikada 60 istekten fazlasına izin verme
    if (rateLimitData.requestIndex >= 20) {
        addLog("⚠️ Rate limit aşıldı: " + clientIP.toString(), WARN, "SECURITY");
        return false;
    }
    
    rateLimitData.requests[rateLimitData.requestIndex++] = now;
    return true;
}

// Device Info API
void handleDeviceInfoAPI() {
    JsonDocument doc;
    doc["ip"] = ETH.localIP().toString();
    doc["mac"] = ETH.macAddress();
    doc["hostname"] = "teias-eklim";
    doc["mdns"] = "teias-eklim.local";
    doc["version"] = "v5.2";
    doc["model"] = "WT32-ETH01";
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// System Info API (Auth gerekli)
void handleSystemInfoAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Rate limiting
    if (!checkRateLimit()) {
        server.send(429, "application/json", "{\"error\":\"Too many requests\"}");
        return;
    }
    
    JsonDocument doc;
    
    // Hardware info
    doc["hardware"]["chip"] = "ESP32";
    doc["hardware"]["cores"] = 2;
    doc["hardware"]["frequency"] = getCpuFrequencyMhz();
    doc["hardware"]["revision"] = ESP.getChipRevision();
    doc["hardware"]["flashSize"] = ESP.getFlashChipSize();
    
    // Memory info
    doc["memory"]["totalHeap"] = ESP.getHeapSize();
    doc["memory"]["freeHeap"] = ESP.getFreeHeap();
    doc["memory"]["usedHeap"] = ESP.getHeapSize() - ESP.getFreeHeap();
    doc["memory"]["minFreeHeap"] = ESP.getMinFreeHeap();
    doc["memory"]["maxAllocHeap"] = ESP.getMaxAllocHeap();
    
    // Software info
    doc["software"]["version"] = "5.2";
    doc["software"]["sdk"] = ESP.getSdkVersion();
    doc["software"]["buildDate"] = __DATE__ " " __TIME__;
    doc["software"]["uptime"] = millis() / 1000;
    
    // UART statistics - uartStats extern olarak kullanılıyor
    doc["uart"]["txCount"] = uartStats.totalFramesSent;
    doc["uart"]["rxCount"] = uartStats.totalFramesReceived;
    doc["uart"]["errors"] = uartStats.frameErrors + uartStats.checksumErrors + uartStats.timeoutErrors;
    doc["uart"]["successRate"] = uartStats.successRate;
    doc["uart"]["baudRate"] = 250000;  // settings.currentBaudRate yerine sabit değer
    
    // File system info
    size_t totalBytes = LittleFS.totalBytes();
    size_t usedBytes = LittleFS.usedBytes();
    doc["filesystem"]["type"] = "LittleFS";
    doc["filesystem"]["total"] = totalBytes;
    doc["filesystem"]["used"] = usedBytes;
    doc["filesystem"]["free"] = totalBytes - usedBytes;
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// Network Configuration API - GET (değişiklik yok)
void handleGetNetworkAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addSecurityHeaders();
    
    JsonDocument doc;
    
    // Mevcut ethernet durumu
    doc["linkUp"] = ETH.linkUp();
    doc["linkSpeed"] = ETH.linkSpeed();
    doc["fullDuplex"] = ETH.fullDuplex();
    doc["mac"] = ETH.macAddress();
    
    // IP bilgileri
    doc["ip"] = ETH.localIP().toString();
    doc["gateway"] = ETH.gatewayIP().toString();
    doc["subnet"] = ETH.subnetMask().toString();
    doc["dns1"] = ETH.dnsIP().toString();
    doc["dns2"] = ETH.dnsIP(1).toString();
    
    // Artık her zaman static olarak göster (DHCP yok)
    doc["dhcp"] = false;
    doc["mode"] = "static";
    
    String output;
    serializeJson(doc, output);
    
    server.send(200, "application/json", output);
}

// Network Configuration API - POST (Sadece Statik IP versiyonu)
void handlePostNetworkAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addSecurityHeaders();
    
    // Form'dan gelen değerler - ipMode kontrolü kaldırıldı, her zaman static
    String staticIP = server.arg("staticIP");
    String gateway = server.arg("gateway");
    String subnet = server.arg("subnet");
    String dns1 = server.arg("dns1");
    String dns2 = server.arg("dns2"); // Opsiyonel
    
    // Validation
    if (staticIP.length() == 0 || gateway.length() == 0 || 
        subnet.length() == 0 || dns1.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"Zorunlu alanlar eksik\"}");
        return;
    }
    
    // IP format validation
    IPAddress testIP, testGW, testSubnet, testDNS1;
    
    if (!testIP.fromString(staticIP)) {
        server.send(400, "application/json", "{\"error\":\"Geçersiz IP adresi\"}");
        return;
    }
    
    if (!testGW.fromString(gateway)) {
        server.send(400, "application/json", "{\"error\":\"Geçersiz Gateway adresi\"}");
        return;
    }
    
    if (!testSubnet.fromString(subnet)) {
        server.send(400, "application/json", "{\"error\":\"Geçersiz Subnet maskesi\"}");
        return;
    }
    
    if (!testDNS1.fromString(dns1)) {
        server.send(400, "application/json", "{\"error\":\"Geçersiz DNS1 adresi\"}");
        return;
    }
    
    // DNS2 opsiyonel ama girilmişse validate et
    IPAddress testDNS2;
    if (dns2.length() > 0) {
        if (!testDNS2.fromString(dns2)) {
            server.send(400, "application/json", "{\"error\":\"Geçersiz DNS2 adresi\"}");
            return;
        }
    }
     // Settings'e kaydet
    Preferences prefs;
    prefs.begin("app-settings", false);
    prefs.putString("local_ip", staticIP);
    prefs.putString("gateway", gateway);
    prefs.putString("subnet", subnet);
    prefs.putString("dns1", dns1);
    
    if (dns2.length() > 0) {
        prefs.putString("dns2", dns2);
    }
    
    prefs.end();
    
    // Global settings güncelle
    settings.local_IP.fromString(staticIP);
    settings.gateway.fromString(gateway);
    settings.subnet.fromString(subnet);
    settings.primaryDNS.fromString(dns1);
    
    if (dns2.length() > 0) {
        settings.secondaryDNS.fromString(dns2);
    }
    
    addLog("✅ Statik IP ayarları kaydedildi: " + staticIP, SUCCESS, "NETWORK");
    addLog("  Gateway: " + gateway, INFO, "NETWORK");
    addLog("  Subnet: " + subnet, INFO, "NETWORK");
    addLog("  DNS1: " + dns1, INFO, "NETWORK");
    
    if (dns2.length() > 0) {
        addLog("  DNS2: " + dns2, INFO, "NETWORK");
    }
    
    // Başarılı yanıt
    JsonDocument response;
    response["success"] = true;
    response["message"] = "Statik IP ayarları kaydedildi. Cihaz yeniden başlatılıyor...";
    response["newIP"] = staticIP;
    
    String output;
    serializeJson(response, output);
    
    server.send(200, "application/json", output);
    
    // 2 saniye bekle ve restart et
    delay(2000);
    ESP.restart();
}

// Notification API
void handleNotificationAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    JsonDocument doc;
    JsonArray notifications = doc.to<JsonArray>();
    
    // Son kritik logları bildirim olarak göster - YENİ YAPI İLE
    int notificationCount = 0;
    
    // logStorage vector'ünden son hataları al
    for (auto it = logStorage.rbegin(); it != logStorage.rend() && notificationCount < 10; ++it) {
        if (it->level == ERROR || it->level == WARN) {
            JsonObject notif = notifications.add<JsonObject>();
            notif["id"] = notificationCount;
            notif["type"] = (it->level == ERROR) ? "error" : "warning";
            notif["message"] = it->message;
            notif["time"] = it->timestamp;
            notif["read"] = false;
            notificationCount++;
        }
    }
    
    doc["count"] = notificationCount;
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// System Reboot API
void handleSystemRebootAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addLog("🔄 Sistem yeniden başlatılıyor...", WARN, "SYSTEM");
    server.send(200, "application/json", "{\"success\":true,\"message\":\"Sistem 3 saniye içinde yeniden başlatılacak\"}");
    
    delay(3000);
    ESP.restart();
}

// DateTime bilgisi çek - GET /api/datetime
void handleGetDateTimeAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addSecurityHeaders();
    
    JsonDocument doc;
    
    // Mevcut datetime verisi
    doc["isValid"] = isDateTimeDataValid();
    doc["date"] = datetimeData.date;
    doc["time"] = datetimeData.time;
    doc["rawData"] = datetimeData.rawData;
    
    if (datetimeData.lastUpdate > 0) {
        unsigned long elapsed = (millis() - datetimeData.lastUpdate) / 1000;
        doc["lastUpdate"] = String(elapsed) + " saniye önce";
        doc["lastUpdateTimestamp"] = datetimeData.lastUpdate;
    } else {
        doc["lastUpdate"] = "Henüz çekilmedi";
        doc["lastUpdateTimestamp"] = 0;
    }
    
    // ESP32 sistem saati
    doc["esp32DateTime"] = getCurrentESP32DateTime();
    
    String output;
    serializeJson(doc, output);
    
    server.send(200, "application/json", output);
}

// DateTime bilgisi güncelle - POST /api/datetime/fetch  
void handleFetchDateTimeAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addSecurityHeaders();
    
    addLog("DateTime bilgisi dsPIC'ten çekiliyor...", INFO, "DATETIME");
    
    bool success = requestDateTimeFromDsPIC();
    
    JsonDocument doc;
    doc["success"] = success;
    
    if (success) {
        doc["message"] = "Tarih-saat bilgisi başarıyla güncellendi";
        doc["date"] = datetimeData.date;
        doc["time"] = datetimeData.time;
        doc["rawData"] = datetimeData.rawData;
    } else {
        doc["message"] = "Tarih-saat bilgisi alınamadı";
        doc["error"] = "dsPIC'ten yanıt alınamadı veya format geçersiz";
    }
    
    String output;
    serializeJson(doc, output);
    
    server.send(success ? 200 : 500, "application/json", output);
}

// DateTime ayarla - POST /api/datetime/set
void handleSetDateTimeAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addSecurityHeaders();
    
    String manualDate = server.arg("manualDate");  // Format: 2025-02-27
    String manualTime = server.arg("manualTime");  // Format: 11:22:33
    
    // Input validation
    if (manualDate.length() == 0 || manualTime.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"Tarih ve saat alanları boş olamaz\"}");
        return;
    }
    
    if (!validateDateTime(manualDate, manualTime)) {
        server.send(400, "application/json", "{\"error\":\"Geçersiz tarih veya saat formatı\"}");
        return;
    }
    
    addLog("Manual tarih-saat ayarlanıyor: " + manualDate + " " + manualTime, INFO, "DATETIME");
    
    bool success = setDateTimeToDsPIC(manualDate, manualTime);
    
    JsonDocument doc;
    doc["success"] = success;
    
    if (success) {
        doc["message"] = "Tarih-saat başarıyla ayarlandı";
        doc["setDate"] = manualDate;
        doc["setTime"] = manualTime;
        doc["timeCommand"] = formatTimeCommand(manualTime);
        doc["dateCommand"] = formatDateCommand(manualDate);
    } else {
        doc["message"] = "Tarih-saat ayarlanamadı";
        doc["error"] = "Komut gönderimi başarısız";
    }
    
    String output;
    serializeJson(doc, output);
    
    server.send(success ? 200 : 500, "application/json", output);
}



// handleSetCurrentTimeAPI fonksiyonunu güncelle
void handleSetCurrentTimeAPI() {
    // Bu fonksiyon artık kullanılmayacak
    // Bunun yerine client tarafında form alanları doldurulacak
    server.send(400, "application/json", "{\"error\":\"Bu endpoint artık kullanılmıyor\"}");
}


// mDNS güncelleme (teias-eklim.local)
void updateMDNS() {
    MDNS.end();
    
    if (MDNS.begin("teias-eklim")) {
        MDNS.addService("http", "tcp", 80);
        addLog("✅ mDNS güncellendi: teias-eklim.local", SUCCESS, "mDNS");
    } else {
        addLog("❌ mDNS başlatılamadı", ERROR, "mDNS");
    }
}

void serveStaticFile(const String& path, const String& contentType) {
    // Cache headers ekle
    String cacheControl = "public, max-age=";
    if (path.endsWith(".css") || path.endsWith(".js")) {
        cacheControl += "604800"; // 7 gün
        server.sendHeader("Cache-Control", cacheControl);
    } else if (path.endsWith(".html")) {
        cacheControl += "3600"; // 1 saat
        server.sendHeader("Cache-Control", cacheControl);
    }
    
    // ETag desteği için basit bir hash
    String etag = "\"" + String(path.length()) + "-" + String(LittleFS.totalBytes()) + "\"";
    server.sendHeader("ETag", etag);
    
    // Client ETag kontrolü
    if (server.hasHeader("If-None-Match")) {
        String clientEtag = server.header("If-None-Match");
        if (clientEtag == etag) {
            server.send(304); // Not Modified
            return;
        }
    }
    
    // Gzip kontrolü - ÖNCELİKLİ
    String pathWithGz = path + ".gz";
    if (LittleFS.exists(pathWithGz)) {
        File file = LittleFS.open(pathWithGz, "r");
        server.sendHeader("Content-Encoding", "gzip");
        server.sendHeader("Vary", "Accept-Encoding");
        server.streamFile(file, contentType);
        file.close();
        return;
    }

    // Normal dosya
    if (LittleFS.exists(path)) {
        File file = LittleFS.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return;
    }

    server.send(404, "text/plain", "404: Not Found");
}


String getUptime() {
    unsigned long sec = millis() / 1000;
    char buffer[32];
    sprintf(buffer, "%lu:%02lu:%02lu", sec/3600, (sec%3600)/60, sec%60);
    return String(buffer);
}

// API Handler'lar
void handleStatusAPI() {
    if (!checkSession()) {
        server.send(401, "text/plain", "Unauthorized");
        return;
    }
    
    JsonDocument doc;
    doc["datetime"] = getCurrentDateTime();
    doc["uptime"] = getUptime();
    doc["deviceName"] = settings.deviceName;
    doc["tmName"] = settings.transformerStation;
    doc["deviceIP"] = ETH.localIP().toString();
    doc["ethernetStatus"] = ETH.linkUp();
    doc["timeSynced"] = isTimeSynced();
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["totalHeap"] = ESP.getHeapSize();

    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handleGetSettingsAPI() {
    if (!checkSession()) { server.send(401); return; }
    JsonDocument doc;
    doc["deviceName"] = settings.deviceName;
    doc["tmName"] = settings.transformerStation;
    doc["username"] = settings.username;
    String output;
    serializeJson(doc, output);
    server.send(200, "application/json", output);
}

void handlePostSettingsAPI() {
    if (!checkSession()) { server.send(401); return; }
    if (saveSettings(server.arg("deviceName"), server.arg("tmName"), server.arg("username"), server.arg("password"))) {
        server.send(200, "text/plain", "OK");
    } else {
        server.send(400, "text/plain", "Error");
    }
}

// YENİ: Arıza sayısını al API'si
void handleGetFaultCountAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addLog("📊 Arıza sayısı sorgulanıyor", INFO, "API");
    
    int count = getTotalFaultCount(); // uart_handler.cpp'deki yeni fonksiyon
    
    JsonDocument doc;
    doc["success"] = (count > 0);
    doc["count"] = count;
    doc["message"] = count > 0 ? 
        "Toplam " + String(count) + " arıza bulundu" : 
        "Arıza sayısı alınamadı";
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// YENİ: Belirli bir arıza kaydını al
void handleGetSpecificFaultAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    String faultNoStr = server.arg("faultNo");
    if (faultNoStr.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"faultNo parameter required\"}");
        return;
    }
    
    int faultNo = faultNoStr.toInt();
    if (faultNo < 1 || faultNo > 9999) {
        server.send(400, "application/json", "{\"error\":\"Invalid fault number\"}");
        return;
    }
    
    addLog("🔍 Arıza " + String(faultNo) + " sorgulanıyor", INFO, "API");
    
    bool success = requestSpecificFault(faultNo);
    
    if (success) {
        String response = getLastFaultResponse();
        
        JsonDocument doc;
        doc["success"] = true;
        doc["faultNo"] = faultNo;
        doc["rawData"] = response;
        doc["length"] = response.length();
        
        String output;
        serializeJson(doc, output);
        
        server.send(200, "application/json", output);
    } else {
        server.send(500, "application/json", 
            "{\"success\":false,\"error\":\"Arıza kaydı alınamadı\"}");
    }
}
// dsPIC'teki arızaları sil API'si
void handleDeleteFaultsFromDsPICAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addLog("🗑️ dsPIC arıza silme isteği alındı", INFO, "API");
    
    // tT komutu gönder
    bool success = deleteAllFaultsFromDsPIC();
    
    JsonDocument doc;
    doc["success"] = success;
    
    if (success) {
        doc["message"] = "dsPIC33EP üzerindeki tüm arıza kayıtları başarıyla silindi";
        doc["command"] = "tT";
        
        addLog("✅ dsPIC arızaları silindi", SUCCESS, "API");
    } else {
        doc["message"] = "Arıza silme işlemi başarısız oldu";
        doc["error"] = "tT komutu yanıt vermedi veya hata döndü";
        
        addLog("❌ dsPIC arıza silme başarısız", ERROR, "API");
    }
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(success ? 200 : 500, "application/json", output);
}

// Son N arızayı al API'si
void handleGetLastNFaultsAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    // Kaç arıza isteniyor? (varsayılan 50)
    int count = 50;
    if (server.hasArg("count")) {
        count = server.arg("count").toInt();
        if (count < 1) count = 1;
        if (count > 100) count = 100; // Maksimum 100 ile sınırla
    }
    
    addLog("📥 Son " + String(count) + " arıza isteniyor", INFO, "API");
    
    std::vector<String> faultData;
    bool success = requestLastNFaults(count, faultData);
    
    JsonDocument doc;
    doc["success"] = success;
    doc["requestedCount"] = count;
    doc["receivedCount"] = faultData.size();
    
    if (success) {
        JsonArray faults = doc["faults"].to<JsonArray>();
        
        int faultIndex = faultData.size();
        for (const String& fault : faultData) {
            JsonObject faultObj = faults.add<JsonObject>();
            faultObj["index"] = faultIndex--;
            faultObj["rawData"] = fault;
            faultObj["length"] = fault.length();
        }
        
        doc["message"] = String(faultData.size()) + " arıza kaydı alındı";
    } else {
        doc["message"] = "Arıza kayıtları alınamadı";
        doc["error"] = "Sistemde arıza yok veya iletişim hatası";
    }
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(success ? 200 : 500, "application/json", output);
}

// Mevcut handleParsedFaultAPI fonksiyonunu GÜNCELLE
void handleParsedFaultAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    String action = server.arg("action");
    
    if (action == "count") {
        // Toplam arıza sayısını döndür
        int count = getTotalFaultCount();
        
        JsonDocument doc;
        doc["success"] = (count > 0);
        doc["count"] = count;
        doc["message"] = count > 0 ? 
            String(count) + " adet arıza bulundu" : 
            "Sistemde arıza kaydı yok";
        
        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
        
    } else if (action == "get") {
        // Belirli bir arıza kaydını al ve parse et
        String faultNoStr = server.arg("faultNo");
        if (faultNoStr.length() == 0) {
            server.send(400, "application/json", "{\"error\":\"faultNo parameter required\"}");
            return;
        }
        
        int faultNo = faultNoStr.toInt();
        if (requestSpecificFault(faultNo)) {
            String rawResponse = getLastFaultResponse();
            FaultRecord fault = parseFaultData(rawResponse);
            
            if (fault.isValid) {
                JsonDocument doc;
                doc["success"] = true;
                doc["faultNo"] = faultNo;
                doc["fault"]["pinNumber"] = fault.pinNumber;
                doc["fault"]["pinType"] = fault.pinType;
                doc["fault"]["pinName"] = fault.pinName;
                doc["fault"]["dateTime"] = fault.dateTime;
                doc["fault"]["duration"] = formatDuration(fault.duration);
                doc["fault"]["durationSeconds"] = fault.duration;
                doc["fault"]["millisecond"] = fault.millisecond;
                doc["fault"]["rawData"] = fault.rawData;
                
                String output;
                serializeJson(doc, output);
                server.send(200, "application/json", output);
            } else {
                server.send(400, "application/json", 
                    "{\"success\":false,\"error\":\"" + fault.errorMessage + "\"}");
            }
        } else {
            server.send(500, "application/json", 
                "{\"success\":false,\"error\":\"Arıza kaydı alınamadı\"}");
        }
        
    } else if (action == "clear") {
        // Arıza kayıtlarını temizle (sadece ESP32 tarafında)
        faultCount = 0;
        server.send(200, "application/json", 
            "{\"success\":true,\"message\":\"Arıza kayıtları temizlendi\"}");
            
    } else {
        server.send(400, "application/json", 
            "{\"error\":\"Invalid action. Use: count, get, or clear\"}");
    }
}

// ✅ handleUARTTestAPI fonksiyonu
void handleUARTTestAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    addLog("🧪 UART test başlatılıyor...", INFO, "WEB");
    
    JsonDocument doc;
    doc["uartHealthy"] = uartHealthy;
    doc["baudRate"] = 250000;
    
    // Basit test komutu gönder
    String testResponse;
    bool testResult = sendCustomCommand("TEST", testResponse, 2000);
    
    doc["testCommand"] = "TEST";
    doc["testSuccess"] = testResult;
    doc["testResponse"] = testResponse;
    doc["responseLength"] = testResponse.length();
    
    // İstatistikler
    doc["stats"]["sent"] = uartStats.totalFramesSent;
    doc["stats"]["received"] = uartStats.totalFramesReceived;
    doc["stats"]["errors"] = uartStats.frameErrors + uartStats.checksumErrors + uartStats.timeoutErrors;
    doc["stats"]["successRate"] = uartStats.successRate;
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// LED durumu API handler'ı - GÜNCELLENMİŞ VERSİYON
void handleGetLedStatusAPI() {
    if (!checkSession()) {
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return;
    }
    
    // "LN" komutuyla LED durumunu dsPIC'ten iste
    String ledResponse;
    bool success = sendCustomCommand("LN", ledResponse, 2000);
    
    JsonDocument doc;
    doc["success"] = success;
    doc["command"] = "LN";
    doc["response"] = ledResponse;
    doc["timestamp"] = getFormattedTimestamp();
    
    if (success && ledResponse.length() > 0) {
        // Parse LED data - Format: "L:AABBCC"
        // AA = Input byte (2 hex digits)
        // BB = Output byte (2 hex digits)
        // CC = Alarm byte (2 hex digits) - OPSİYONEL
        if (ledResponse.startsWith("L:")) {
            String hexData = ledResponse.substring(2);
            hexData.trim();

            // Minimum 4 karakter olmalı (AABB)
            if (hexData.length() >= 4) {
                // İlk 2 karakter Input, sonraki 2 karakter Output
                String inputHex = hexData.substring(0, 2);
                String outputHex = hexData.substring(2, 4);

                doc["parsed"]["valid"] = true;
                doc["parsed"]["rawData"] = hexData;
                doc["parsed"]["inputHex"] = inputHex;
                doc["parsed"]["outputHex"] = outputHex;

                // Hex string'i integer'a çevir
                long inputByte = strtol(inputHex.c_str(), NULL, 16);
                long outputByte = strtol(outputHex.c_str(), NULL, 16);

                doc["parsed"]["inputByte"] = inputByte;
                doc["parsed"]["outputByte"] = outputByte;

                // Eğer 6 veya daha fazla karakter varsa, alarm byte'ı da parse et
                long alarmByte = 0;
                if (hexData.length() >= 6) {
                    String alarmHex = hexData.substring(4, 6);
                    alarmByte = strtol(alarmHex.c_str(), NULL, 16);

                    doc["parsed"]["alarmHex"] = alarmHex;
                    doc["parsed"]["alarmByte"] = alarmByte;

                    // Alarm binary formatı
                    char alarmBinary[9];
                    for (int i = 0; i < 8; i++) {
                        alarmBinary[7-i] = (alarmByte & (1 << i)) ? '1' : '0';
                    }
                    alarmBinary[8] = '\0';
                    doc["parsed"]["alarmBinary"] = String(alarmBinary);

                    // Alarm detayları
                    JsonObject alarms = doc["parsed"]["alarms"].to<JsonObject>();
                    alarms["ntp"] = (alarmByte & 0x40) != 0;       // Bit 6
                    alarms["dc2"] = (alarmByte & 0x20) != 0;       // Bit 5
                    alarms["dc1"] = (alarmByte & 0x10) != 0;       // Bit 4
                    alarms["rs232"] = ((alarmByte & 0x02) != 0) ||
                                     ((alarmByte & 0x04) != 0) ||
                                     ((alarmByte & 0x08) != 0);    // Bit 1, 2, 3
                    alarms["general"] = alarms["ntp"] || alarms["dc2"] ||
                                       alarms["dc1"] || alarms["rs232"];
                } else {
                    // Eski format, alarm yok
                    doc["parsed"]["alarmHex"] = "00";
                    doc["parsed"]["alarmByte"] = 0;

                    JsonObject alarms = doc["parsed"]["alarms"].to<JsonObject>();
                    alarms["ntp"] = false;
                    alarms["dc2"] = false;
                    alarms["dc1"] = false;
                    alarms["rs232"] = false;
                    alarms["general"] = false;
                }

                // Binary formatlarını da ekle (debug için)
                char inputBinary[9];
                char outputBinary[9];
                for (int i = 0; i < 8; i++) {
                    inputBinary[7-i] = (inputByte & (1 << i)) ? '1' : '0';
                    outputBinary[7-i] = (outputByte & (1 << i)) ? '1' : '0';
                }
                inputBinary[8] = '\0';
                outputBinary[8] = '\0';

                doc["parsed"]["inputBinary"] = String(inputBinary);
                doc["parsed"]["outputBinary"] = String(outputBinary);

                // Her bir LED'in durumunu hesapla
                JsonArray inputs = doc["parsed"]["inputs"].to<JsonArray>();
                int activeInputs = 0;
                for (int i = 0; i < 8; i++) {
                    bool isOn = (inputByte & (1 << i)) != 0;
                    inputs.add(isOn);
                    if (isOn) activeInputs++;
                }

                JsonArray outputs = doc["parsed"]["outputs"].to<JsonArray>();
                int activeOutputs = 0;
                for (int i = 0; i < 8; i++) {
                    bool isOn = (outputByte & (1 << i)) != 0;
                    outputs.add(isOn);
                    if (isOn) activeOutputs++;
                }

                doc["parsed"]["activeInputs"] = activeInputs;
                doc["parsed"]["activeOutputs"] = activeOutputs;

                // Log ekle (alarm bilgisiyle)
                String logMsg = "LED durumu: IN=" + String(activeInputs) + "/8, OUT=" +
                               String(activeOutputs) + "/8";
                if (alarmByte != 0) {
                    logMsg += ", ALARM=0x" + String((int)alarmByte, HEX);
                }
                logMsg += " [" + hexData + "]";
                addLog(logMsg, INFO, "LED");

            } else {
                doc["parsed"]["valid"] = false;
                doc["parsed"]["error"] = "Data too short (expected at least 4 chars)";
            }
        } else {
            doc["parsed"]["valid"] = false;
            doc["parsed"]["error"] = "Invalid format (expected L:XXXX or L:XXXXXX)";
        }
    } else {
        doc["parsed"]["valid"] = false;
        doc["parsed"]["error"] = "No response from dsPIC";
    }
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

void handleGetNtpAPI() {
    if (!checkSession()) { 
        server.send(401); 
        return; 
    }
    
    addLog("🌐 NTP ayarları sorgulanıyor", DEBUG, "API");
    
     // dsPIC'ten güncel NTP ayarlarını al
    String ntp1_from_dspic = "";
    String ntp2_from_dspic = "";
    
    bool dspicSuccess = requestNTPFromDsPIC(ntp1_from_dspic, ntp2_from_dspic);
    
    if (dspicSuccess) {
        addLog("✅ dsPIC'ten NTP alındı: NTP1=" + ntp1_from_dspic + ", NTP2=" + ntp2_from_dspic, SUCCESS, "API");
        
        // Global config'i güncelle
        ntp1_from_dspic.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
        ntp2_from_dspic.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
        
        // Preferences'a da kaydet
        Preferences preferences;
        preferences.begin("ntp-config", false);
        preferences.putString("ntp_server1", ntp1_from_dspic);
        preferences.putString("ntp_server2", ntp2_from_dspic);
        preferences.end();
    } else {
        addLog("⚠️ dsPIC'ten NTP alınamadı, lokal değerler kullanılıyor", WARN, "API");
        
        Preferences preferences;
        preferences.begin("ntp-config", true);
        ntp1_from_dspic = preferences.getString("ntp_server1", "192.168.3.2");
        ntp2_from_dspic = preferences.getString("ntp_server2", "8.8.8.8");
        preferences.end();
    }
    
    // Network ayarlarını da oku
    Preferences preferences;
    preferences.begin("ntp-config", true);
    String subnet = preferences.getString("subnet", "255.255.255.0");
    String gateway = preferences.getString("gateway", "192.168.3.1");
    String dns = preferences.getString("dns", "8.8.8.8");
    preferences.end();
    
    // JSON yanıtı hazırla
    JsonDocument doc;
    doc["ntpServer1"] = ntp1_from_dspic;
    doc["ntpServer2"] = ntp2_from_dspic;
    doc["subnet"] = subnet;           // YENİ
    doc["gateway"] = gateway;         // YENİ
    doc["dns"] = dns;                 // YENİ
    doc["timezone"] = ntpConfig.timezone;
    doc["enabled"] = ntpConfig.enabled;
    doc["configured"] = ntpConfigured;
    doc["syncStatus"] = dspicSuccess ? "synced" : "local";
    
    String output;
    serializeJson(doc, output);
    
    addLog("📤 NTP ayarları gönderildi: " + output, DEBUG, "API");
    
    server.send(200, "application/json", output);
}

// ✅ BU FONKSİYONU DA BULUN VE DEĞİŞTİRİN:
void handlePostNtpAPI() {
    if (!checkSession()) { 
        server.send(401); 
        return; 
    }
    
    String server1 = server.arg("ntpServer1");
    String server2 = server.arg("ntpServer2");
    String subnet = server.arg("ntpSubnet");      // YENİ
    String gateway = server.arg("ntpGateway");    // YENİ
    String dns = server.arg("ntpDNS");            // YENİ
    
    addLog("📝 NTP ayarları kaydediliyor: NTP1=" + server1 + 
           ", Subnet=" + subnet + ", Gateway=" + gateway + ", DNS=" + dns, INFO, "API");
    
    // YENİ: 5 parametreli fonksiyon çağrısı
    if (saveNTPSettings(server1, server2, subnet, gateway, dns, 3)) {
        // Başarılı kayıt sonrası backend'e gönder (zaten saveNTPSettings içinde yapılıyor)
        
        // Kontrol için tekrar oku
        Preferences preferences;
        preferences.begin("ntp-config", true);
        String checkServer1 = preferences.getString("ntp_server1", "");
        String checkServer2 = preferences.getString("ntp_server2", "");
        String checkSubnet = preferences.getString("subnet", "");
        String checkGateway = preferences.getString("gateway", "");
        String checkDNS = preferences.getString("dns", "");
        preferences.end();
        
        addLog("✅ NTP kayıt kontrolü - NTP1: " + checkServer1 + ", NTP2: " + checkServer2, SUCCESS, "API");
        
        JsonDocument doc;
        doc["success"] = true;
        doc["message"] = "NTP ve network ayarları kaydedildi";
        doc["ntpServer1"] = checkServer1;
        doc["ntpServer2"] = checkServer2;
        doc["subnet"] = checkSubnet;
        doc["gateway"] = checkGateway;
        doc["dns"] = checkDNS;
        
        String output;
        serializeJson(doc, output);
        
        server.send(200, "application/json", output);
    } else {
        server.send(400, "application/json", "{\"success\":false,\"error\":\"NTP ayarları kaydedilemedi\"}");
    }
}


// Baudrate değiştirme
void handlePostBaudRateAPI() {
    if (!checkSession()) { 
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return; 
    }
    
    String baudStr = server.arg("baud");
    if (baudStr.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"Baudrate parametresi eksik\"}");
        return;
    }
    
    long newBaudRate = baudStr.toInt();
    
    addLog("⚙️ Baudrate değişikliği: " + String(newBaudRate) + " bps", INFO, "API");
    
    if (changeBaudRate(newBaudRate)) {
        // Başarılı değişiklik sonrası ayarları kaydet
        settings.currentBaudRate = newBaudRate;
        
        // Preferences ile kaydet
        Preferences prefs;
        prefs.begin("app-settings", false);
        prefs.putLong("baudRate", newBaudRate);
        prefs.end();
        
        JsonDocument doc;
        doc["success"] = true;
        doc["newBaudRate"] = newBaudRate;
        doc["message"] = "Baudrate başarıyla değiştirildi";
        
        String output;
        serializeJson(doc, output);
        server.send(200, "application/json", output);
    } else {
        server.send(500, "application/json", 
            "{\"success\":false,\"error\":\"Baudrate değiştirilemedi\"}");
    }
}

// Mevcut baudrate'i dsPIC'ten al
void handleGetCurrentBaudRateAPI() {
    if (!checkSession()) { 
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return; 
    }
    
    addLog("📡 Mevcut baudrate dsPIC'ten sorgulanıyor", INFO, "API");
    
    int currentBaud = getCurrentBaudRateFromDsPIC(); // uart_handler.cpp'deki yeni fonksiyon
    
    JsonDocument doc;
    if (currentBaud > 0) {
        doc["success"] = true;
        doc["currentBaudRate"] = currentBaud;
        doc["espBaudRate"] = 250000; // ESP32 her zaman 250000'de çalışıyor
        doc["message"] = "Baudrate başarıyla alındı";
        
        // Mevcut değeri storage'a da kaydet
        settings.currentBaudRate = currentBaud;
    } else {
        doc["success"] = false;
        doc["currentBaudRate"] = -1;
        doc["espBaudRate"] = 250000;
        doc["message"] = "dsPIC'ten baudrate bilgisi alınamadı";
    }
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// Password change sayfası için token kontrolü (ama atmaz)
void handlePasswordChangeCheck() {
    String token = "";
    if (server.hasHeader("Authorization")) {
        String authHeader = server.header("Authorization");
        if (authHeader.startsWith("Bearer ")) {
            token = authHeader.substring(7);
        }
    }
    
    // Token yoksa veya geçersizse sadece uyarı döndür
    if (token.length() == 0 || settings.sessionToken.length() == 0 || token != settings.sessionToken) {
        server.send(200, "application/json", "{\"validSession\":false,\"message\":\"Oturum geçersiz ama devam edebilirsiniz\"}");
    } else {
        server.send(200, "application/json", "{\"validSession\":true}");
    }
}

void handleGetLogsAPI() {
    if (!checkSession()) { 
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return; 
    }
    
    // Sayfa numarasını al (varsayılan 1)
    int pageNumber = 1;
    if (server.hasArg("page")) {
        pageNumber = server.arg("page").toInt();
        if (pageNumber < 1) pageNumber = 1;
    }
    
    // Filtre parametrelerini al (opsiyonel)
    String levelFilter = server.arg("level");  // all, ERROR, WARN, INFO, etc.
    String sourceFilter = server.arg("source"); // all veya belirli kaynak
    String searchFilter = server.arg("search"); // Arama terimi
    
    JsonDocument doc;
    
    // Pagination bilgileri
    doc["currentPage"] = pageNumber;
    doc["pageSize"] = PAGE_SIZE;
    doc["totalLogs"] = getTotalLogCount();
    doc["totalPages"] = getTotalPageCount();
    
    // Sayfa loglarını al
    std::vector<LogEntry> pageLogs = getLogsPage(pageNumber);
    
    JsonArray logArray = doc["logs"].to<JsonArray>();
    
    for (const auto& log : pageLogs) {
        // Filtreleme kontrolü
        bool includeLog = true;
        
        // Level filtresi
        if (levelFilter.length() > 0 && levelFilter != "all") {
            if (logLevelToString(log.level) != levelFilter) {
                includeLog = false;
            }
        }
        
        // Source filtresi  
        if (includeLog && sourceFilter.length() > 0 && sourceFilter != "all") {
            if (log.source != sourceFilter) {
                includeLog = false;
            }
        }
        
        // Arama filtresi
        if (includeLog && searchFilter.length() > 0) {
            String searchLower = searchFilter;
            searchLower.toLowerCase();
            String messageLower = log.message;
            messageLower.toLowerCase();
            String sourceLower = log.source;
            sourceLower.toLowerCase();
            
            if (messageLower.indexOf(searchLower) == -1 && 
                sourceLower.indexOf(searchLower) == -1) {
                includeLog = false;
            }
        }
        
        if (includeLog) {
            JsonObject logEntry = logArray.add<JsonObject>();
            logEntry["t"] = log.timestamp;
            logEntry["m"] = log.message;
            logEntry["l"] = logLevelToString(log.level);
            logEntry["s"] = log.source;
            logEntry["id"] = log.millis_time; // Unique ID için millis kullan
        }
    }
    
    // İstatistikler
    int errorCount = 0, warnCount = 0, infoCount = 0, successCount = 0;
    for (const auto& log : logStorage) {
        switch(log.level) {
            case ERROR: errorCount++; break;
            case WARN: warnCount++; break;
            case INFO: infoCount++; break;
            case SUCCESS: successCount++; break;
        }
    }
    
    doc["stats"]["errorCount"] = errorCount;
    doc["stats"]["warnCount"] = warnCount;
    doc["stats"]["infoCount"] = infoCount;
    doc["stats"]["successCount"] = successCount;
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
}

// handleClearLogsAPI fonksiyonunu güncelle - GERÇEKTEN TEMİZLEYECEK
void handleClearLogsAPI() {
    if (!checkSession()) { 
        server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
        return; 
    }
    
    // Temizlemeden önce log sayısını kaydet
    int previousLogCount = getTotalLogCount();
    
    // GERÇEKTEN TÜM LOGLARI TEMİZLE
    clearLogs();
    
    // Başarılı yanıt
    JsonDocument doc;
    doc["success"] = true;
    doc["message"] = String(previousLogCount) + " log kaydı hafızadan temizlendi";
    doc["previousCount"] = previousLogCount;
    doc["currentCount"] = getTotalLogCount();
    
    String output;
    serializeJson(doc, output);
    
    addSecurityHeaders();
    server.send(200, "application/json", output);
    
    // Temizleme işlemini logla
    addLog("✅ " + String(previousLogCount) + " log kaydı kullanıcı tarafından temizlendi", SUCCESS, "SYSTEM");
}

void setupWebRoutes() {
    
    server.on("/favicon.ico", HTTP_GET, []() { server.send(204); });
    
    // ANA SAYFALAR (Oturum kontrolü yok, JS halledecek)
    server.on("/", HTTP_GET, []() { serveStaticFile("/index.html", "text/html"); });
    server.on("/login.html", HTTP_GET, []() { serveStaticFile("/login.html", "text/html"); });
    server.on("/password_change.html", HTTP_GET, []() { serveStaticFile("/password_change.html", "text/html"); });
    
    // STATİK DOSYALAR
    server.on("/style.css", HTTP_GET, []() { serveStaticFile("/style.css", "text/css"); });
    server.on("/script.js", HTTP_GET, []() { serveStaticFile("/script.js", "application/javascript"); });
    server.on("/login.js", HTTP_GET, []() { serveStaticFile("/login.js", "application/javascript"); });

    // SPA SAYFA PARÇALARI (Oturum kontrolü GEREKLİ)
    server.on("/pages/dashboard.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/dashboard.html", "text/html"); else server.send(401); });
    server.on("/pages/network.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/network.html", "text/html"); else server.send(401); });
    server.on("/pages/systeminfo.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/systeminfo.html", "text/html"); else server.send(401); });
    server.on("/pages/ntp.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/ntp.html", "text/html"); else server.send(401); });
    server.on("/pages/baudrate.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/baudrate.html", "text/html"); else server.send(401); });
    server.on("/pages/fault.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/fault.html", "text/html"); else server.send(401); });
    server.on("/pages/log.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/log.html", "text/html"); else server.send(401); });
    server.on("/pages/datetime.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/datetime.html", "text/html"); else server.send(401); });
    server.on("/pages/account.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/account.html", "text/html"); else server.send(401); });
    server.on("/pages/backup.html", HTTP_GET, []() { if(checkSession()) serveStaticFile("/pages/backup.html", "text/html"); else server.send(401); });

    // KİMLİK DOĞRULAMA
    server.on("/login", HTTP_POST, handleUserLogin);
    server.on("/logout", HTTP_GET, handleUserLogout);

    // API ENDPOINT'LERİ

    // Device Info (Auth gerekmez)
    server.on("/api/device-info", HTTP_GET, handleDeviceInfoAPI);
    
    // System Info (Auth gerekli)
    server.on("/api/system-info", HTTP_GET, handleSystemInfoAPI);

    // Network Configuration
    server.on("/api/network", HTTP_GET, handleGetNetworkAPI);
    server.on("/api/network", HTTP_POST, handlePostNetworkAPI);

    // Notifications
    server.on("/api/notifications", HTTP_GET, handleNotificationAPI);
    
    // System Reboot
    server.on("/api/system/reboot", HTTP_POST, handleSystemRebootAPI);

    server.on("/api/status", HTTP_GET, handleStatusAPI);
    server.on("/api/settings", HTTP_GET, handleGetSettingsAPI);
    server.on("/api/settings", HTTP_POST, handlePostSettingsAPI);
    server.on("/api/ntp", HTTP_GET, handleGetNtpAPI);
    server.on("/api/ntp", HTTP_POST, handlePostNtpAPI);
    server.on("/api/baudrate/current", HTTP_GET, handleGetCurrentBaudRateAPI);  // Mevcut baudrate sorgula
    server.on("/api/baudrate", HTTP_POST, handlePostBaudRateAPI);   // Baudrate değiştir
    server.on("/api/logs", HTTP_GET, handleGetLogsAPI);
    server.on("/api/logs/clear", HTTP_POST, handleClearLogsAPI);
    // DateTime API endpoints
    server.on("/api/datetime", HTTP_GET, handleGetDateTimeAPI);
    server.on("/api/datetime/fetch", HTTP_POST, handleFetchDateTimeAPI);
    server.on("/api/datetime/set", HTTP_POST, handleSetDateTimeAPI);
    // ✅ UART Test API'si ekle
    server.on("/api/uart/test", HTTP_GET, handleUARTTestAPI);
    // ✅ LED API'si ekle
    server.on("/api/led/status", HTTP_GET, handleGetLedStatusAPI);

    // YENİ route'ları EKLE:
    server.on("/api/faults/count", HTTP_GET, handleGetFaultCountAPI);
    server.on("/api/faults/get", HTTP_POST, handleGetSpecificFaultAPI);
    server.on("/api/faults/parsed", HTTP_POST, handleParsedFaultAPI); // Güncellendi

     // ✅ Fault komutları için debug endpoint'leri
    server.on("/api/uart/send", HTTP_POST, []() {
        if (!checkSession()) {
            server.send(401, "application/json", "{\"error\":\"Unauthorized\"}");
            return;
        }
        
        String command = server.arg("command");
        if (command.length() == 0) {
            server.send(400, "application/json", "{\"error\":\"Command parameter required\"}");
            return;
        }
        
        addLog("🧪 Manuel komut gönderiliyor: " + command, INFO, "UART");
        
        String response;
        bool success = sendCustomCommand(command, response, 3000);
        
        JsonDocument doc;
        doc["command"] = command;
        doc["success"] = success;
        doc["response"] = response;
        doc["responseLength"] = response.length();
        doc["timestamp"] = getFormattedTimestamp();
        
        String output;
        serializeJson(doc, output);
        
        server.send(200, "application/json", output);
    });

    // Arıza silme API'si
    server.on("/api/faults/delete", HTTP_POST, handleDeleteFaultsFromDsPICAPI);
    
    // Son N arızayı al API'si
    server.on("/api/faults/last", HTTP_GET, handleGetLastNFaultsAPI);

    
    server.on("/api/backup/download", HTTP_GET, handleBackupDownload);
    // Yedek yükleme için doğru handler tanımı
    server.on("/api/backup/upload", HTTP_POST, 
        []() { server.send(200, "text/plain", "OK"); }, // Önce bir OK yanıtı gönderilir
        handleBackupUpload // Sonra dosya yükleme işlenir
    );
    server.on("/api/change-password", HTTP_POST, handlePasswordChangeAPI);

    // Password Change Check (soft check)
    server.on("/api/check-password-session", HTTP_GET, handlePasswordChangeCheck);
    
    // Her response'ta security headers ekle
    server.onNotFound([]() {
        addSecurityHeaders();
        addLog("404 isteği: " + server.uri(), WARN, "WEB");
        server.send(404, "application/json", "{\"error\":\"Not Found\"}");
    });
    
    server.begin();
    addLog("✅ Web sunucu başlatıldı", SUCCESS, "WEB");
}