// ntp_handler.cpp - DÜZELTİLMİŞ VERSİYON
#include "ntp_handler.h"
#include "log_system.h"
#include "uart_handler.h"
#include <Preferences.h>

// Global değişkenler
NTPConfig ntpConfig;
bool ntpConfigured = false;

// IP adresini dsPIC formatına dönüştür
void formatIPForDsPIC(const String& ip, String& part1, String& part2) {
    // IP'yi parçala
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
    
    // Oktetleri integer'a çevir
    int o1 = octet1.toInt();
    int o2 = octet2.toInt();
    int o3 = octet3.toInt();
    int o4 = octet4.toInt();
    
    // İlk iki oktet'i birleştir - sprintf ile formatlama
    char buffer1[7];
    sprintf(buffer1, "%03d%03d", o1, o2);
    part1 = String(buffer1);
    
    // Son iki oktet'i birleştir - sprintf ile formatlama
    char buffer2[7];
    sprintf(buffer2, "%03d%03d", o3, o4);
    part2 = String(buffer2);
}

// NTP ayarlarını dsPIC33EP'ye gönder
void sendNTPConfigToBackend() {
    if (strlen(ntpConfig.ntpServer1) == 0) {
        addLog("NTP sunucu adresi boş", WARN, "NTP");
        return;
    }
    
    // UART3'ü başlat (eğer başlatılmamışsa)
    initUART3();
    delay(100); // İletişimin stabil olması için bekle
    
    String response;
    bool allSuccess = true;
    bool secondCardSuccess = true;
    
    // NTP1 için format dönüşümü
    String ntp1_part1, ntp1_part2;
    formatIPForDsPIC(String(ntpConfig.ntpServer1), ntp1_part1, ntp1_part2);
    
    if (ntp1_part1.length() == 6 && ntp1_part2.length() == 6) {
        // NTP1 ilk komut: 192168u
        String cmd1 = ntp1_part1 + "u";
        
        // dsPIC33EP'ye gönder
        if (sendCustomCommand(cmd1, response, 1000)) {
            addLog("✅ NTP1 Part1 dsPIC'e gönderildi: " + cmd1, SUCCESS, "NTP");
        } else {
            addLog("❌ NTP1 Part1 gönderilemedi: " + cmd1, ERROR, "NTP");
            allSuccess = false;
        }
        
        // İkinci karta gönder
        bool sent = false;
        for (int retry = 0; retry < 3 && !sent; retry++) {
            if (retry > 0) {
                delay(100); // Retry öncesi bekle
                addLog("NTP1 Part1 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-SYNC");
            }
            
            if (sendToSecondCard(cmd1)) {
                addLog("✅ NTP1 Part1 ikinci karta gönderildi", SUCCESS, "NTP-SYNC");
                sent = true;
            }
        }
        
        if (!sent) {
            addLog("⚠️ NTP1 Part1 ikinci karta gönderilemedi", WARN, "NTP-SYNC");
            secondCardSuccess = false;
        }
        
        delay(100); // Komutlar arası bekleme
        
        // NTP1 ikinci komut: 001002y
        String cmd2 = ntp1_part2 + "y";
        
        // dsPIC33EP'ye gönder
        if (sendCustomCommand(cmd2, response, 1000)) {
            addLog("✅ NTP1 Part2 dsPIC'e gönderildi: " + cmd2, SUCCESS, "NTP");
        } else {
            addLog("❌ NTP1 Part2 gönderilemedi: " + cmd2, ERROR, "NTP");
            allSuccess = false;
        }
        
        // İkinci karta gönder
        sent = false;
        for (int retry = 0; retry < 3 && !sent; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("NTP1 Part2 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-SYNC");
            }
            
            if (sendToSecondCard(cmd2)) {
                addLog("✅ NTP1 Part2 ikinci karta gönderildi", SUCCESS, "NTP-SYNC");
                sent = true;
            }
        }
        
        if (!sent) {
            addLog("⚠️ NTP1 Part2 ikinci karta gönderilemedi", WARN, "NTP-SYNC");
            secondCardSuccess = false;
        }
    } else {
        addLog("❌ NTP1 format dönüşümü başarısız", ERROR, "NTP");
        allSuccess = false;
    }
    
    // NTP2 varsa gönder
    if (strlen(ntpConfig.ntpServer2) > 0) {
        delay(100);
        
        String ntp2_part1, ntp2_part2;
        formatIPForDsPIC(String(ntpConfig.ntpServer2), ntp2_part1, ntp2_part2);
        
        if (ntp2_part1.length() == 6 && ntp2_part2.length() == 6) {
            // NTP2 ilk komut: 192169w
            String cmd3 = ntp2_part1 + "w";
            
            // dsPIC33EP'ye gönder
            if (sendCustomCommand(cmd3, response, 1000)) {
                addLog("✅ NTP2 Part1 dsPIC'e gönderildi: " + cmd3, SUCCESS, "NTP");
            } else {
                addLog("❌ NTP2 Part1 gönderilemedi: " + cmd3, ERROR, "NTP");
                allSuccess = false;
            }
            
            // İkinci karta gönder
            bool sent = false;
            for (int retry = 0; retry < 3 && !sent; retry++) {
                if (retry > 0) {
                    delay(100);
                    addLog("NTP2 Part1 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-SYNC");
                }
                
                if (sendToSecondCard(cmd3)) {
                    addLog("✅ NTP2 Part1 ikinci karta gönderildi", SUCCESS, "NTP-SYNC");
                    sent = true;
                }
            }
            
            if (!sent) {
                addLog("⚠️ NTP2 Part1 ikinci karta gönderilemedi", WARN, "NTP-SYNC");
                secondCardSuccess = false;
            }
            
            delay(100);
            
            // NTP2 ikinci komut: 001001x
            String cmd4 = ntp2_part2 + "x";
            
            // dsPIC33EP'ye gönder
            if (sendCustomCommand(cmd4, response, 1000)) {
                addLog("✅ NTP2 Part2 dsPIC'e gönderildi: " + cmd4, SUCCESS, "NTP");
            } else {
                addLog("❌ NTP2 Part2 gönderilemedi: " + cmd4, ERROR, "NTP");
                allSuccess = false;
            }
            
            // İkinci karta gönder
            sent = false;
            for (int retry = 0; retry < 3 && !sent; retry++) {
                if (retry > 0) {
                    delay(100);
                    addLog("NTP2 Part2 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "NTP-SYNC");
                }
                
                if (sendToSecondCard(cmd4)) {
                    addLog("✅ NTP2 Part2 ikinci karta gönderildi", SUCCESS, "NTP-SYNC");
                    sent = true;
                }
            }
            
            if (!sent) {
                addLog("⚠️ NTP2 Part2 ikinci karta gönderilemedi", WARN, "NTP-SYNC");
                secondCardSuccess = false;
            }
        } else {
            addLog("❌ NTP2 format dönüşümü başarısız", ERROR, "NTP");
            allSuccess = false;
        }
    }
    
    // Sonuç mesajı
    if (allSuccess && secondCardSuccess) {
        addLog("✅ Tüm NTP ayarları başarıyla dsPIC33EP ve ikinci karta gönderildi", SUCCESS, "NTP");
    } else if (allSuccess && !secondCardSuccess) {
        addLog("✅ NTP ayarları dsPIC33EP'ye gönderildi, ⚠️ ikinci karta kısmen gönderildi", WARN, "NTP");
    } else {
        addLog("⚠️ NTP ayarları kısmen gönderildi", WARN, "NTP");
    }
}

