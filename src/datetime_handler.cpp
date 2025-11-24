#include "datetime_handler.h"
#include "uart_handler.h"
#include "log_system.h"
#include <time.h>

// Global datetime verisi
DateTimeData datetimeData = {
    .rawData = "",
    .date = "",
    .time = "",
    .lastUpdate = 0,
    .isValid = false
};

// Komut geçmişi (son 10 komut)
static CommandHistory commandHistory[10];
static int historyIndex = 0;
static int historyCount = 0;

// dsPIC'ten tarih-saat bilgisi iste ('DN' komutu)
bool requestDateTimeFromDsPIC() {
    String response;
    
    // 'DN' komutunu gönder
    if (!sendCustomCommand("DN", response, 3000)) {
        addLog("❌ dsPIC'ten tarih-saat bilgisi alınamadı", ERROR, "DATETIME");
        addCommandToHistory("DN", false, "Timeout/Error");
        return false;
    }
    
    addLog("dsPIC'ten yanıt alındı: " + response, DEBUG, "DATETIME");
    
    // Yanıtı parse et
    if (parseeDateTimeResponse(response)) {
        datetimeData.lastUpdate = millis();
        datetimeData.isValid = true;
        addLog("✅ Tarih-saat bilgisi güncellendi", SUCCESS, "DATETIME");
        addCommandToHistory("DN", true, response);
        return true;
    } else {
        addLog("❌ Geçersiz tarih-saat formatı: " + response, ERROR, "DATETIME");
        addCommandToHistory("DN", false, "Invalid format: " + response);
        return false;
    }
}

// dsPIC'ten gelen yanıtı parse et
// Beklenen format: "D:22/02/25 11:22:33"
bool parseeDateTimeResponse(const String& response) {
    if (response.length() < 10) {
        return false;
    }
    
    // Raw datayı sakla
    datetimeData.rawData = response;
    
    // "D:" prefix'ini kontrol et
    if (response.startsWith("D:")) {
        String dateTimeStr = response.substring(2); // "D:" kısmını at
        dateTimeStr.trim();
        
        // Format: "22/02/25 11:22:33"
        int spaceIndex = dateTimeStr.indexOf(' ');
        if (spaceIndex > 0 && spaceIndex < dateTimeStr.length() - 1) {
            String dateStr = dateTimeStr.substring(0, spaceIndex);    // "22/02/25"
            String timeStr = dateTimeStr.substring(spaceIndex + 1);   // "11:22:33"
            
            // Tarih formatını kontrol et (DD/MM/YY)
            if (dateStr.length() == 8 && dateStr.charAt(2) == '/' && dateStr.charAt(5) == '/') {
                // Saat formatını kontrol et (HH:MM:SS)
                if (timeStr.length() == 8 && timeStr.charAt(2) == ':' && timeStr.charAt(5) == ':') {
                    datetimeData.date = formatDateForDisplay(dateStr);
                    datetimeData.time = formatTimeForDisplay(timeStr);
                    return true;
                }
            }
        }
    }
    
    return false;
}

// dsPIC'e tarih ve saat ayarı gönder
bool setDateTimeToDsPIC(const String& date, const String& time) {
    if (!validateDateTime(date, time)) {
        addLog("❌ Geçersiz tarih veya saat formatı", ERROR, "DATETIME");
        return false;
    }
    
    // Önce saat komutunu hazırla ve gönder
    String timeCommand = formatTimeCommand(time);
    String timeResponse;
    
    addLog("Saat ayarlama komutu gönderiliyor: " + timeCommand, INFO, "DATETIME");
    
    if (!sendCustomCommand(timeCommand, timeResponse, 2000)) {
        addLog("❌ Saat ayarlama komutu gönderilirken hata", ERROR, "DATETIME");
        addCommandToHistory(timeCommand, false, "Timeout/Error");
        return false;
    }
    
    addCommandToHistory(timeCommand, true, timeResponse);
    addLog("Saat komutu yanıtı: " + timeResponse, DEBUG, "DATETIME");
    
    // Kısa bir bekleme
    delay(500);
    
    // Sonra tarih komutunu hazırla ve gönder
    String dateCommand = formatDateCommand(date);
    String dateResponse;
    
    addLog("Tarih ayarlama komutu gönderiliyor: " + dateCommand, INFO, "DATETIME");
    
    if (!sendCustomCommand(dateCommand, dateResponse, 2000)) {
        addLog("❌ Tarih ayarlama komutu gönderilirken hata", ERROR, "DATETIME");
        addCommandToHistory(dateCommand, false, "Timeout/Error");
        return false;
    }
    
    addCommandToHistory(dateCommand, true, dateResponse);
    addLog("Tarih komutu yanıtı: " + dateResponse, DEBUG, "DATETIME");
    
    addLog("✅ Tarih-saat ayarlama tamamlandı", SUCCESS, "DATETIME");
    
    // Ayarlama sonrası kontrol et
    delay(1000);
    requestDateTimeFromDsPIC();
    
    return true;
}

// Saat komutunu formatla: "11:22:33" -> "112233c"
String formatTimeCommand(const String& time) {
    if (time.length() != 8) return "";
    
    String hours = time.substring(0, 2);
    String minutes = time.substring(3, 5);
    String seconds = time.substring(6, 8);
    
    return hours + minutes + seconds + "c";
}

