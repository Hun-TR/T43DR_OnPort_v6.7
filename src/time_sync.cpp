// src/time_sync.cpp
#include "time_sync.h"
#include "uart_handler.h"
#include "log_system.h"
#include <Arduino.h>
#include <time.h>

// Global zaman verisi
TimeData timeData;

static bool parseDNResponse(const String& response) {
    // Beklenen format: "D:22/02/25 11:22:33"
    if (!response.startsWith("D:")) {
        addLog("❌ Geçersiz zaman formatı (prefix yok): " + response, ERROR, "TIME");
        return false;
    }

    String payload = response.substring(2); // "22/02/25 11:22:33"

    int spaceIndex = payload.indexOf(' ');
    if (spaceIndex == -1) {
        addLog("❌ Geçersiz zaman formatı (boşluk yok): " + response, ERROR, "TIME");
        return false;
    }

    String datePart = payload.substring(0, spaceIndex);   // "22/02/25"
    String timePart = payload.substring(spaceIndex + 1);  // "11:22:33"

    // Tarih parçala
    int d, m, y;
    if (sscanf(datePart.c_str(), "%d/%d/%d", &d, &m, &y) != 3) {
        addLog("❌ Tarih parse hatası: " + datePart, ERROR, "TIME");
        return false;
    }

    // Yılı 2000'le tamamla
    y += 2000;

    // Saat parçala
    int hh, mm, ss;
    if (sscanf(timePart.c_str(), "%d:%d:%d", &hh, &mm, &ss) != 3) {
        addLog("❌ Saat parse hatası: " + timePart, ERROR, "TIME");
        return false;
    }

    // Mevcut ESP32 saatini al
    struct tm currentTime;
    bool hasCurrentTime = getLocalTime(&currentTime, 10);
    
    // Yeni zaman bilgisini oluştur
    char bufDate[11];
    char bufTime[9];
    snprintf(bufDate, sizeof(bufDate), "%04d-%02d-%02d", y, m, d);
    snprintf(bufTime, sizeof(bufTime), "%02d:%02d:%02d", hh, mm, ss);

    String newDate = String(bufDate);
    String newTime = String(bufTime);
    
    // Saat farkını kontrol et (sadece farklıysa güncelle)
    bool needUpdate = false;
    
    if (hasCurrentTime) {
        // ESP32'nin mevcut saatini al
        char espDate[11];
        char espTime[9];
        strftime(espDate, sizeof(espDate), "%Y-%m-%d", &currentTime);
        strftime(espTime, sizeof(espTime), "%H:%M:%S", &currentTime);
        
        // Tarih veya saat farklıysa güncelleme gerekli
        if (String(espDate) != newDate || String(espTime) != newTime) {
            // Saat farkını hesapla (saniye cinsinden)
            struct tm newTm;
            memset(&newTm, 0, sizeof(newTm));
            newTm.tm_year = y - 1900;
            newTm.tm_mon = m - 1;
            newTm.tm_mday = d;
            newTm.tm_hour = hh;
            newTm.tm_min = mm;
            newTm.tm_sec = ss;
            
            time_t currentTimestamp = mktime(&currentTime);
            time_t newTimestamp = mktime(&newTm);
            int diffSeconds = abs((int)(newTimestamp - currentTimestamp));
            
            // 2 saniyeden fazla fark varsa güncelle
            if (diffSeconds > 2) {
                needUpdate = true;
                addLog("⚠️ Saat farkı tespit edildi: " + String(diffSeconds) + " saniye", WARN, "TIME");
            }
        }
    } else {
        // ESP32 saati yoksa her zaman güncelle
        needUpdate = true;
    }
    
    // timeData'yı güncelle
    timeData.lastDate = newDate;
    timeData.lastTime = newTime;
    timeData.isValid = true;
    
    // Gerekirse ESP32 sistem saatini güncelle
    if (needUpdate) {
        updateSystemTime();
        addLog("✅ ESP32 saati dsPIC ile senkronize edildi: " + newDate + " " + newTime, SUCCESS, "TIME");
    } else {
        addLog("ℹ️ Saat senkron, güncelleme gerekmedi", DEBUG, "TIME");
    }
    
    return true;
}

bool requestTimeFromDsPIC() {
    String response;
    if (!sendCustomCommand("DN", response, 2000)) {
        timeData.failCount++;
        // Her seferinde log yazmayalım, sadece ilk hatada
        if (timeData.failCount == 1) {
            addLog("❌ dsPIC'ten zaman bilgisi alınamadı", ERROR, "TIME");
        }
        return false;
    }

    if (!parseDNResponse(response)) {
        timeData.failCount++;
        return false;
    }

    timeData.lastSync = millis();
    timeData.syncCount++;
    timeData.failCount = 0;

    return true;
}

void updateSystemTime() {
    if (!timeData.isValid) return;

    struct tm tm;
    memset(&tm, 0, sizeof(tm));

    // "YYYY-MM-DD"
    sscanf(timeData.lastDate.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
    tm.tm_year -= 1900;   // struct tm yılı 1900'dan başlar
    tm.tm_mon  -= 1;      // struct tm ay 0–11

    // "HH:MM:SS"
    sscanf(timeData.lastTime.c_str(), "%d:%d:%d", &tm.tm_hour, &tm.tm_min, &tm.tm_sec);

    time_t t = mktime(&tm);
    struct timeval now = { .tv_sec = t, .tv_usec = 0 };
    settimeofday(&now, NULL);

    addLog("⏰ ESP32 sistem saati güncellendi: " + timeData.lastDate + " " + timeData.lastTime, INFO, "TIME");
}

// Şu anki tarihi string olarak döndür (YYYY-MM-DD)
String getCurrentDate() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        char buffer[11];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d", &timeinfo);
        return String(buffer);
    }
    return "0000-00-00";
}

// Şu anki tarih+saati string olarak döndür (YYYY-MM-DD HH:MM:SS)
String getCurrentDateTime() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        char buffer[20];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buffer);
    }
    return "0000-00-00 00:00:00";
}

// Zaman senkronize mi?
bool isTimeSynced() {
    return timeData.isValid;
}

// Otomatik kontrol - 5 dakikada bir
void checkTimeSync() {
    static unsigned long lastCheck = 0;
    const unsigned long interval = 300000; // 5 dakika (300 saniye)

    if (millis() - lastCheck >= interval) {
        lastCheck = millis();
        
        // Sessizce kontrol et (log spam'i önlemek için)
        requestTimeFromDsPIC();
    }
}

// İlk başlangıçta hemen senkronize et
void initTimeSync() {
    addLog("🕐 Zaman senkronizasyonu başlatılıyor...", INFO, "TIME");
    
    // İlk senkronizasyonu yap
    if (requestTimeFromDsPIC()) {
        addLog("✅ İlk zaman senkronizasyonu başarılı", SUCCESS, "TIME");
    } else {
        addLog("⚠️ İlk zaman senkronizasyonu başarısız, daha sonra tekrar denenecek", WARN, "TIME");
    }
}
