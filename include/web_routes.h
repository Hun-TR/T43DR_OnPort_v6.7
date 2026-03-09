// include/web_routes.h - ARIZA API'LERİ GÜNCELLENMİŞ

#ifndef WEB_ROUTES_H
#define WEB_ROUTES_H

#include <Arduino.h>

void setupWebRoutes();
void serveStaticFile(const String& path, const String& contentType);
String getUptime();
void addSecurityHeaders();
bool checkRateLimit();

// API Handler fonksiyonları
void handleStatusAPI();
void handleGetSettingsAPI();
void handlePostSettingsAPI();

// Arıza API'leri - YENİ
void handleGetFaultCountAPI();      // AN komutu ile toplam sayıyı al
void handleGetSpecificFaultAPI();   // Belirli arıza kaydını al
void handleParsedFaultAPI();        // Parse edilmiş arıza verisi (güncellendi)
// handleFaultRequest() KALDIRILDI - artık kullanılmıyor

// NTP API'leri
void handleGetNtpAPI();
void handlePostNtpAPI();

// BaudRate API'leri
void handleGetBaudRateAPI();
void handlePostBaudRateAPI();

// Log API'leri
void handleGetLogsAPI();
void handleClearLogsAPI();

// System API'leri
void handleSystemInfoAPI();
void handleSessionRefresh();
void handleUARTTestAPI();
void handleDeviceInfoAPI();
void handleSystemRebootAPI();

// Network API'leri
void handleGetNetworkAPI();
void handlePostNetworkAPI();

// Notification API
void handleNotificationAPI();

// DateTime API handlers
void handleGetDateTimeAPI();
void handleFetchDateTimeAPI();
void handleSetDateTimeAPI();
void handleSyncESP32API();
void handleSetCurrentTimeAPI();
void handleDateTimeHistoryAPI();
void handleDateTimePreviewAPI();

// LED Status API
void handleGetLedStatusAPI();

#endif // WEB_ROUTES_H