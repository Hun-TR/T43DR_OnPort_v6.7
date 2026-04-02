// ntp_handler.cpp - Network Ayarları ile Güncellenmiş Versiyon
#include "ntp_handler.h"
#include "log_system.h"
#include "uart_handler.h"
#include <Preferences.h>

// Global değişkenler
NTPConfig ntpConfig;
bool ntpConfigured = false;

// IP formatını UART için dönüştür (12 digit format)
String formatIPForUART(const String& ip) {
    int dot1 = ip.indexOf('.');
    int dot2 = ip.indexOf('.', dot1 + 1);
    int dot3 = ip.indexOf('.', dot2 + 1);
    
    if (dot1 == -1 || dot2 == -1 || dot3 == -1) {
        return "";
    }
    
    String octet1 = ip.substring(0, dot1);
    String octet2 = ip.substring(dot1 + 1, dot2);
    String octet3 = ip.substring(dot2 + 1, dot3);
    String octet4 = ip.substring(dot3 + 1);
    
    char buffer[13];
    sprintf(buffer, "%03d%03d%03d%03d", 
            octet1.toInt(), octet2.toInt(), octet3.toInt(), octet4.toInt());
    
    return String(buffer);
}

// NTP sunucu IP'sini 2 parçaya böl (6+6 digit)
void splitIPForNTP(const String& ip, String& part1, String& part2) {
    int dot1 = ip.indexOf('.');
    int dot2 = ip.indexOf('.', dot1 + 1);
    int dot3 = ip.indexOf('.', dot2 + 1);
    
    if (dot1 == -1 || dot2 == -1 || dot3 == -1) {
        part1 = "";
        part2 = "";
        return;
    }
    
    String octet1 = ip.substring(0, dot1);
    String octet2 = ip.substring(dot1 + 1, dot2);
    String octet3 = ip.substring(dot2 + 1, dot3);
    String octet4 = ip.substring(dot3 + 1);
    
    int o1 = octet1.toInt();
    int o2 = octet2.toInt();
    int o3 = octet3.toInt();
    int o4 = octet4.toInt();
    
    // İlk iki oktet (6 digit)
    char buffer1[7];
    sprintf(buffer1, "%03d%03d", o1, o2);
    part1 = String(buffer1);
    
    // Son iki oktet (6 digit)
    char buffer2[7];
    sprintf(buffer2, "%03d%03d", o3, o4);
    part2 = String(buffer2);
}