// Tarih komutunu formatla: "2025-02-27" -> "270225f"
String formatDateCommand(const String& date) {
    if (date.length() != 10) return "";
    
    String year = date.substring(2, 4);   // "25" (son iki hanesi)
    String month = date.substring(5, 7);  // "02"
    String day = date.substring(8, 10);   // "27"
    
    return day + month + year + "f";
}

// Tarih-saat validasyonu
bool validateDateTime(const String& date, const String& time) {
    // Tarih formatı: YYYY-MM-DD (2025-02-27)
    if (date.length() != 10 || date.charAt(4) != '-' || date.charAt(7) != '-') {
        return false;
    }
    
    // Saat formatı: HH:MM:SS (11:22:33)
    if (time.length() != 8 || time.charAt(2) != ':' || time.charAt(5) != ':') {
        return false;
    }
    
    // Sayısal kontrol
    int year = date.substring(0, 4).toInt();
    int month = date.substring(5, 7).toInt();
    int day = date.substring(8, 10).toInt();
    
    int hour = time.substring(0, 2).toInt();
    int minute = time.substring(3, 5).toInt();
    int second = time.substring(6, 8).toInt();
    
    // Temel aralık kontrolleri
    if (year < 2020 || year > 2099) return false;
    if (month < 1 || month > 12) return false;
    if (day < 1 || day > 31) return false;
    if (hour < 0 || hour > 23) return false;
    if (minute < 0 || minute > 59) return false;
    if (second < 0 || second > 59) return false;
    
    return true;
}

// ESP32'nin mevcut tarih-saatini al
String getCurrentESP32DateTime() {
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
        return String(buffer);
    } else {
        // NTP yoksa millis bazlı zaman
        unsigned long sec = millis() / 1000;
        unsigned long min = sec / 60;
        unsigned long hour = min / 60;
        
        char buffer[32];
        sprintf(buffer, "Uptime: %02lu:%02lu:%02lu", hour % 24, min % 60, sec % 60);
        return String(buffer);
    }
}

// ESP32 saati ile senkronize et
bool syncWithESP32Time() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo, 10)) {
        addLog("❌ ESP32 sistem saati alınamadı", ERROR, "DATETIME");
        return false;
    }
    
    // ESP32 saatini al
    char dateBuffer[16];
    char timeBuffer[16];
    strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%d", &timeinfo);
    strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &timeinfo);
    
    String espDate = String(dateBuffer);
    String espTime = String(timeBuffer);
    
    addLog("ESP32 saati ile senkronizasyon: " + espDate + " " + espTime, INFO, "DATETIME");
    
    return setDateTimeToDsPIC(espDate, espTime);
}

// Komut geçmişine ekle
void addCommandToHistory(const String& command, bool success, const String& response) {
    commandHistory[historyIndex].command = command;
    commandHistory[historyIndex].success = success;
    commandHistory[historyIndex].response = response;
    
    // Timestamp ekle
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 10)) {
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
        commandHistory[historyIndex].timestamp = String(buffer);
    } else {
        commandHistory[historyIndex].timestamp = String(millis() / 1000) + "s";
    }
    
    historyIndex = (historyIndex + 1) % 10;
    if (historyCount < 10) {
        historyCount++;
    }
}

// Komut geçmişini JSON olarak döndür
String getCommandHistoryJSON() {
    String json = "[";
    
    for (int i = 0; i < historyCount; i++) {
        int idx = (historyIndex - 1 - i + 10) % 10;
        
        if (i > 0) json += ",";
        
        json += "{";
        json += "\"command\":\"" + commandHistory[idx].command + "\",";
        json += "\"timestamp\":\"" + commandHistory[idx].timestamp + "\",";
        json += "\"success\":" + String(commandHistory[idx].success ? "true" : "false") + ",";
        json += "\"response\":\"" + commandHistory[idx].response + "\"";
        json += "}";
    }
    
    json += "]";
    return json;
}

// Görüntüleme için tarih formatla: "25/09/17" (YY/MM/DD) -> "17/09/2025" (DD/MM/YYYY)
String formatDateForDisplay(const String& rawDate) {
    if (rawDate.length() != 8) return rawDate;
    
    String year = rawDate.substring(0, 2);   // "25"
    String month = rawDate.substring(3, 5);  // "09" 
    String day = rawDate.substring(6, 8);    // "17"
    
    // YY -> YYYY dönüşümü (20XX varsayımı)
    int yearInt = year.toInt();
    if (yearInt >= 0 && yearInt <= 99) {
        yearInt += 2000;
        year = String(yearInt);
    }
    
    // DD/MM/YYYY formatında döndür
    return day + "/" + month + "/" + year;
}

// Görüntüleme için saat formatla: zaten HH:MM:SS formatında
String formatTimeForDisplay(const String& rawTime) {
    return rawTime; // Zaten doğru formatta
}

// DateTime verisi geçerli mi?
bool isDateTimeDataValid() {
    return datetimeData.isValid && 
           datetimeData.date.length() > 0 && 
           datetimeData.time.length() > 0;
}

// DateTime verilerini temizle
void clearDateTimeData() {
    datetimeData.rawData = "";
    datetimeData.date = "";
    datetimeData.time = "";
    datetimeData.lastUpdate = 0;
    datetimeData.isValid = false;
    
    addLog("DateTime verileri temizlendi", INFO, "DATETIME");
}