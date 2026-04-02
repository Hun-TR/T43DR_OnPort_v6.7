#ifndef TIME_SYNC_H
#define TIME_SYNC_H

#include <Arduino.h>

// Zaman verilerini saklamak için yapı
struct TimeData {
    String lastDate;   // "2025-02-22"
    String lastTime;   // "11:22:33"
    unsigned long lastSync = 0;
    unsigned int syncCount = 0;
    unsigned int failCount = 0;
    bool isValid = false;
};

// Dışarıya açık global değişken
extern TimeData timeData;

// Fonksiyonlar
bool requestTimeFromDsPIC();
void updateSystemTime();
String getCurrentDate();
String getCurrentDateTime();
bool isTimeSynced();
void checkTimeSync();
void initTimeSync(); // YENİ FONKSİYON

#endif