// Network ayarlarını Slave WT32'ye gönder
bool sendNetworkConfigToSlave() {
    if (!uart3Initialized) {
        initUART3();
        delay(100);
    }
    
    addLog("📤 Network ayarları Slave WT32'ye gönderiliyor...", INFO, "NTP-NET");
    
    bool allSuccess = true;
    int successCount = 0;
    int totalCommands = 3; // Subnet, Gateway, DNS
    
    // 1. Subnet gönder - Format: "255255255000s"
    String subnetFormatted = formatIPForUART(String(ntpConfig.subnet));
    if (subnetFormatted.length() == 12) {
        String subnetCmd = subnetFormatted + "s";
        
        bool sent = false;
        for (int retry = 0; retry < 3 && !sent; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("Subnet tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-NET");
            }
            
            if (sendToSecondCard(subnetCmd)) {
                addLog("✅ Subnet Slave'e gönderildi: " + String(ntpConfig.subnet), SUCCESS, "NTP-NET");
                sent = true;
                successCount++;
            }
        }
        
        if (!sent) {
            addLog("❌ Subnet gönderilemedi", ERROR, "NTP-NET");
            allSuccess = false;
        }
        
        delay(150);
    } else {
        addLog("❌ Subnet format hatası", ERROR, "NTP-NET");
        allSuccess = false;
    }
    
    // 2. Gateway gönder - Format: "192168001001g"
    String gatewayFormatted = formatIPForUART(String(ntpConfig.gateway));
    if (gatewayFormatted.length() == 12) {
        String gatewayCmd = gatewayFormatted + "g";
        
        bool sent = false;
        for (int retry = 0; retry < 3 && !sent; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("Gateway tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-NET");
            }
            
            if (sendToSecondCard(gatewayCmd)) {
                addLog("✅ Gateway Slave'e gönderildi: " + String(ntpConfig.gateway), SUCCESS, "NTP-NET");
                sent = true;
                successCount++;
            }
        }
        
        if (!sent) {
            addLog("❌ Gateway gönderilemedi", ERROR, "NTP-NET");
            allSuccess = false;
        }
        
        delay(150);
    } else {
        addLog("❌ Gateway format hatası", ERROR, "NTP-NET");
        allSuccess = false;
    }
    
    // 3. DNS gönder - Format: "008008008008d"
    String dnsFormatted = formatIPForUART(String(ntpConfig.dns));
    if (dnsFormatted.length() == 12) {
        String dnsCmd = dnsFormatted + "d";
        
        bool sent = false;
        for (int retry = 0; retry < 3 && !sent; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("DNS tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-NET");
            }
            
            if (sendToSecondCard(dnsCmd)) {
                addLog("✅ DNS Slave'e gönderildi: " + String(ntpConfig.dns), SUCCESS, "NTP-NET");
                sent = true;
                successCount++;
            }
        }
        
        if (!sent) {
            addLog("❌ DNS gönderilemedi", ERROR, "NTP-NET");
            allSuccess = false;
        }
    } else {
        addLog("❌ DNS format hatası", ERROR, "NTP-NET");
        allSuccess = false;
    }
    
    // Sonuç özeti
    if (allSuccess) {
        addLog("✅ Tüm network ayarları Slave'e gönderildi (" + String(successCount) + "/" + String(totalCommands) + ")", SUCCESS, "NTP-NET");
    } else {
        addLog("⚠️ Network ayarları kısmen gönderildi (" + String(successCount) + "/" + String(totalCommands) + ")", WARN, "NTP-NET");
    }
    
    return allSuccess;
}

