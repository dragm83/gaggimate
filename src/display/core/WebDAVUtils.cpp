#include "WebDAVUtils.h"
#include <WiFi.h>

String WebDAVUtils::normalizeBase(const String& raw) {
    if (!raw.length()) return "";
    String b = raw;
    if (!b.startsWith("http://") && !b.startsWith("https://")) b = "http://" + b;
    while (b.endsWith("/")) b.remove(b.length()-1);
    return b;
}

String WebDAVUtils::urlJoin(const String& baseUrl, const String& path) {
    const String b = normalizeBase(baseUrl);
    if (!b.length()) return "";
    return path.startsWith("/") ? (b + path) : (b + "/" + path);
}

String WebDAVUtils::httpGetString(const String& baseUrl, const String& path) {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("WebDAV", "WiFi not connected; skipping GET request");
        return "";
    }
    
    String url = urlJoin(baseUrl, path);
    if (!url.length()) return "";
    
    HTTPClient http;
    WiFiClient client;
    String out;
    
    if (!http.begin(client, url)) return out;
    int code = http.GET();
    if (code == 200) out = http.getString();
    http.end();
    return out;
}

bool WebDAVUtils::httpPostJson(const String& baseUrl, const String& path, const String& json) {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("WebDAV", "WiFi not connected; skipping POST request");
        return false;
    }
    
    String url = urlJoin(baseUrl, path);
    if (!url.length()) return false;
    
    HTTPClient http;
    WiFiClient client;
    
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    int code = http.POST((uint8_t*)json.c_str(), json.length());
    http.end();
    return (code >= 200 && code < 300);
}

bool WebDAVUtils::httpDelete(const String& baseUrl, const String& path) {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("WebDAV", "WiFi not connected; skipping DELETE request");
        return false;
    }
    
    String url = urlJoin(baseUrl, path);
    if (!url.length()) return false;
    
    HTTPClient http;
    WiFiClient client;
    
    if (!http.begin(client, url)) return false;
    int code = http.sendRequest("DELETE");
    http.end();
    return (code >= 200 && code < 300);
}

bool WebDAVUtils::httpUploadFile(const String& baseUrl, const String& path, File& file, const String& shotId) {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("WebDAV", "WiFi not connected; skipping upload");
        return false;
    }
    
    String url = urlJoin(baseUrl, path);
    if (!url.length()) return false;
    
    HTTPClient http;
    WiFiClient client;
    
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "text/plain");
    if (shotId.length() > 0) {
        http.addHeader("X-Shot-Id", shotId);
    }
    
    const size_t len = file.size();
    int httpCode = http.sendRequest("POST", &file, len);
    
    http.end();
    if (httpCode < 200 || httpCode >= 300) {
        ESP_LOGW("WebDAV", "Upload failed, HTTP code: %d", httpCode);
        return false;
    }
    return true;
}