// ✅ DÜZELTİLMİŞ: NTP ayarlarını Preferences'tan yükle
bool loadNTPSettings() {
    Preferences preferences;
    preferences.begin("ntp-config", true); // true = read-only mode
    
    String server1 = preferences.getString("ntp_server1", "");
    String server2 = preferences.getString("ntp_server2", "");
    
    // Debug için log ekle
    addLog("📖 Preferences'tan okuma: NTP1=" + server1 + ", NTP2=" + server2, DEBUG, "NTP");
    
    if (server1.length() == 0) {
        preferences.end();
        
        // Kayıtlı ayar yoksa varsayılanları kullan
        strcpy(ntpConfig.ntpServer1, "192.168.1.1");
        strcpy(ntpConfig.ntpServer2, "8.8.8.8");
        ntpConfig.timezone = 3;
        ntpConfig.enabled = false;
        ntpConfigured = false;
        
        addLog("⚠️ Kayıtlı NTP ayarı bulunamadı, varsayılanlar yüklendi", WARN, "NTP");
        
        // Varsayılanları da kaydet
        saveNTPSettings("192.168.1.1", "8.8.8.8", 3);
        return false;
    }
    
    // Geçerlilik kontrolü
    if (!isValidIPOrDomain(server1) || (server2.length() > 0 && !isValidIPOrDomain(server2))) {
        preferences.end();
        
        // Geçersizse varsayılanları yükle
        strcpy(ntpConfig.ntpServer1, "192.168.1.1");
        strcpy(ntpConfig.ntpServer2, "8.8.8.8");
        ntpConfig.timezone = 3;
        ntpConfig.enabled = false;
        ntpConfigured = false;
        
        addLog("⚠️ Geçersiz NTP ayarları, varsayılanlar yüklendi", WARN, "NTP");
        return false;
    }
    
    // Global config'e yükle
    server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
    server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
    
    ntpConfig.timezone = preferences.getInt("timezone", 3);
    ntpConfig.enabled = preferences.getBool("enabled", true);
    
    preferences.end();
    
    ntpConfigured = true;
    addLog("✅ NTP ayarları Preferences'tan yüklendi: " + server1 + ", " + server2, SUCCESS, "NTP");
    return true;
}

