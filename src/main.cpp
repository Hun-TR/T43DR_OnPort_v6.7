// src/main.cpp - Network ile ilgili düzeltmeler

#include <Arduino.h>
#include <LittleFS.h>
#include <ESPmDNS.h>
#include <esp_log.h>
#include "settings.h"
#include "log_system.h"
#include "uart_handler.h"
#include "web_routes.h"
#include "password_policy.h"
#include "backup_restore.h"
#include "datetime_handler.h"
#include "fault_parser.h"
#include "time_sync.h"  // BU SATIRI EKLE

// External fonksiyonlar
extern String getTimeSyncStats();
extern void loadNetworkConfig();
extern void initEthernetAdvanced();

TaskHandle_t webTaskHandle = NULL;
TaskHandle_t uartTaskHandle = NULL;

// Web server task - Core 0'da çalışacak
void webServerTask(void *parameter) {
    while(true) {
        server.handleClient();
        vTaskDelay(1);
    }
}

// UART ve zaman senkronizasyon task - Core 1'de
void uartTask(void *parameter) {
    while(true) {
        checkTimeSync();
        checkUARTHealth();
        vTaskDelay(1000); // 1 saniye
    }
}

void initMDNS() {
    uint8_t mac[6];
    ETH.macAddress(mac);
    char hostname[32];
    sprintf(hostname, "teias-%02x%02x", mac[4], mac[5]);
    
    if (MDNS.begin(hostname)) {
        addLog("✅ mDNS başlatıldı: " + String(hostname) + ".local", SUCCESS, "mDNS");
        MDNS.addService("http", "tcp", 80);
    } else {
        addLog("❌ mDNS başlatılamadı", ERROR, "mDNS");
    }
}

void setup() {
    Serial.begin(115200);
    setCpuFrequencyMhz(240);
    esp_log_level_set("*", ESP_LOG_NONE);
    
    Serial.println("\n\n--- TEIAS EKLIM SISTEMI v5.0 ---");
    
    if(!LittleFS.begin(true)){
        Serial.println("LittleFS HATA!");
        ESP.restart();
    }
    
    initLogSystem();
    loadSettings();
    loadNetworkConfig();
    initEthernetAdvanced();
    initUART();
    setupWebRoutes();
    loadPasswordPolicy();
    initMDNS();

    // Sistem başlangıcında zaman senkronizasyonu yap
    delay(2000); // dsPIC'in hazır olmasını bekle
    initTimeSync(); // YENİ SATIR
    
    xTaskCreatePinnedToCore(webServerTask, "WebServer", 8192, NULL, 2, &webTaskHandle, 0);
    xTaskCreatePinnedToCore(uartTask, "UART", 4096, NULL, 1, &uartTaskHandle, 1);
    
    addLog("🚀 Sistem başlatıldı", SUCCESS, "SYSTEM");
}

void loop() {
    // Otomatik backup - her 24 saatte bir
    static unsigned long lastBackupCheck = 0;
    if (millis() - lastBackupCheck > 86400000) { // 24 saat
        createAutomaticBackup();
        lastBackupCheck = millis();
    }
    
    // Ethernet durumu kontrolü - 30 saniyede bir
    static unsigned long lastEthCheck = 0;
    static bool lastEthStatus = false;
    if (millis() - lastEthCheck > 30000) {
        bool currentEthStatus = ETH.linkUp();
        if (currentEthStatus != lastEthStatus) {
            addLog(currentEthStatus ? "✅ Ethernet bağlandı" : "❌ Ethernet kesildi", 
                   currentEthStatus ? SUCCESS : ERROR, "ETH");
            lastEthStatus = currentEthStatus;
        }
        lastEthCheck = millis();
    }
    
    vTaskDelay(1000); // Ana döngüyü yavaşlat
}