#include "uart_handler.h"
#include "log_system.h"
#include "settings.h"
#include <Preferences.h>

// UART Pin tanımlamaları
#define UART_RX_PIN 4   // IO4 - RX2
#define UART_TX_PIN 14  // IO14 - TX2
#define UART_PORT   Serial2
#define UART_TIMEOUT 2000
#define UART_QUICK_TIMEOUT 300  // Hızlı sorgular için (YENİ EKLE)
#define MAX_RESPONSE_LENGTH 512 // Daha büyük buffer

#define UART3_RX_PIN 36  // IO36 - RX3
#define UART3_TX_PIN 33  // IO33 - TX3
#define UART3_PORT Serial1  // ESP32'de ikinci donanım UART'ı

static bool uart3Initialized = false;

// Global değişkenler
static unsigned long lastUARTActivity = 0;
static int uartErrorCount = 0;
bool uartHealthy = true;
String lastResponse = "";
UARTStatistics uartStats = {0, 0, 0, 0, 0, 100.0};

// Buffer temizleme
void clearUARTBuffer() {
    delay(50);
    while (UART_PORT.available()) {
        UART_PORT.read();
        delay(1);
    }
}

// UART istatistiklerini güncelle
void updateUARTStats(bool success) {
    if (success) {
        unsigned long total = uartStats.totalFramesSent + uartStats.totalFramesReceived;
        unsigned long errors = uartStats.frameErrors + uartStats.checksumErrors + uartStats.timeoutErrors;
        if (total > 0) {
            uartStats.successRate = ((float)(total - errors) / (float)total) * 100.0;
        }
    } else {
        uartStats.frameErrors++;
        unsigned long total = uartStats.totalFramesSent + uartStats.totalFramesReceived;
        unsigned long errors = uartStats.frameErrors + uartStats.checksumErrors + uartStats.timeoutErrors;
        if (total > 0) {
            uartStats.successRate = ((float)(total - errors) / (float)total) * 100.0;
        }
    }
}

