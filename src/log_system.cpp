#include "log_system.h"
#include <time.h>

// Global değişkenlerin tanımlamaları
std::vector<LogEntry> logStorage;
const int MAX_LOG_SIZE = 500;  // Maksimum 500 log kaydı tutulacak
const int PAGE_SIZE = 50;      // Sayfa başına 50 kayıt

// NTP'den geçerli zaman alınamazsa kullanılacak zaman formatı
String getFormattedTimestampFallback() {
    unsigned long seconds = millis() / 1000;
    unsigned long minutes = seconds / 60;
    unsigned long hours = minutes / 60;
    seconds %= 60;
    minutes %= 60;
    hours %= 24;

    char buffer[16];
    sprintf(buffer, "%02lu:%02lu:%02lu", hours, minutes, seconds);
    return String(buffer);
}

// NTP'den veya sistemden zamanı alıp formatlayan ana fonksiyon
String getFormattedTimestamp() {
    struct tm timeinfo;
    // getLocalTime'a bir timeout parametresi ekleyerek takılmayı önle
    if (getLocalTime(&timeinfo, 10)) { // 10 milisaniye bekle
        char buffer[32];
        strftime(buffer, sizeof(buffer), "%d.%m.%Y %H:%M:%S", &timeinfo);
        return String(buffer);
    } else {
        // NTP senkronize değilse, millis() kullan
        return getFormattedTimestampFallback();
    }
}

// Log sistemini başlatan fonksiyon
void initLogSystem() {
    logStorage.clear();
    logStorage.reserve(MAX_LOG_SIZE); // Bellek rezervasyonu yap
    
    // Sistem başlatıldığında ilk logu ekle
    addLog("Log sistemi başlatıldı. Max " + String(MAX_LOG_SIZE) + " kayıt tutulacak.", INFO, "SYSTEM");
}

// Eski logları temizle (FIFO mantığı)
void trimOldLogs() {
    // Eğer MAX_LOG_SIZE aşılmışsa, en eski logları sil
    while (logStorage.size() > MAX_LOG_SIZE) {
        logStorage.erase(logStorage.begin()); // İlk (en eski) elemanı sil
    }
}

// Yeni bir log ekleyen ana fonksiyon
void addLog(const String& msg, LogLevel level, const String& source) {
    LogEntry newEntry;
    newEntry.timestamp = getFormattedTimestamp();
    newEntry.message = msg;
    newEntry.level = level;
    newEntry.source = source;
    newEntry.millis_time = millis();

    // Yeni logu vektörün sonuna ekle (en yeni)
    logStorage.push_back(newEntry);
    
    // Boyut kontrolü yap
    trimOldLogs();

    // Sadece DEBUG_MODE tanımlıysa seri porta yazdır
    #ifdef DEBUG_MODE
    Serial.println("[" + newEntry.timestamp + "] [" + logLevelToString(level) + "] [" + source + "] " + msg);
    #endif
}

// Log seviyesini string'e çeviren yardımcı fonksiyon
String logLevelToString(LogLevel level) {
    switch (level) {
        case ERROR: return "ERROR";
        case WARN:  return "WARN";
        case INFO:  return "INFO";
        case DEBUG: return "DEBUG";
        case SUCCESS: return "SUCCESS";
        default: return "UNKNOWN";
    }
}

// Tüm logları temizleyen fonksiyon - GERÇEKTEN HER ŞEYİ TEMİZLER
void clearLogs() {
    // Belleği tamamen temizle
    logStorage.clear();
    logStorage.shrink_to_fit(); // Ayrılan belleği de serbest bırak
    
    // Yeniden başlat
    logStorage.reserve(MAX_LOG_SIZE);
    
    // Temizleme logu ekle
    addLog("Log kayıtları temizlendi.", WARN, "SYSTEM");
}

// Toplam log sayısını döndür
int getTotalLogCount() {
    return logStorage.size();
}

// Toplam sayfa sayısını döndür
int getTotalPageCount() {
    if (logStorage.size() == 0) return 1;
    return (logStorage.size() + PAGE_SIZE - 1) / PAGE_SIZE; // Yukarı yuvarlama
}

// Belirli bir sayfanın loglarını döndür
std::vector<LogEntry> getLogsPage(int pageNumber) {
    std::vector<LogEntry> pageLogs;
    
    if (pageNumber < 1) pageNumber = 1;
    
    int totalPages = getTotalPageCount();
    if (pageNumber > totalPages) pageNumber = totalPages;
    
    // Başlangıç ve bitiş indekslerini hesapla
    // En yeni loglar ilk sayfada olacak şekilde tersine sıralama
    int totalLogs = logStorage.size();
    int startIdx = totalLogs - (pageNumber * PAGE_SIZE);
    int endIdx = totalLogs - ((pageNumber - 1) * PAGE_SIZE) - 1;
    
    if (startIdx < 0) startIdx = 0;
    if (endIdx >= totalLogs) endIdx = totalLogs - 1;
    
    // Tersine sıralı olarak logları ekle (en yeni en üstte)
    for (int i = endIdx; i >= startIdx; i--) {
        pageLogs.push_back(logStorage[i]);
    }
    
    return pageLogs;
}