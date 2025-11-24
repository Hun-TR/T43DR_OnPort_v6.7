#ifndef DATETIME_HANDLER_H
#define DATETIME_HANDLER_H

#include <Arduino.h>

// DateTime işleme yapısı
struct DateTimeData {
    String rawData;
    String date;
    String time;
    unsigned long lastUpdate;
    bool isValid;
};

// Global datetime verisi
extern DateTimeData datetimeData;

// Komut geçmişi yapısı
struct CommandHistory {
    String command;
    String timestamp;
    bool success;
    String response;
};

// Fonksiyon tanımlamaları
bool requestDateTimeFromDsPIC();
bool parseeDateTimeResponse(const String& response);
bool setDateTimeToDsPIC(const String& date, const String& time);
String formatDateCommand(const String& date);
String formatTimeCommand(const String& time);
bool validateDateTime(const String& date, const String& time);
String getCurrentESP32DateTime();
bool syncWithESP32Time();
void addCommandToHistory(const String& command, bool success, const String& response);
String getCommandHistoryJSON();

// Yardımcı fonksiyonlar
String formatDateForDisplay(const String& rawDate);
String formatTimeForDisplay(const String& rawTime);
bool isDateTimeDataValid();
void clearDateTimeData();

#endif // DATETIME_HANDLER_H