// UART reset
void resetUART() {
    addLog("🔄 UART reset ediliyor...", WARN, "UART");
    
    UART_PORT.end();
    delay(200);
    
    pinMode(UART_RX_PIN, INPUT);
    pinMode(UART_TX_PIN, OUTPUT);
    digitalWrite(UART_TX_PIN, HIGH);
    
    UART_PORT.begin(250000, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    delay(200);
    
    clearUARTBuffer();
    
    lastUARTActivity = millis();
    uartErrorCount = 0;
    uartHealthy = true;
    
    addLog("✅ UART reset tamamlandı", SUCCESS, "UART");
    delay(500);
}

// UART başlatma
void initUART() {
    addLog("🚀 UART başlatılıyor...", INFO, "UART");
    
    pinMode(UART_RX_PIN, INPUT);
    pinMode(UART_TX_PIN, OUTPUT);
    
    UART_PORT.begin(250000, SERIAL_8N1, UART_RX_PIN, UART_TX_PIN);
    
    delay(100);
    clearUARTBuffer();
    
    lastUARTActivity = millis();
    uartErrorCount = 0;
    uartHealthy = true;
    
    addLog("✅ UART başlatıldı - TX2: IO" + String(UART_TX_PIN) + 
           ", RX2: IO" + String(UART_RX_PIN) + 
           ", Baud: 250000", SUCCESS, "UART");
    
    testUARTConnection();
}

// UART bağlantı testi
bool testUARTConnection() {
    addLog("🧪 UART bağlantısı test ediliyor...", INFO, "UART");
    
    if (UART_PORT.available()) {
        String response = "";
        while (UART_PORT.available() && response.length() < 50) {
            char c = UART_PORT.read();
            if (c >= 32 && c <= 126) {
                response += c;
            }
        }
        
        if (response.length() > 0) {
            addLog("✅ UART'da mevcut veri: '" + response + "'", SUCCESS, "UART");
            uartHealthy = true;
            lastUARTActivity = millis();
            return true;
        }
    }
    
    if (UART_PORT) {
        addLog("✅ UART portu aktif", SUCCESS, "UART");
        uartHealthy = true;
        return true;
    } else {
        addLog("❌ UART portu kapalı", ERROR, "UART");
        uartHealthy = false;
        return false;
    }
}

// İYİLEŞTİRİLMİŞ - DAHA GÜVENİLİR UART OKUMA
String safeReadUARTResponse(unsigned long timeout) {
    String response = "";
    response.reserve(MAX_RESPONSE_LENGTH); // Belleği önceden ayır
    
    unsigned long startTime = millis();
    bool dataStarted = false;
    uint8_t buffer[32]; // Buffer boyutunu küçült, daha sık oku
    int consecutiveEmpty = 0; // Art arda boş okuma sayacı
    
    while (millis() - startTime < timeout) {
        int available = UART_PORT.available();
        
        if (available > 0) {
            lastUARTActivity = millis();
            uartHealthy = true;
            dataStarted = true;
            consecutiveEmpty = 0; // Reset empty counter
            
            // CHUNK HALİNDE OKU (daha küçük buffer)
            int toRead = min(available, (int)sizeof(buffer));
            int bytesRead = UART_PORT.readBytes(buffer, toRead);
            
            for (int i = 0; i < bytesRead; i++) {
                char c = buffer[i];
                
                if (c == '\n' || c == '\r') {
                    if (response.length() > 0) {
                        uartStats.totalFramesReceived++;
                        return response;
                    }
                } else if (c >= 32 && c <= 126) {
                    response += c;
                    if (response.length() >= MAX_RESPONSE_LENGTH - 1) {
                        uartStats.totalFramesReceived++;
                        return response;
                    }
                } else if (c != 0) {
                    // Görünmeyen karakterler de veri olabilir (hex data için)
                    response += c;
                }
            }
            
            // Veri gelmeye devam ediyorsa biraz bekle
            delayMicroseconds(100);
            
        } else if (dataStarted) {
            // Veri başlamıştı ama şimdi gelmiyor
            consecutiveEmpty++;
            
            if (consecutiveEmpty > 5) {
                // 5 kez art arda boş, veri tamamlanmış olabilir
                if (response.length() > 0) {
                    uartStats.totalFramesReceived++;
                    return response;
                }
            }
            
            delay(2); // 2ms bekle
        } else {
            // Henüz veri başlamadı
            yield(); // CPU'yu yorma
            delayMicroseconds(500); // 0.5ms bekle
        }
    }
    
    // Timeout oldu
    if (response.length() > 0) {
        // Kısmi veri varsa döndür
        uartStats.totalFramesReceived++;
        return response;
    }
    
    uartStats.timeoutErrors++;
    return response;
}

// ARIZA İÇİN OPTİMİZE EDİLMİŞ ÖZEL OKUMA
String readFaultResponse(unsigned long timeout = 500) {
    String response = "";
    response.reserve(64); // Arıza verisi için yeterli
    
    unsigned long startTime = millis();
    int expectedLength = 22; // Minimum arıza verisi uzunluğu
    
    // Buffer'ı temizle
    while (UART_PORT.available() > 100) {
        UART_PORT.read();
        delayMicroseconds(10);
    }
    
    while (millis() - startTime < timeout) {
        if (UART_PORT.available() > 0) {
            char c = UART_PORT.read();
            
            // Görünür karakter veya veri karakteri
            if ((c >= 32 && c <= 126) || (c >= '0' && c <= '9') || 
                (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f')) {
                response += c;
                
                // Yeterli veri geldiyse döndür
                if (response.length() >= expectedLength) {
                    return response;
                }
            } else if (c == '\n' || c == '\r') {
                // Satır sonu, veri tamamlandı
                if (response.length() > 0) {
                    return response;
                }
            } else if (c == 'E' && response.length() == 0) {
                // Error response
                return "E";
            }
        }
        
        delayMicroseconds(100); // Kısa bekle
    }
    
    return response; // Ne varsa döndür
}

// Özel komut gönderme
bool sendCustomCommand(const String& command, String& response, unsigned long timeout) {
    if (command.length() == 0 || command.length() > 100) {
        return false;
    }
    
    if (!uartHealthy) {
        resetUART();
    }
    
    clearUARTBuffer();
    
    UART_PORT.print(command);
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    response = safeReadUARTResponse(timeout == 0 ? UART_TIMEOUT : timeout);
    
    bool success = response.length() > 0;
    updateUARTStats(success);
    
    if (!success) {
        uartErrorCount++;
    }
    
    return success;
}

// BaudRate değiştirme
bool changeBaudRate(long baudRate) {
    //addLog("ESP32 UART hızı sabit 250000'de kalıyor, sadece dsPIC'e kod gönderiliyor", INFO, "UART");
    return sendBaudRateCommand(baudRate);
}

// BaudRate komutunu gönder
bool sendBaudRateCommand(long baudRate) {
    String command = "";
    
    switch(baudRate) {
        case 9600:   command = "0Br";   break;
        case 19200:  command = "1Br";  break;
        case 38400:  command = "2Br";  break;
        case 57600:  command = "3Br";  break;
        case 115200: command = "4Br"; break;
        default:
            addLog("Geçersiz baudrate: " + String(baudRate), ERROR, "UART");
            return false;
    }
    
    clearUARTBuffer();
    
    UART_PORT.print(command);
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    addLog("dsPIC33EP'ye baudrate kodu gönderildi: " + command, INFO, "UART");
    
    String response = safeReadUARTResponse(2000);
    
    if (response == "ACK" || response.indexOf("OK") >= 0) {
        addLog("✅ Baudrate kodu dsPIC33EP tarafından alındı", SUCCESS, "UART");
        updateUARTStats(true);
        return true;
    } else if (response.length() > 0) {
        addLog("dsPIC33EP yanıtı: " + response, WARN, "UART");
        updateUARTStats(true);
        return true;
    } else {
        addLog("❌ dsPIC33EP'den yanıt alınamadı", ERROR, "UART");
        updateUARTStats(false);
        return false;
    }
}

// uart_handler.cpp içindeki getCurrentBaudRateFromDsPIC fonksiyonu - DÜZELTİLMİŞ

// dsPIC'ten mevcut baudrate değerini al
int getCurrentBaudRateFromDsPIC() {
    clearUARTBuffer();
    
    // BN komutunu gönder
    UART_PORT.print("BN");
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    addLog("📊 Mevcut baudrate sorgulanıyor (BN komutu)", DEBUG, "UART");
    
    String response = safeReadUARTResponse(2000);

    if (response.length() >= 2 && response.charAt(0) == 'B') {
        addLog("📥 Baudrate yanıtı: " + response, DEBUG, "UART");
        
        // ":" varsa ondan sonrasını al, yoksa eski formatı kullan
        int colonIndex = response.indexOf(':');
        String baudStr = (colonIndex >= 0) ? response.substring(colonIndex + 1)
                                           : response.substring(1);

        int baudIndex = baudStr.toInt();
        int baudRate = 0;
        
        switch(baudIndex) {
            case 0: baudRate = 9600; break;
            case 1: baudRate = 19200; break;
            case 2: baudRate = 38400; break;
            case 3: baudRate = 57600; break;
            case 4: baudRate = 115200; break;
            case 5: baudRate = 250000; break;
            default:
                addLog("❌ Tanımlanmamış baudrate indeksi: " + String(baudIndex), ERROR, "UART");
                return -1;
        }
        
        addLog("✅ Mevcut baudrate: " + String(baudRate) + " bps", SUCCESS, "UART");
        updateUARTStats(true);
        return baudRate;
    }
    
    // Buraya geldiysek, yanıt geçersiz veya boş demektir
    addLog("❌ Baudrate yanıtı alınamadı veya geçersiz format: " + response, ERROR, "UART");
    updateUARTStats(false);
    return -1;  // Hata değeri döndür
}


// UART3 başlatma
void initUART3() {
    if (uart3Initialized) return;
    
    pinMode(UART3_RX_PIN, INPUT);
    pinMode(UART3_TX_PIN, OUTPUT);
    
    UART3_PORT.begin(115200, SERIAL_8N1, UART3_RX_PIN, UART3_TX_PIN);
    
    delay(100);
    // Buffer'ı temizle
    while (UART3_PORT.available()) {
        UART3_PORT.read();
        delay(1);
    }
    
    uart3Initialized = true;
    addLog("✅ UART3 başlatıldı - TX: IO33, RX: IO36, Baud: 250000", SUCCESS, "UART3");
}

// İkinci karta veri gönderme - İyileştirilmiş versiyon
bool sendToSecondCard(const String& data) {
    if (!uart3Initialized) {
        initUART3();
        delay(100);
    }
    
    // Buffer'ı temizle
    while (UART3_PORT.available()) {
        UART3_PORT.read();
        delay(1);
    }
    
    // Veriyi gönder
    UART3_PORT.print(data);
    UART3_PORT.flush();
    
    // Biraz bekle - veri gönderimi tamamlansın
    delay(50);
    
    // ACK bekle
    unsigned long startTime = millis();
    String response = "";
    
    while (millis() - startTime < 2000) {
        if (UART3_PORT.available()) {
            char c = UART3_PORT.read();
            
            // Sadece görünür karakterleri al
            if (c >= 32 && c <= 126) {
                response += c;
                
                // ACK kontrolü
                if (response.endsWith("ACK")) {
                    addLog("✅ İkinci karttan ACK alındı", SUCCESS, "UART3");
                    return true;
                }
                
                // Buffer çok büyüdüyse temizle
                if (response.length() > 100) {
                    response = response.substring(response.length() - 50);
                }
            }
        }
        delay(5);
    }
    
    // Debug için alınan veriyi göster
    if (response.length() > 0) {
        addLog("İkinci karttan gelen: " + response, DEBUG, "UART3");
        
        // Hex formatında da göster
        String hexDump = "Hex: ";
        for (unsigned int i = 0; i < response.length() && i < 20; i++) {
            char hex[4];
            sprintf(hex, "%02X ", (unsigned char)response[i]);
            hexDump += hex;
        }
        addLog(hexDump, DEBUG, "UART3");
    }
    
    return false;
}

// ============ YENİ ARIZA SORGULAMA FONKSİYONLARI ============

// Toplam arıza sayısını al (AN komutu)
int getTotalFaultCount() {
    clearUARTBuffer();
    
    UART_PORT.print("AN");
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    addLog("📊 Arıza sayısı sorgulanıyor (AN komutu)", DEBUG, "UART");
    
    String response = safeReadUARTResponse(2000);
    
    if (response.length() >= 2 && response.charAt(0) == 'A') {
        addLog("📥 Gelen yanıt: " + response, DEBUG, "UART");
        
        // A'dan sonrasını sayıya çevir
        String numberStr = response.substring(1);  
        int count = numberStr.toInt();
        
        // 50 - 1 = 49 mantığı
        int actualFaultCount = count - 1;
        
        if (actualFaultCount >= 0) {
            addLog("✅ Toplam arıza sayısı: " + String(actualFaultCount), SUCCESS, "UART");
            updateUARTStats(true);
            return actualFaultCount;
        }
    }
    
    addLog("❌ Arıza sayısı alınamadı veya geçersiz format: " + response, ERROR, "UART");
    updateUARTStats(false);
    return 0;
}

// Belirli bir arıza adresini sorgula
bool requestSpecificFault(int faultNumber) {
    // Önceki veriyi temizle
    lastResponse = "";
    
    // Hızlı buffer temizleme
    int availableBytes = UART_PORT.available();
    if (availableBytes > 0) {
        // Az miktarda veri varsa hızlıca temizle
        if (availableBytes < 50) {
            while (UART_PORT.available()) {
                UART_PORT.read();
                delayMicroseconds(10);
            }
        } else {
            // Çok veri varsa flush et
            UART_PORT.flush();
            delay(5);
            while (UART_PORT.available()) {
                UART_PORT.read();
            }
        }
    }
    
    // Komutu hazırla
    char command[10];
    sprintf(command, "%05dv", faultNumber);
    
    // Gönder
    UART_PORT.print(command);
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    // Kısa bekleme (dsPIC'in hazırlanması için)
    delayMicroseconds(500);
    
    // Özel arıza okuma fonksiyonunu kullan
    lastResponse = readFaultResponse(600); // 600ms timeout
    
    if (lastResponse.length() > 0 && lastResponse != "E") {
        updateUARTStats(true);
        return true;
    } else {
        updateUARTStats(false);
        return false;
    }
}

// İlk arıza kaydını al (geriye uyumluluk için)
bool requestFirstFault() {
    return requestSpecificFault(1);
}


// Son yanıtı al
String getLastFaultResponse() {
    return lastResponse;
}

// Test komutu gönder
bool sendTestCommand(const String& testCmd) {
    clearUARTBuffer();
    
    UART_PORT.print(testCmd);
    UART_PORT.flush();
    
    addLog("🧪 Test komutu gönderildi: " + testCmd, DEBUG, "UART");
    
    String response = safeReadUARTResponse(3000);
    
    if (response.length() > 0) {
        addLog("📡 Test yanıtı: " + response, DEBUG, "UART");
        return true;
    } else {
        addLog("❌ Test komutu için yanıt yok", WARN, "UART");
        return false;
    }
}

// UART sağlık kontrolü
void checkUARTHealth() {
    static unsigned long lastHealthCheck = 0;
    
    if (millis() - lastHealthCheck < 30000) {
        return;
    }
    lastHealthCheck = millis();
    
    // 2 saat sessizlik kontrolü
    if (millis() - lastUARTActivity > 7200000) {
        if (uartHealthy) {
            addLog("⚠️ UART 2 saattir sessiz", WARN, "UART");
            uartHealthy = false;
        }
    }
    
    // Çok fazla hata varsa reset
    if (uartErrorCount > 5) {
        addLog("🔄 Çok fazla UART hatası (" + String(uartErrorCount) + "), reset yapılıyor...", WARN, "UART");
        resetUART();
    }
    
    // Periyodik test
    if (!uartHealthy) {
        addLog("🩺 UART sağlık testi yapılıyor...", INFO, "UART");
        testUARTConnection();
    }
}

// UART durumunu al
String getUARTStatus() {
    String status = "UART Durumu:\n";
    status += "Sağlık: " + String(uartHealthy ? "✅ İyi" : "❌ Kötü") + "\n";
    status += "Son Aktivite: " + String((millis() - lastUARTActivity) / 1000) + " saniye önce\n";
    status += "Hata Sayısı: " + String(uartErrorCount) + "\n";
    status += "Başarı Oranı: " + String(uartStats.successRate, 1) + "%\n";
    status += "Gönderilen: " + String(uartStats.totalFramesSent) + "\n";
    status += "Alınan: " + String(uartStats.totalFramesReceived) + "\n";
    status += "Timeout: " + String(uartStats.timeoutErrors);
    return status;
}

// dsPIC'teki tüm arızaları sil (tT komutu)
bool deleteAllFaultsFromDsPIC() {
    clearUARTBuffer();
    
    // tT komutunu gönder
    UART_PORT.print("tT");
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    addLog("🗑️ dsPIC arızaları siliniyor (tT komutu)", INFO, "UART");
    
    String response = safeReadUARTResponse(3000); // 3 saniye timeout
    
    if (response.length() > 0) {
        addLog("📥 tT komut yanıtı: " + response, DEBUG, "UART");
        
        // Başarılı yanıt kontrolü
        if (response == "OK" || response == "ACK" || response.indexOf("DELETED") >= 0 || response.indexOf("CLEARED") >= 0) {
            addLog("✅ dsPIC arızaları başarıyla silindi", SUCCESS, "UART");
            updateUARTStats(true);
            return true;
        } else if (response == "E" || response.indexOf("ERROR") >= 0) {
            addLog("❌ dsPIC arıza silme hatası: " + response, ERROR, "UART");
            updateUARTStats(false);
            return false;
        } else {
            // Bilinmeyen yanıt ama veri geldi, muhtemelen başarılı
            addLog("⚠️ Beklenmeyen yanıt ama işlem muhtemelen başarılı: " + response, WARN, "UART");
            updateUARTStats(true);
            return true;
        }
    } else {
        addLog("❌ tT komutu için yanıt alınamadı (timeout)", ERROR, "UART");
        updateUARTStats(false);
        return false;
    }
}

// Son N arıza kaydını al (performans optimizasyonlu)
bool requestLastNFaults(int count, std::vector<String>& faultData) {
    // Önce toplam arıza sayısını al
    int totalFaults = getTotalFaultCount();
    
    if (totalFaults == 0) {
        addLog("📊 Sistemde arıza kaydı yok", INFO, "UART");
        return false;
    }
    
    // Son N kaydın başlangıç noktasını hesapla
    int requestCount = min(count, totalFaults);
    int startFault = max(1, totalFaults - requestCount + 1);
    
    addLog("📥 Son " + String(requestCount) + " arıza alınıyor (toplam: " + String(totalFaults) + ")", INFO, "UART");
    
    faultData.clear();
    faultData.reserve(requestCount);
    
    // Son kayıtları al (en yeniden en eskiye)
    for (int i = totalFaults; i >= startFault; i--) {
        if (requestSpecificFault(i)) {
            String response = getLastFaultResponse();
            if (response.length() > 0 && response != "E") {
                faultData.push_back(response);
            }
        }
        
        // Her 10 kayıtta bir kısa mola
        if ((totalFaults - i + 1) % 10 == 0) {
            delay(100);
            yield();
        }
    }
    
    //addLog("✅ " + String(faultData.size()) + " arıza kaydı alındı", SUCCESS, "UART");
    return faultData.size() > 0;
}

// ============ LED DURUM SORGULAMA FONKSİYONU ============

// LED durumunu dsPIC'ten al (LN komutu)
bool requestLEDStatus(String& ledResponse) {
    clearUARTBuffer();
    
    // LN komutunu gönder
    UART_PORT.print("LN");
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    addLog("💡 LED durumu sorgulanıyor (LN komutu)", DEBUG, "UART");
    
    // Hızlı timeout kullan (LED sorgusu hızlı olmalı)
    ledResponse = safeReadUARTResponse(UART_QUICK_TIMEOUT); // 500ms
    
    if (ledResponse.length() > 0) {
        // Format kontrolü: "L:XXXX" olmalı
        if (ledResponse.startsWith("L:") && ledResponse.length() >= 6) {
            addLog("✅ LED durumu alındı: " + ledResponse, DEBUG, "UART");
            updateUARTStats(true);
            lastUARTActivity = millis();
            return true;
        } else {
            addLog("⚠️ Geçersiz LED formatı: " + ledResponse, WARN, "UART");
            updateUARTStats(false);
            return false;
        }
    }
    
    addLog("❌ LED durumu alınamadı (timeout)", ERROR, "UART");
    updateUARTStats(false);
    return false;
}

// LED durumunu otomatik parse et ve detaylı bilgi döndür (eski format - backward compatibility)
bool parseLEDStatus(const String& ledData, uint8_t& inputByte, uint8_t& outputByte) {
    uint8_t alarmByte = 0;
    return parseLEDStatus(ledData, inputByte, outputByte, alarmByte);
}

// LED durumunu otomatik parse et - YENİ FORMAT (L:AABBCC)
bool parseLEDStatus(const String& ledData, uint8_t& inputByte, uint8_t& outputByte, uint8_t& alarmByte) {
    // Format: "L:AABBCC"
    // AA = Input byte (2 hex digits)
    // BB = Output byte (2 hex digits)
    // CC = Alarm byte (2 hex digits) - OPSİYONEL

    if (!ledData.startsWith("L:") || ledData.length() < 6) {
        return false;
    }

    // "L:" kısmını atla
    String hexData = ledData.substring(2);
    hexData.trim();

    // Minimum 4 karakter olmalı (AABB)
    if (hexData.length() < 4) {
        return false;
    }

    // İlk 2 hex digit -> Input
    String inputHex = hexData.substring(0, 2);
    inputByte = (uint8_t)strtol(inputHex.c_str(), NULL, 16);

    // Sonraki 2 hex digit -> Output
    String outputHex = hexData.substring(2, 4);
    outputByte = (uint8_t)strtol(outputHex.c_str(), NULL, 16);

    // Eğer 6 karakter varsa (AABBCC), son 2 karakter -> Alarm
    if (hexData.length() >= 6) {
        String alarmHex = hexData.substring(4, 6);
        alarmByte = (uint8_t)strtol(alarmHex.c_str(), NULL, 16);

        // Debug log (alarm dahil)
        addLog("📊 LED Parse: IN=0x" + String(inputByte, HEX) +
               " (0b" + String(inputByte, BIN) + "), OUT=0x" + String(outputByte, HEX) +
               " (0b" + String(outputByte, BIN) + "), ALARM=0x" + String(alarmByte, HEX) +
               " (0b" + String(alarmByte, BIN) + ")", DEBUG, "UART");
    } else {
        // Eski format, alarm yok
        alarmByte = 0;

        // Debug log (alarm olmadan)
        addLog("📊 LED Parse: IN=0x" + String(inputByte, HEX) +
               " (0b" + String(inputByte, BIN) + "), OUT=0x" + String(outputByte, HEX) +
               " (0b" + String(outputByte, BIN) + ")", DEBUG, "UART");
    }

    return true;
}

// LED durumunu insan okunabilir formatta al
String getLEDStatusReadable() {
    String ledData;
    if (!requestLEDStatus(ledData)) {
        return "LED durumu alınamadı";
    }

    uint8_t inputByte = 0, outputByte = 0, alarmByte = 0;
    if (!parseLEDStatus(ledData, inputByte, outputByte, alarmByte)) {
        return "LED verisi parse edilemedi: " + ledData;
    }

    String status = "LED Durumu [" + ledData + "]:\n";

    // Input LED'leri
    status += "INPUT: ";
    int activeInputs = 0;
    for (int i = 0; i < 8; i++) {
        if (inputByte & (1 << i)) {
            status += "I" + String(i + 1) + " ";
            activeInputs++;
        }
    }
    status += "(" + String(activeInputs) + "/8)\n";

    // Output LED'leri
    status += "OUTPUT: ";
    int activeOutputs = 0;
    for (int i = 0; i < 8; i++) {
        if (outputByte & (1 << i)) {
            status += "O" + String(i + 1) + " ";
            activeOutputs++;
        }
    }
    status += "(" + String(activeOutputs) + "/8)\n";

    // Alarm LED'leri (eğer varsa)
    if (alarmByte != 0) {
        status += "ALARM: ";

        // Alarm byte formatı:
        // Bit 6 (0x40) = NTP Alarm
        // Bit 5 (0x20) = DC2 Alarm
        // Bit 4 (0x10) = DC1 Alarm
        // Bit 1,2,3 (0x02,0x06,0x08) = RS232 Alarm

        bool hasAlarm = false;

        if (alarmByte & 0x40) {
            status += "NTP ";
            hasAlarm = true;
        }
        if (alarmByte & 0x20) {
            status += "DC2 ";
            hasAlarm = true;
        }
        if (alarmByte & 0x10) {
            status += "DC1 ";
            hasAlarm = true;
        }
        if ((alarmByte & 0x02) || (alarmByte & 0x04) || (alarmByte & 0x08)) {
            status += "RS232 ";
            hasAlarm = true;
        }

        if (!hasAlarm) {
            status += "YOK";
        }
    } else {
        status += "ALARM: YOK";
    }

    return status;
}

// NTP ayarlarını dsPIC'ten oku (XN komutu)
bool requestNTPFromDsPIC(String& ntp1, String& ntp2) {
    clearUARTBuffer();
    
    // XN komutunu gönder
    UART_PORT.print("XN");
    UART_PORT.flush();
    
    uartStats.totalFramesSent++;
    
    addLog("📡 NTP ayarları dsPIC'ten sorgulanıyor (XN komutu)", DEBUG, "UART");
    
    String response = safeReadUARTResponse(2000);
    
    if (response.length() > 0 && response.startsWith("X:")) {
        addLog("📥 NTP yanıtı: " + response, DEBUG, "UART");
        
        // Format: X:19216800011801921680002180
        // X: sonrası 26 karakter olmalı (192168001180 + 192168000218 + 0)
        String ntpData = response.substring(2); // "X:" kısmını atla
        ntpData.trim();
        
        if (ntpData.length() >= 24) {
            // İlk 12 karakter NTP1
            String ntp1Str = ntpData.substring(0, 12);
            
            // Sonraki 12 karakter NTP2
            String ntp2Str = ntpData.substring(12, 24);
            
            // Parse et: 192168001180 -> 192.168.1.180
            if (ntp1Str.length() == 12) {
                String o1 = String(ntp1Str.substring(0, 3).toInt());
                String o2 = String(ntp1Str.substring(3, 6).toInt());
                String o3 = String(ntp1Str.substring(6, 9).toInt());
                String o4 = String(ntp1Str.substring(9, 12).toInt());
                ntp1 = o1 + "." + o2 + "." + o3 + "." + o4;
            }
            
            if (ntp2Str.length() == 12) {
                String o1 = String(ntp2Str.substring(0, 3).toInt());
                String o2 = String(ntp2Str.substring(3, 6).toInt());
                String o3 = String(ntp2Str.substring(6, 9).toInt());
                String o4 = String(ntp2Str.substring(9, 12).toInt());
                ntp2 = o1 + "." + o2 + "." + o3 + "." + o4;
            }
            
            addLog("✅ NTP1: " + ntp1 + ", NTP2: " + ntp2, SUCCESS, "UART");
            updateUARTStats(true);
            return true;
        } else {
            addLog("❌ NTP veri formatı hatalı (uzunluk: " + String(ntpData.length()) + ")", ERROR, "UART");
            updateUARTStats(false);
            return false;
        }
    }
    
    addLog("❌ NTP ayarları alınamadı", ERROR, "UART");
    updateUARTStats(false);
    return false;
}

// NTP ayarlarını sadece ikinci karta gönder (UART3)
bool sendNTPToSecondCardOnly(const String& ntp1, const String& ntp2) {
    if (!uart3Initialized) {
        initUART3();
        delay(100);
    }
    
    addLog("📤 NTP ayarları sadece ikinci karta gönderiliyor...", INFO, "UART3");
    
    bool allSuccess = true;
    
    // NTP1 için format dönüşümü
    int dot1 = ntp1.indexOf('.');
    int dot2 = ntp1.indexOf('.', dot1 + 1);
    int dot3 = ntp1.indexOf('.', dot2 + 1);
    
    if (dot1 != -1 && dot2 != -1 && dot3 != -1) {
        String octet1 = ntp1.substring(0, dot1);
        String octet2 = ntp1.substring(dot1 + 1, dot2);
        String octet3 = ntp1.substring(dot2 + 1, dot3);
        String octet4 = ntp1.substring(dot3 + 1);
        
        int o1 = octet1.toInt();
        int o2 = octet2.toInt();
        int o3 = octet3.toInt();
        int o4 = octet4.toInt();
        
        // İlk iki oktet'i birleştir
        char buffer1[7];
        sprintf(buffer1, "%03d%03d", o1, o2);
        String ntp1_part1 = String(buffer1);
        
        // Son iki oktet'i birleştir
        char buffer2[7];
        sprintf(buffer2, "%03d%03d", o3, o4);
        String ntp1_part2 = String(buffer2);
        
        // NTP1 Part1 komutu: 192168u
        String cmd1 = ntp1_part1 + "u";
        bool sent1 = false;
        for (int retry = 0; retry < 3 && !sent1; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("NTP1 Part1 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "UART3");
            }
            
            if (sendToSecondCard(cmd1)) {
                addLog("✅ NTP1 Part1 ikinci karta gönderildi: " + cmd1, SUCCESS, "UART3");
                sent1 = true;
            }
        }
        
        if (!sent1) {
            addLog("❌ NTP1 Part1 ikinci karta gönderilemedi: " + cmd1, ERROR, "UART3");
            allSuccess = false;
        }
        
        delay(100);
        
        // NTP1 Part2 komutu: 001180y
        String cmd2 = ntp1_part2 + "y";
        bool sent2 = false;
        for (int retry = 0; retry < 3 && !sent2; retry++) {
            if (retry > 0) {
                delay(100);
                addLog("NTP1 Part2 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "UART3");
            }
            
            if (sendToSecondCard(cmd2)) {
                addLog("✅ NTP1 Part2 ikinci karta gönderildi: " + cmd2, SUCCESS, "UART3");
                sent2 = true;
            }
        }
        
        if (!sent2) {
            addLog("❌ NTP1 Part2 ikinci karta gönderilemedi: " + cmd2, ERROR, "UART3");
            allSuccess = false;
        }
    } else {
        addLog("❌ NTP1 format dönüşümü başarısız: " + ntp1, ERROR, "UART3");
        allSuccess = false;
    }
    
    // NTP2 varsa gönder
    if (ntp2.length() > 0 && ntp2 != "0.0.0.0") {
        delay(100);
        
        dot1 = ntp2.indexOf('.');
        dot2 = ntp2.indexOf('.', dot1 + 1);
        dot3 = ntp2.indexOf('.', dot2 + 1);
        
        if (dot1 != -1 && dot2 != -1 && dot3 != -1) {
            String octet1 = ntp2.substring(0, dot1);
            String octet2 = ntp2.substring(dot1 + 1, dot2);
            String octet3 = ntp2.substring(dot2 + 1, dot3);
            String octet4 = ntp2.substring(dot3 + 1);
            
            int o1 = octet1.toInt();
            int o2 = octet2.toInt();
            int o3 = octet3.toInt();
            int o4 = octet4.toInt();
            
            char buffer1[7];
            sprintf(buffer1, "%03d%03d", o1, o2);
            String ntp2_part1 = String(buffer1);
            
            char buffer2[7];
            sprintf(buffer2, "%03d%03d", o3, o4);
            String ntp2_part2 = String(buffer2);
            
            // NTP2 Part1 komutu: 192168w
            String cmd3 = ntp2_part1 + "w";
            bool sent3 = false;
            for (int retry = 0; retry < 3 && !sent3; retry++) {
                if (retry > 0) {
                    delay(100);
                    addLog("NTP2 Part1 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "UART3");
                }
                
                if (sendToSecondCard(cmd3)) {
                    addLog("✅ NTP2 Part1 ikinci karta gönderildi: " + cmd3, SUCCESS, "UART3");
                    sent3 = true;
                }
            }
            
            if (!sent3) {
                addLog("❌ NTP2 Part1 ikinci karta gönderilemedi: " + cmd3, ERROR, "UART3");
                allSuccess = false;
            }
            
            delay(100);
            
            // NTP2 Part2 komutu: 002180x
            String cmd4 = ntp2_part2 + "x";
            bool sent4 = false;
            for (int retry = 0; retry < 3 && !sent4; retry++) {
                if (retry > 0) {
                    delay(100);
                    addLog("NTP2 Part2 tekrar deneniyor (" + String(retry + 1) + "/3)", DEBUG, "UART3");
                }
                
                if (sendToSecondCard(cmd4)) {
                    addLog("✅ NTP2 Part2 ikinci karta gönderildi: " + cmd4, SUCCESS, "UART3");
                    sent4 = true;
                }
            }
            
            if (!sent4) {
                addLog("❌ NTP2 Part2 ikinci karta gönderilemedi: " + cmd4, ERROR, "UART3");
                allSuccess = false;
            }
        } else {
            addLog("❌ NTP2 format dönüşümü başarısız: " + ntp2, ERROR, "UART3");
            allSuccess = false;
        }
    }
    
    if (allSuccess) {
        addLog("✅ Tüm NTP ayarları başarıyla ikinci karta gönderildi", SUCCESS, "UART3");
    } else {
        addLog("⚠️ NTP ayarları kısmen ikinci karta gönderildi", WARN, "UART3");
    }
    
    return allSuccess;
}