bool isValidIPOrDomain(const String& address) {
    if (address.length() < 7 || address.length() > 253) return false;
    
    // IP adresi kontrolü
    IPAddress testIP;
    if (testIP.fromString(address)) {
        return true;
    }
    
    // Domain adı kontrolü (basit)
    if (address.indexOf('.') > 0 && address.indexOf(' ') == -1) {
        return true;
    }
    
    return false;
}

// ✅ DÜZELTİLMİŞ: NTP ayarlarını Preferences'a kaydet
bool saveNTPSettings(const String& server1, const String& server2, int timezone) {
    if (!isValidIPOrDomain(server1)) {
        addLog("❌ Geçersiz birincil NTP sunucu: " + server1, ERROR, "NTP");
        return false;
    }
    
    if (server2.length() > 0 && !isValidIPOrDomain(server2)) {
        addLog("❌ Geçersiz ikincil NTP sunucu: " + server2, ERROR, "NTP");
        return false;
    }
    
    Preferences preferences;
    preferences.begin("ntp-config", false); // false = read-write mode
    
    // Preferences'a kaydet
    preferences.putString("ntp_server1", server1);
    preferences.putString("ntp_server2", server2);
    preferences.putInt("timezone", timezone);
    preferences.putBool("enabled", true);
    
    // Commit işlemi (ESP32 için)
    preferences.end();
    
    // Debug için kontrol oku
    preferences.begin("ntp-config", true);
    String checkServer1 = preferences.getString("ntp_server1", "");
    String checkServer2 = preferences.getString("ntp_server2", "");
    preferences.end();
    
    addLog("💾 Preferences'a kaydedildi: NTP1=" + checkServer1 + ", NTP2=" + checkServer2, DEBUG, "NTP");
    
    // Global config güncelle
    server1.toCharArray(ntpConfig.ntpServer1, sizeof(ntpConfig.ntpServer1));
    server2.toCharArray(ntpConfig.ntpServer2, sizeof(ntpConfig.ntpServer2));
    ntpConfig.timezone = timezone;
    ntpConfig.enabled = true;
    ntpConfigured = true;
    
    addLog("✅ NTP ayarları kaydedildi: " + server1 + ", " + server2, SUCCESS, "NTP");
    
    // dsPIC33EP'ye gönder
    sendNTPConfigToBackend();
    return true;
}

// NTP Handler başlatma
void initNTPHandler() {
    // NTP ayarları yükleme
    addLog("🚀 NTP Handler başlatılıyor...", INFO, "NTP");
    
    if (!loadNTPSettings()) {
        addLog("⚠️ Kayıtlı NTP ayarı bulunamadı, varsayılanlar kullanılıyor", WARN, "NTP");
    } else {
        addLog("✅ NTP ayarları yüklendi: " + String(ntpConfig.ntpServer1) + ", " + String(ntpConfig.ntpServer2), SUCCESS, "NTP");
    }
    
    // Eğer kayıtlı ayar varsa gönder
    if (ntpConfigured && strlen(ntpConfig.ntpServer1) > 0) {
        delay(1000); // Backend'in hazır olmasını bekle
        sendNTPConfigToBackend();
    }
    
    addLog("✅ NTP Handler başlatıldı", SUCCESS, "NTP");
}

