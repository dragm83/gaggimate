#ifndef WEBDAV_UTILS_H
#define WEBDAV_UTILS_H

#include <Arduino.h>
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <SPIFFS.h>

class WebDAVUtils {
public:
    static String normalizeBase(const String& raw);
    static String urlJoin(const String& baseUrl, const String& path);
    static String httpGetString(const String& baseUrl, const String& path);
    static bool httpPostJson(const String& baseUrl, const String& path, const String& json);
    static bool httpDelete(const String& baseUrl, const String& path);
    static bool httpUploadFile(const String& baseUrl, const String& path, File& file, const String& shotId = "");
};

#endif