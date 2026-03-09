#ifndef CRYPTO_UTILS_H
#define CRYPTO_UTILS_H

#include <Arduino.h>

String sha256(const String& data, const String& salt);
// Fonksiyon adını daha anlaşılır hale getirelim
String generateRandomToken(int length = 32); 

#endif