// include/network_config.h - Güncellenmiş header

#ifndef NETWORK_CONFIG_H
#define NETWORK_CONFIG_H

#include <Arduino.h>
#include <ETH.h>
#include <WiFi.h>  // Network event'ları için gerekli

struct NetworkConfig {
    bool useDHCP;
    IPAddress staticIP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress dns1;
    IPAddress dns2;
    String hostname;
    bool autoReconnect;
};

// Global değişken extern declaration
extern NetworkConfig netConfig;

// Fonksiyon tanımlamaları
void loadNetworkConfig();
void saveNetworkConfig(bool useDHCP, String ip, String gw, String sn, String d1, String d2);
void initEthernetAdvanced();
void setupNetworkEvents();
void networkReconnectionTask();

// Yardımcı fonksiyonlar
String getNetworkConfigJSON();
String getNetworkDiagnostics();
bool validateNetworkConfig(String ip, String gateway, String subnet, String dns1, String dns2);
void resetNetworkToDefaults();

#endif // NETWORK_CONFIG_H