#include "crypto_utils.h"
#include "mbedtls/sha256.h"
#include <Arduino.h>

String sha256(const String& data, const String& salt) {
    if (data.length() == 0 || salt.length() == 0) {
        return "";
    }
    
    String toHash = salt + data;
    byte hashResult[32];
    
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const unsigned char*) toHash.c_str(), toHash.length());
    mbedtls_sha256_finish(&ctx, hashResult);
    mbedtls_sha256_free(&ctx);

    char hexString[65];
    for (int i = 0; i < 32; i++) {
        sprintf(hexString + i * 2, "%02x", hashResult[i]);
    }
    hexString[64] = '\0';
    
    return String(hexString);
}

// Jeton (token) ve tuz (salt) üretmek için daha güvenli fonksiyon
String generateRandomToken(int length) {
    if (length <= 0 || length > 64) {
        length = 32; // Jeton için daha uzun bir varsayılan
    }
    
    String token = "";
    const char charset[] = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    for (int i = 0; i < length; i++) {
        uint32_t randomNum = esp_random();
        token += charset[randomNum % (sizeof(charset) - 1)];
    }
    
    return token;
}