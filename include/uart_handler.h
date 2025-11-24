#ifndef UART_HANDLER_H
#define UART_HANDLER_H

#include <Arduino.h>
#include <vector>  // YENİ EKLENEN - std::vector için gerekli

// Global değişkenler
extern bool uartHealthy;
extern String lastResponse;

// UART İstatistikleri yapısı
struct UARTStatistics {
    unsigned long totalFramesSent;
    unsigned long totalFramesReceived;
    unsigned long frameErrors;
    unsigned long checksumErrors;
    unsigned long timeoutErrors;
    float successRate;
};

extern UARTStatistics uartStats;

// Temel UART fonksiyonları
void initUART();
void resetUART();
bool testUARTConnection();
void checkUARTHealth();
String getUARTStatus();

// BaudRate fonksiyonları
bool changeBaudRate(long newBaudRate);
bool sendBaudRateCommand(long baudRate);

// YENİ FONKSIYONLAR - BaudRate sorgulama
int getCurrentBaudRateFromDsPIC();    // dsPIC'ten mevcut baudrate'i al (BN komutu)

// Arıza sorgulama fonksiyonları - YENİ
int getTotalFaultCount();                    // AN komutu ile toplam sayıyı al
bool requestSpecificFault(int faultNumber);  // Belirli bir arıza adresini sorgula (00001v, 00002v, ...)
bool requestFirstFault();                    // Geriye uyumluluk için (00001v)
String getLastFaultResponse();               // Son yanıtı al

// YENİ FONKSİYONLAR
bool deleteAllFaultsFromDsPIC();                      // tT komutu ile tüm arızaları sil
bool requestLastNFaults(int count, std::vector<String>& faultData); // Son N arızayı al

// Genel komut gönderme
bool sendCustomCommand(const String& command, String& response, unsigned long timeout = 0);
bool sendTestCommand(const String& testCmd);
bool sendToSecondCard(const String& data);
void initUART3();

// NTP ayarlarını dsPIC'ten oku (XN komutu)
bool requestNTPFromDsPIC(String& ntp1, String& ntp2);
// NTP ayarlarını sadece ikinci karta gönder (UART3)
bool sendNTPToSecondCardOnly(const String& ntp1, const String& ntp2);

// Yardımcı fonksiyonlar
void clearUARTBuffer();
String safeReadUARTResponse(unsigned long timeout);
void updateUARTStats(bool success);

// LED durum sorgulama fonksiyonları
bool requestLEDStatus(String& ledResponse);
bool parseLEDStatus(const String& ledData, uint8_t& inputByte, uint8_t& outputByte); // Eski format (backward compatibility)
bool parseLEDStatus(const String& ledData, uint8_t& inputByte, uint8_t& outputByte, uint8_t& alarmByte); // Yeni format (L:AABBCC)
String getLEDStatusReadable();

#endif // UART_HANDLER_H