// Eski fonksiyonları inline yap (çoklu tanımlama hatası için)
ReceivedTimeData receivedTime = {.date = "", .time = "", .isValid = false, .lastUpdate = 0};

void processReceivedData() {
    // Bu fonksiyon artık time_sync.cpp tarafından yönetiliyor
}

void readBackendData() {
    // Bu fonksiyon artık time_sync.cpp tarafından yönetiliyor  
}

void parseTimeData(const String& data) {
    // Bu fonksiyon artık time_sync.cpp tarafından yönetiliyor
}

bool isTimeDataValid() {
    return false; // time_sync.cpp'deki isTimeSynced() kullanılacak
}

bool isNTPSynced() {
    return ntpConfigured;
}

void resetNTPSettings() {
    Preferences preferences;
    preferences.begin("ntp-config", false);
    preferences.clear();
    preferences.end();
    
    // Varsayılanları ata
    strcpy(ntpConfig.ntpServer1, "192.168.1.1");
    strcpy(ntpConfig.ntpServer2, "8.8.8.8");
    ntpConfig.timezone = 3;
    ntpConfig.enabled = false;
    ntpConfigured = false;
    
    addLog("🔄 NTP ayarları sıfırlandı ve varsayılanlar yüklendi", INFO, "NTP");
}

// dsPIC'teki NTP ayarları ile Preferences'taki ayarları karşılaştır ve sadece ikinci karta senkronize et
bool syncNTPWithDsPIC() {
    String dspicNtp1, dspicNtp2;
    
    // dsPIC'ten mevcut NTP ayarlarını oku
    if (!requestNTPFromDsPIC(dspicNtp1, dspicNtp2)) {
        addLog("⚠️ dsPIC'ten NTP ayarları okunamadı", WARN, "NTP");
        return false;
    }
    
    // Preferences'tan ön port (WT32-ETH01) ayarlarını oku
    Preferences preferences;
    preferences.begin("ntp-config", true);
    String frontNtp1 = preferences.getString("ntp_server1", "");
    String frontNtp2 = preferences.getString("ntp_server2", "");
    preferences.end();
    
    addLog("🔍 NTP karşılaştırma:", INFO, "NTP");
    addLog("  WT32-ETH01 (Preferences) -> NTP1: " + frontNtp1 + ", NTP2: " + frontNtp2, INFO, "NTP");
    addLog("  dsPIC (UART2)            -> NTP1: " + dspicNtp1 + ", NTP2: " + dspicNtp2, INFO, "NTP");
    
    // Karşılaştır
    bool ntp1Match = (dspicNtp1 == frontNtp1);
    bool ntp2Match = (dspicNtp2 == frontNtp2);
    
    if (ntp1Match && ntp2Match) {
        addLog("✅ NTP ayarları tüm kartlarda senkron (değişiklik yok)", SUCCESS, "NTP");
        return true;
    } else {
        addLog("⚠️ NTP ayarları farklı! WT32-ETH01 ayarları ikinci karta gönderiliyor...", WARN, "NTP");
        
        if (!ntp1Match) {
            addLog("  NTP1 farkı: WT32=" + frontNtp1 + " ≠ dsPIC=" + dspicNtp1, WARN, "NTP");
        }
        if (!ntp2Match) {
            addLog("  NTP2 farkı: WT32=" + frontNtp2 + " ≠ dsPIC=" + dspicNtp2, WARN, "NTP");
        }
        
        // Sadece ikinci karta gönder (UART3)
        bool success = sendNTPToSecondCardOnly(frontNtp1, frontNtp2);
        
        if (success) {
            addLog("✅ NTP senkronizasyonu tamamlandı (ikinci kart güncellendi)", SUCCESS, "NTP");
        } else {
            addLog("⚠️ İkinci karta NTP gönderiminde sorun oluştu", WARN, "NTP");
        }
        
        return success;
    }
}