#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <WebServer.h>
#include <ETH.h>

struct Settings {
    IPAddress local_IP;
    IPAddress gateway;
    IPAddress subnet;
    IPAddress primaryDNS;
    IPAddress secondaryDNS;  // Bu satırı ekle
    String deviceName;
    String transformerStation;
    String username;
    String passwordSalt;
    String passwordHash;
    long currentBaudRate;
    
    // isLoggedIn yerine bu alanı kullanacağız
    String sessionToken; 
    unsigned long sessionStartTime;
    bool isAdminSession;  // YÖNETİCİ OTURUMU İŞARETİ
    unsigned long SESSION_TIMEOUT;
};

extern WebServer server;
extern Settings settings;

void loadSettings();
bool saveSettings(const String& newDevName, const String& newTmName, const String& newUsername, const String& newPassword);
void initEthernet();

#endif