// NTP sunucu ayarlarını Slave WT32'ye gönder
bool sendNTPServersToSlave() {
    if (!uart3Initialized) {
        initUART3();
        delay(100);
    }
    
    addLog("📤 NTP sunucu ayarları Slave WT32'ye gönderiliyor...", INFO, "NTP");
    
    bool allSuccess = true;
    
    // NTP1 gönder
    String ntp1_part1, ntp1_part2;
    splitIPForNTP(String(ntpConfig.ntpServer1), ntp1_part1, ntp1_part2);
    
    if (ntp1_part1.length() == 6 && ntp1_part2.length() == 6) {
        // NTP1 Part1: "192168u"
        String cmd1 = ntp1_part1 + "u";
        bool sent1 = false;
        
        for (int retry = 0; retry < 3 && !sent1; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("NTP1 Part1 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP");
            }
            
            if (sendToSecondCard(cmd1)) {
                addLog("✅ NTP1 Part1 Slave'e gönderildi: " + cmd1, SUCCESS, "NTP");
                sent1 = true;
            }
        }
        
        if (!sent1) {
            addLog("❌ NTP1 Part1 gönderilemedi", ERROR, "NTP");
            allSuccess = false;
        }
        
        delay(150);
        
        // NTP1 Part2: "003002y"
        String cmd2 = ntp1_part2 + "y";
        bool sent2 = false;
        
        for (int retry = 0; retry < 3 && !sent2; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("NTP1 Part2 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP");
            }
            
            if (sendToSecondCard(cmd2)) {
                addLog("✅ NTP1 Part2 Slave'e gönderildi: " + cmd2, SUCCESS, "NTP");
                sent2 = true;
            }
        }
        
        if (!sent2) {
            addLog("❌ NTP1 Part2 gönderilemedi", ERROR, "NTP");
            allSuccess = false;
        }
    } else {
        addLog("❌ NTP1 format dönüşümü başarısız", ERROR, "NTP");
        allSuccess = false;
    }
    
    // NTP2 varsa gönder
    if (strlen(ntpConfig.ntpServer2) > 0) {
        delay(150);
        
        String ntp2_part1, ntp2_part2;
        splitIPForNTP(String(ntpConfig.ntpServer2), ntp2_part1, ntp2_part2);
        
        if (ntp2_part1.length() == 6 && ntp2_part2.length() == 6) {
            // NTP2 Part1: "008008w"
            String cmd3 = ntp2_part1 + "w";
            bool sent3 = false;
            
            for (int retry = 0; retry < 3 && !sent3; retry++) {
                if (retry > 0) {
                    delay(100);
                    addLog("NTP2 Part1 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP");
                }
                
                if (sendToSecondCard(cmd3)) {
                    addLog("✅ NTP2 Part1 Slave'e gönderildi: " + cmd3, SUCCESS, "NTP");
                    sent3 = true;
                }
            }
            
            if (!sent3) {
                addLog("❌ NTP2 Part1 gönderilemedi", ERROR, "NTP");
                allSuccess = false;
            }
            
            delay(150);
            
            // NTP2 Part2: "008008x"
            String cmd4 = ntp2_part2 + "x";
            bool sent4 = false;
            
            for (int retry = 0; retry < 3 && !sent4; retry++) {
                if (retry > 0) {
                    delay(100);
                    addLog("NTP2 Part2 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP");
                }
                
                if (sendToSecondCard(cmd4)) {
                    addLog("✅ NTP2 Part2 Slave'e gönderildi: " + cmd4, SUCCESS, "NTP");
                    sent4 = true;
                }
            }
            
            if (!sent4) {
                addLog("❌ NTP2 Part2 gönderilemedi", ERROR, "NTP");
                allSuccess = false;
            }
        } else {
            addLog("❌ NTP2 format dönüşümü başarısız", ERROR, "NTP");
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

// Ana gönderim fonksiyonu (dsPIC + Slave WT32)
void sendNTPConfigToBackend() {
    if (strlen(ntpConfig.ntpServer1) == 0) {
        addLog("NTP sunucu adresi boş", WARN, "NTP");
        return;
    }
    
    // UART3'ü başlat
    initUART3();
    delay(100);
    
    addLog("🚀 NTP ve Network ayarları gönderiliyor...", INFO, "NTP");
    
    String response;
    bool dspicSuccess = true;
    bool slaveNetSuccess = false;
    bool slaveNtpSuccess = false;
    
    // 1. ÖNCE NETWORK AYARLARINI SLAVE'E GÖNDER
    slaveNetSuccess = sendNetworkConfigToSlave();
    delay(200);
    
    // 2. SONRA NTP SUNUCU AYARLARINI SLAVE'E GÖNDER
    slaveNtpSuccess = sendNTPServersToSlave();
    delay(200);
    
    // 3. dsPIC33EP'YE NTP AYARLARINI GÖNDER (eski metod)
    String ntp1_part1, ntp1_part2;
    splitIPForNTP(String(ntpConfig.ntpServer1), ntp1_part1, ntp1_part2);
    
    if (ntp1_part1.length() == 6 && ntp1_part2.length() == 6) {
        String cmd1 = ntp1_part1 + "u";
        if (sendCustomCommand(cmd1, response, 1000)) {
            addLog("✅ NTP1 Part1 dsPIC'e gönderildi: " + cmd1, SUCCESS, "NTP-DSPIC");
        } else {
            addLog("❌ NTP1 Part1 dsPIC'e gönderilemedi", ERROR, "NTP-DSPIC");
            dspicSuccess = false;
        }
        
        delay(100);
        
        String cmd2 = ntp1_part2 + "y";
        if (sendCustomCommand(cmd2, response, 1000)) {
            addLog("✅ NTP1 Part2 dsPIC'e gönderildi: " + cmd2, SUCCESS, "NTP-DSPIC");
        } else {
            addLog("❌ NTP1 Part2 dsPIC'e gönderilemedi", ERROR, "NTP-DSPIC");
            dspicSuccess = false;
        }
    }
    
    // NTP2 varsa dsPIC'e gönder
    if (strlen(ntpConfig.ntpServer2) > 0) {
        delay(100);
        
        String ntp2_part1, ntp2_part2;
        splitIPForNTP(String(ntpConfig.ntpServer2), ntp2_part1, ntp2_part2);
        
        if (ntp2_part1.length() == 6 && ntp2_part2.length() == 6) {
            String cmd3 = ntp2_part1 + "w";
            if (sendCustomCommand(cmd3, response, 1000)) {
                addLog("✅ NTP2 Part1 dsPIC'e gönderildi: " + cmd3, SUCCESS, "NTP-DSPIC");
            } else {
                addLog("❌ NTP2 Part1 dsPIC'e gönderilemedi", ERROR, "NTP-DSPIC");
                dspicSuccess = false;
            }
            
            delay(100);
            
            String cmd4 = ntp2_part2 + "x";
            if (sendCustomCommand(cmd4, response, 1000)) {
                addLog("✅ NTP2 Part2 dsPIC'e gönderildi: " + cmd4, SUCCESS, "NTP-DSPIC");
            } else {
                addLog("❌ NTP2 Part2 dsPIC'e gönderilemedi", ERROR, "NTP-DSPIC");
                dspicSuccess = false;
            }
        }
    }
    
    // Genel sonuç
    if (dspicSuccess && slaveNetSuccess && slaveNtpSuccess) {
        addLog("✅ TÜM ayarlar başarıyla gönderildi (dsPIC + Slave WT32)", SUCCESS, "NTP");
    } else if (dspicSuccess && (slaveNetSuccess || slaveNtpSuccess)) {
        addLog("⚠️ dsPIC OK, Slave kısmen başarılı", WARN, "NTP");
    } else if (!dspicSuccess && (slaveNetSuccess && slaveNtpSuccess)) {
        addLog("⚠️ Slave OK, dsPIC başarısız", WARN, "NTP");
    } else {
        addLog("❌ Gönderim kısmen başarısız", ERROR, "NTP");
    }
}

// NTP ayarlarını kaydet (güncellenmiş versiyon)
bool saveNTPSettings(const String& server1, const String& server2, 
                     const String& subnet, const String& gateway, 
                     const String& dns, int timezone) {
    if (!isValidIPOrDomain(server1)) {
        addLog("❌ Geçersiz birincil NTP sunucu: " + server1, ERROR, "NTP");
        return false;
    }
    
    if (!isValidIPOrDomain(subnet)) {
        addLog("❌ Geçersiz Subnet: " + subnet, ERROR, "NTP");
        return false;
    }
    
    if (!isValidIPOrDomain(gateway)) {
        addLog("❌ Geçersiz Gateway: " + gateway, ERROR, "NTP");
        return false;
    }
    
    if (!isValidIPOrDomain(dns)) {
        addLog("❌ Geçersiz DNS: " + dns, ERROR, "NTP");
        return false;
    }
    
    if (server2.length() > 0 && !isValidIPOrDomain(server2)) {
        addLog("❌ Geçersiz ikincil NTP sunucu: " + server2, ERROR, "NTP");
        return false;
    }
    
    Preferences preferences;
    preferences.begin("ntp-config", false);
    
    // Tüm ayarları kaydet
    preferences.putString("ntp_server1", server1);
    preferences.putString("ntp_server2", server2);
    preferences.putString("subnet", subnet);
    preferences.putString("gateway", gateway);
    preferences.putString("dns", dns);
    preferences.putInt("timezone", timezone);
    preferences.putBool("enabled", true);
    
    preferences.end();
    
    // Global config güncelle
    server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
    server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
    subnet.toCharArray(ntpConfig.subnet, sizeof(ntpConfig.subnet));
    gateway.toCharArray(ntpConfig.gateway, sizeof(ntpConfig.gateway));
    dns.toCharArray(ntpConfig.dns, sizeof(ntpConfig.dns));
    ntpConfig.timezone = timezone;
    ntpConfig.enabled = true;
    ntpConfigured = true;
    
    addLog("✅ NTP+Network ayarları kaydedildi", SUCCESS, "NTP");
    addLog("  NTP1   : " + server1, INFO, "NTP");
    addLog("  NTP2   : " + server2, INFO, "NTP");
    addLog("  Subnet : " + subnet, INFO, "NTP");
    addLog("  Gateway: " + gateway, INFO, "NTP");
    addLog("  DNS    : " + dns, INFO, "NTP");
    
    // Backend'e gönder
    sendNTPConfigToBackend();
    return true;
}

// NTP ayarlarını yükle (güncellenmiş)
bool loadNTPSettings() {
    Preferences preferences;
    preferences.begin("ntp-config", true);
    
    String server1 = preferences.getString("ntp_server1", "");
    String server2 = preferences.getString("ntp_server2", "");
    String subnet = preferences.getString("subnet", "255.255.255.0");
    String gateway = preferences.getString("gateway", "192.168.1.1");
    String dns = preferences.getString("dns", "8.8.8.8");
    
    preferences.end();
    
    if (server1.length() == 0) {
        // Varsayılan değerler
        strcpy(ntpConfig.ntpServer1, "192.168.3.2");
        strcpy(ntpConfig.ntpServer2, "8.8.8.8");
        strcpy(ntpConfig.subnet, "255.255.255.0");
        strcpy(ntpConfig.gateway, "192.168.3.2");
        strcpy(ntpConfig.dns, "192.168.3.2");
        ntpConfig.timezone = 3;
        ntpConfig.enabled = false;
        ntpConfigured = false;
        
        addLog("⚠️ Kayıtlı NTP ayarı bulunamadı, varsayılanlar yüklendi", WARN, "NTP");
        
        // Varsayılanları kaydet
        saveNTPSettings("192.168.3.2", "8.8.8.8", "255.255.255.0", "192.168.3.2", "192.168.3.2", 3);
        return false;
    }
    
    // Global config'e yükle
    server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
    server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
    subnet.toCharArray(ntpConfig.subnet, sizeof(ntpConfig.subnet));
    gateway.toCharArray(ntpConfig.gateway, sizeof(ntpConfig.gateway));
    dns.toCharArray(ntpConfig.dns, sizeof(ntpConfig.dns));
    
    ntpConfig.timezone = preferences.getInt("timezone", 3);
    ntpConfig.enabled = preferences.getBool("enabled", true);
    
    ntpConfigured = true;
    addLog("✅ NTP+Network ayarları yüklendi", SUCCESS, "NTP");
    return true;
}

bool isValidIPOrDomain(const String& address) {
    if (address.length() < 7 || address.length() > 253) return false;
    
    IPAddress testIP;
    if (testIP.fromString(address)) {
        return true;
    }
    
    if (address.indexOf('.') > 0 && address.indexOf(' ') == -1) {
        return true;
    }
    
    return false;
}

void initNTPHandler() {
    addLog("🚀 NTP Handler başlatılıyor...", INFO, "NTP");
    
    if (!loadNTPSettings()) {
        addLog("⚠️ Kayıtlı NTP ayarı bulunamadı", WARN, "NTP");
    } else {
        addLog("✅ NTP ayarları yüklendi", SUCCESS, "NTP");
    }
    
    if (ntpConfigured && strlen(ntpConfig.ntpServer1) > 0) {
        delay(1000);
        sendNTPConfigToBackend();
    }
    
    addLog("✅ NTP Handler başlatıldı", SUCCESS, "NTP");
}

// Diğer fonksiyonlar...
// ntp_handler.cpp — YENİ (doğru)
bool syncNTPWithDsPIC() {
    if (strlen(ntpConfig.ntpServer1) == 0) {
        addLog("⚠️ NTP sunucu adresi boş, senkronizasyon atlandı", WARN, "NTP");
        return false;
    }
    
    addLog("🔄 dsPIC ile NTP senkronizasyonu başlatılıyor...", INFO, "NTP");
    
    String ntp1, ntp2;
    bool success = requestNTPFromDsPIC(ntp1, ntp2);
    
    if (success) {
        // dsPIC'ten gelen değerleri global config'e yaz
        ntp1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
        ntp2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
        
        addLog("✅ dsPIC senkronizasyonu başarılı: " + ntp1, SUCCESS, "NTP");
    } else {
        addLog("❌ dsPIC senkronizasyonu başarısız", ERROR, "NTP");
    }
    
    return success;
}

void resetNTPSettings() {
    // Mevcut implementasyon...
}

bool isNTPSynced() {
    return ntpConfigured;
}