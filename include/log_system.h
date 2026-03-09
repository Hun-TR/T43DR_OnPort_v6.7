#ifndef LOG_SYSTEM_H
#define LOG_SYSTEM_H

#include <Arduino.h>
#include <vector>

enum LogLevel {
    ERROR = 0,
    WARN = 1,
    INFO = 2,
    DEBUG = 3,
    SUCCESS = 4
};

struct LogEntry {
    String timestamp;
    String message;
    LogLevel level;
    String source;
    unsigned long millis_time;
};

// Dinamik log sistemi için değişkenler
extern std::vector<LogEntry> logStorage;  // Dinamik log depolama
extern const int MAX_LOG_SIZE;            // Maksimum log sayısı (500 olarak ayarlanacak)
extern const int PAGE_SIZE;               // Sayfa başına log sayısı (50)


void initLogSystem();
void addLog(const String& msg, LogLevel level, const String& source);

String logLevelToString(LogLevel level);
void clearLogs();
String getFormattedTimestamp();
String getFormattedTimestampFallback();

// Pagination için yeni fonksiyonlar
int getTotalLogCount();
int getTotalPageCount();
std::vector<LogEntry> getLogsPage(int pageNumber);
void trimOldLogs();  // Eski logları temizle (MAX_LOG_SIZE'ı aşınca)

#endif