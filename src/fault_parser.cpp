#include "fault_parser.h"
#include "log_system.h"

// Hex karakter to int
int hexCharToInt(char hex) {
    if (hex >= '0' && hex <= '9') return hex - '0';
    if (hex >= 'A' && hex <= 'F') return hex - 'A' + 10;
    if (hex >= 'a' && hex <= 'f') return hex - 'a' + 10;
    return 0;
}

// Hex string to int
int parseHexToInt(const String& hexStr) {
    int result = 0;
    for (int i = 0; i < hexStr.length(); i++) {
        result = result * 16 + hexCharToInt(hexStr.charAt(i));
    }
    return result;
}

// Pin bilgisi formatla
String formatPinInfo(int pinNumber) {
    if (pinNumber >= 1 && pinNumber <= 8) {
        return "Çıkış " + String(pinNumber);
    } else if (pinNumber >= 9 && pinNumber <= 16) {
        return "Giriş " + String(pinNumber);
    } else {
        return "Bilinmeyen Pin " + String(pinNumber);
    }
}

// Tarih-saat formatla
String formatDateTime(int year, int month, int day, int hour, int minute, int second) {
    char buffer[20];
    sprintf(buffer, "%02d/%02d/%04d %02d:%02d:%02d", day, month, 2000 + year, hour, minute, second);
    return String(buffer);
}

// Süre formatla
String formatDuration(float duration) {
    if (duration < 1.0) {
        return String((int)(duration * 1000)) + " ms";
    } else if (duration < 60.0) {
        return String(duration, 3) + " sn";
    } else if (duration < 3600.0) {
        int minutes = (int)(duration / 60);
        float seconds = duration - (minutes * 60);
        return String(minutes) + "dk " + String(seconds, 1) + "sn";
    } else {
        int hours = (int)(duration / 3600);
        int minutes = (int)((duration - hours * 3600) / 60);
        return String(hours) + "sa " + String(minutes) + "dk";
    }
}

// Arıza verisi geçerli mi kontrol et
bool isValidFaultData(const String& data) {
    // En az 22 karakter olmalı (08250723154619 + abcdefgh = 14 + 8 = 22)
    // Ama eksik veri gelirse en az 16 karakter olsun
    if (data.length() < 16) return false;
    
    // İlk iki karakter hex olmalı (pin numarası)
    char first = data.charAt(0);
    char second = data.charAt(1);
    if (!((first >= '0' && first <= '9') || (first >= 'A' && first <= 'F') || (first >= 'a' && first <= 'f'))) return false;
    if (!((second >= '0' && second <= '9') || (second >= 'A' && second <= 'F') || (second >= 'a' && second <= 'f'))) return false;
    
    // Tarih kısmı sayısal olmalı (2-13. karakterler: YYMMDDHHMMSS)
    for (int i = 2; i < 14 && i < data.length(); i++) {
        if (!(data.charAt(i) >= '0' && data.charAt(i) <= '9')) {
            return false;
        }
    }
    
    return true;
}

// Ana parsing fonksiyonu
FaultRecord parseFaultData(const String& rawData) {
    FaultRecord fault;
    fault.rawData = rawData;
    fault.isValid = false;
    fault.errorMessage = "";
    
    // Trim ve temel kontrol
    String data = rawData;
    data.trim();
    
    if (!isValidFaultData(data)) {
        fault.errorMessage = "Geçersiz veri formatı";
        addLog("❌ Geçersiz arıza verisi: " + data, ERROR, "FAULT_PARSER");
        return fault;
    }
    
    try {
        // Pin numarası parse et (ilk 2 hex karakter)
        String pinHex = data.substring(0, 2);
        fault.pinNumber = parseHexToInt(pinHex);
        
        // Pin tipi belirle
        if (fault.pinNumber >= 1 && fault.pinNumber <= 8) {
            fault.pinType = "Çıkış";
            fault.pinName = "Çıkış " + String(fault.pinNumber);
        } else if (fault.pinNumber >= 9 && fault.pinNumber <= 16) {
            fault.pinType = "Giriş";
            fault.pinName = "Giriş " + String(fault.pinNumber);
        } else {
            fault.pinType = "Bilinmeyen";
            fault.pinName = "Pin " + String(fault.pinNumber);
        }
        
        // Tarih-saat parse et (2-13. karakterler: YYMMDDHHMMSS)
        if (data.length() >= 14) {
            fault.year = data.substring(2, 4).toInt();    // YY
            fault.month = data.substring(4, 6).toInt();   // MM
            fault.day = data.substring(6, 8).toInt();     // DD
            fault.hour = data.substring(8, 10).toInt();   // HH
            fault.minute = data.substring(10, 12).toInt(); // MM
            fault.second = data.substring(12, 14).toInt(); // SS
            
            // Tarih doğrulama
            if (fault.month < 1 || fault.month > 12 || 
                fault.day < 1 || fault.day > 31 ||
                fault.hour > 23 || fault.minute > 59 || fault.second > 59) {
                fault.errorMessage = "Geçersiz tarih-saat değerleri";
                return fault;
            }
            
            fault.dateTime = formatDateTime(fault.year, fault.month, fault.day, 
                                          fault.hour, fault.minute, fault.second);
        }
        
        // Milisaniye ve süre parse et (14. karakterden sonra)
        if (data.length() >= 20) {
            // Milisaniye (3 hex karakter) - abc kısmı
            String msHex = data.substring(14, 17);
            fault.millisecond = parseHexToInt(msHex);
            
            // Süre (5 hex karakter) - defgh kısmı -> de.fgh saniye
            String durationHex = data.substring(17, 22);
            if (durationHex.length() == 5) {
                // İlk 2 karakter: tam saniye kısmı
                String secondsHex = durationHex.substring(0, 2);
                int seconds = parseHexToInt(secondsHex);
                
                // Son 3 karakter: ondalık kısmı (0.001 - 0.999)
                String fractionalHex = durationHex.substring(2, 5);
                int fractional = parseHexToInt(fractionalHex);
                
                // Duration hesapla: de + (fgh/4096) saniye
                fault.duration = seconds + (float)fractional / 4096.0;
            }
        }
        
        fault.isValid = true;
        
        addLog("✅ Arıza kaydı parse edildi: " + fault.pinName + " (" + fault.dateTime + ")", 
               SUCCESS, "FAULT_PARSER");
               
    } catch (...) {
        fault.errorMessage = "Parse işlemi sırasında hata";
        fault.isValid = false;
        addLog("❌ Arıza parse hatası: " + data, ERROR, "FAULT_PARSER");
    }
    
    return fault;
}