#ifndef FAULT_PARSER_H
#define FAULT_PARSER_H

#include <Arduino.h>

// Arıza kaydı yapısı
struct FaultRecord {
    int pinNumber;          // Pin numarası (1-8 çıkış, 9-16 giriş)
    String pinType;         // "Çıkış" veya "Giriş"
    String pinName;         // "Çıkış 1" veya "Giriş 9" gibi
    int year;              // Yıl
    int month;             // Ay
    int day;               // Gün
    int hour;              // Saat
    int minute;            // Dakika
    int second;            // Saniye
    int millisecond;       // Milisaniye
    float duration;        // Arıza süresi (saniye)
    String dateTime;       // Formatlanmış tarih-saat
    String rawData;        // Ham veri
    bool isValid;          // Parse başarılı mı?
    String errorMessage;   // Hata mesajı
};

// Fonksiyon tanımlamaları
FaultRecord parseFaultData(const String& rawData);
String formatPinInfo(int pinNumber);
String formatDateTime(int year, int month, int day, int hour, int minute, int second);
String formatDuration(float duration);
int hexCharToInt(char hex);
int parseHexToInt(const String& hexStr);
bool isValidFaultData(const String& data);

#endif // FAULT_PARSER_H