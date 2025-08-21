#include "ShotHistoryPlugin.h"

#include <SPIFFS.h>
#include <display/core/Controller.h>
#include <display/core/ProfileManager.h>
#include <display/core/utils.h>
#include <WiFi.h>
#include <HTTPClient.h>

ShotHistoryPlugin ShotHistory;

void ShotHistoryPlugin::setup(Controller *c, PluginManager *pm) {
    controller = c;
    pluginManager = pm;
    pm->on("controller:brew:start", [this](Event const &) { startRecording(); });
    pm->on("controller:brew:end", [this](Event const &) { endRecording(); });
    pm->on("controller:volumetric-measurement:estimation:change",
           [this](Event const &event) { currentEstimatedWeight = event.getFloat("value"); });
    pm->on("controller:volumetric-measurement:bluetooth:change", [this](Event const &event) {
        const float weight = event.getFloat("value");
        const unsigned long now = millis();
        if (lastVolumeSample != 0) {
            const unsigned long timeDiff = now - lastVolumeSample;
            const float volumeDiff = weight - currentBluetoothWeight;
            const float volumeFlow = volumeDiff / static_cast<float>(timeDiff) * 1000.0f;
            currentBluetoothFlow = currentBluetoothFlow * 0.9f + volumeFlow * 0.1f;
        }
        lastVolumeSample = now;
        currentBluetoothWeight = weight;
    });
    pm->on("boiler:currentTemperature:change", [this](Event const &event) { currentTemperature = event.getFloat("value"); });
    xTaskCreatePinnedToCore(loopTask, "ShotHistoryPlugin::loop", configMINIMAL_STACK_SIZE * 3, this, 1, &taskHandle, 0);
}

void ShotHistoryPlugin::record() {
    static File file;
    if (recording && controller->getMode() == MODE_BREW) {
        if (!isFileOpen) {
            if (!SPIFFS.exists("/h")) {
                SPIFFS.mkdir("/h");
            }
            file = SPIFFS.open("/h/" + currentId + ".dat", FILE_APPEND);
            if (file) {
                isFileOpen = true;
            }
        }
        if (!headerWritten) {
            file.printf("1,%s,%ld\n", currentProfileName.c_str(), getTime());
            headerWritten = true;
        }
        ShotSample s{millis() - shotStart,
                     controller->getTargetTemp(),
                     currentTemperature,
                     controller->getTargetPressure(),
                     controller->getCurrentPressure(),
                     controller->getCurrentPumpFlow(),
                     controller->getTargetFlow(),
                     controller->getCurrentPuckFlow(),
                     currentBluetoothFlow,
                     currentBluetoothWeight,
                     currentEstimatedWeight};
        if (isFileOpen) {
            file.println(s.serialize().c_str());
        }
    }
    if (!recording && isFileOpen) {
        file.close();
        isFileOpen = false;
        unsigned long duration = millis() - shotStart;
        if (duration <= 7500) { // Exclude failed shots and flushes
            SPIFFS.remove("/h/" + currentId + ".dat");
        } else {
            controller->getSettings().setHistoryIndex(controller->getSettings().getHistoryIndex() + 1);
            cleanupHistory();
        }
    }
}

void ShotHistoryPlugin::startRecording() {
    currentId = controller->getSettings().getHistoryIndex();
    while (currentId.length() < 6) {
        currentId = "0" + currentId;
    }
    shotStart = millis();
    lastVolumeSample = 0;
    currentBluetoothWeight = 0.0f;
    currentEstimatedWeight = 0.0f;
    currentBluetoothFlow = 0.0f;
    currentProfileName = controller->getProfileManager()->getSelectedProfile().label;
    recording = true;
    headerWritten = false;
}

unsigned long ShotHistoryPlugin::getTime() {
    time_t now;
    time(&now);
    return now;
}

void ShotHistoryPlugin::endRecording() { recording = false; }

void ShotHistoryPlugin::cleanupHistory() {
    File directory = SPIFFS.open("/h");
    std::vector<String> entries;
    String filename = directory.getNextFileName();
    while (filename != "") {
        entries.push_back(filename);
        filename = directory.getNextFileName();
    }
    sort(entries.begin(), entries.end(), [](String a, String b) { return a < b; });
    if (entries.size() > MAX_HISTORY_ENTRIES) {
        for (unsigned int i = 0; i < entries.size() - MAX_HISTORY_ENTRIES; i++) {
            String name = entries[i];
            SPIFFS.remove(name);
        }
    }
}

void ShotHistoryPlugin::handleRequest(JsonDocument &request, JsonDocument &response) {
    String type = request["tp"].as<String>();
    response["tp"] = String("res:") + type.substring(4);
    response["rid"] = request["rid"].as<String>();

    if (type == "req:history:list") {
        JsonArray arr = response["history"].to<JsonArray>();
        File root = SPIFFS.open("/h");
        if (root && root.isDirectory()) {
            File file = root.openNextFile();
            while (file) {
                if (String(file.name()).endsWith(".dat")) {
                    auto o = arr.add<JsonObject>();
                    auto name = String(file.name());
                    int start = name.lastIndexOf('/') + 1;
                    int end = name.lastIndexOf('.');
                    o["id"] = name.substring(start, end);
                    o["history"] = file.readString();
                }
                file = root.openNextFile();
            }
        }
    } else if (type == "req:history:get") {
        auto id = request["id"].as<String>();
        File file = SPIFFS.open("/h/" + id + ".dat", "r");
        if (file) {
            String data = file.readString();
            response["history"] = data;
            file.close();
        } else {
            response["error"] = "not found";
        }
    } else if (type == "req:history:delete") {
        auto id = request["id"].as<String>();
        SPIFFS.remove("/h/" + id + ".dat");
        response["msg"] = "Ok";
    }
}

void ShotHistoryPlugin::loopTask(void *arg) {
    auto *plugin = static_cast<ShotHistoryPlugin *>(arg);
    while (true) {
        plugin->record();
        vTaskDelay(250 / portTICK_PERIOD_MS);
    }
}


// Webdav helpers

static String normalizeBase(const String& raw) {
  if (!raw.length()) return "";
  String b = raw;
  if (!b.startsWith("http://") && !b.startsWith("https://")) b = "http://" + b;
  while (b.endsWith("/")) b.remove(b.length()-1);
  return b;
}

String ShotHistoryPlugin::baseUrl() const {
  return normalizeBase(controller->getSettings().getStoreServer());  // SAME as ProfileManager
}

String ShotHistoryPlugin::urlJoin(const String& path) const {
  const String b = baseUrl();
  if (!b.length()) return "";
  return path.startsWith("/") ? (b + path) : (b + "/" + path);
}

String ShotHistoryPlugin::httpGetString(const String& path) const {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("ShotHistory", "WiFi not connected; skipping GET request");
        return "";
    }
    
    String url = urlJoin(path);
    if (!url.length()) return "";           // no remote configured
    HTTPClient http; WiFiClient client;
    String out;
    if (!http.begin(client, url)) return out;
    int code = http.GET();
    if (code == 200) out = http.getString();
    http.end();
    return out;
}

bool ShotHistoryPlugin::httpPostJson(const String& path, const String& json) const {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("ShotHistory", "WiFi not connected; skipping POST request");
        return false;
    }
    
    String url = urlJoin(path);
    if (!url.length()) return false;
    HTTPClient http; WiFiClient client;
    if (!http.begin(client, url)) return false;
    http.addHeader("Content-Type", "application/json");
    int code = http.POST((uint8_t*)json.c_str(), json.length());
    http.end();
    return (code >= 200 && code < 300);
}

bool ShotHistoryPlugin::httpDelete(const String& path) const {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("ShotHistory", "WiFi not connected; skipping DELETE request");
        return false;
    }
    
    String url = urlJoin(path);
    if (!url.length()) return false;
    HTTPClient http; WiFiClient client;
    if (!http.begin(client, url)) return false;
    int code = http.sendRequest("DELETE");
    http.end();
    return (code >= 200 && code < 300);
}

bool ShotHistoryPlugin::uploadShotToNAS(const String& id) {
    if (WiFi.status() != WL_CONNECTED) {
        ESP_LOGW("ShotHistory", "WiFi not connected; keeping shot locally");
        return false;
    }
    
    const String upUrl = urlJoin("/upload");
    if (!upUrl.length()) {
        ESP_LOGW("ShotHistory", "No store server set; keeping shot locally");
        return false;
    }

    const String path = "/h/" + id + ".dat";
    File f = SPIFFS.open(path, FILE_READ);
    if (!f) return false;

    HTTPClient http; WiFiClient client;
    if (!http.begin(client, upUrl)) { f.close(); return false; }
    http.addHeader("Content-Type", "text/plain");
    http.addHeader("X-Shot-Id", id.c_str());

    const size_t len = f.size();
    int httpCode = http.sendRequest("POST", &f, len);

    f.close();
    http.end();
    if (httpCode < 200 || httpCode >= 300) {
        ESP_LOGW("ShotHistory", "NAS upload failed, HTTP code: %d", httpCode);
        return false;
    }
    return true;
}

void ShotHistoryPlugin::syncHistoryIndex() {
    File root = SPIFFS.open("/h");
    if (!root || !root.isDirectory()) return;

    int maxIdx = 0;
    File file = root.openNextFile();
    while (file) {
        String name = file.name();
        if (name.endsWith(".dat")) {
            int num = name.substring(3, name.length() - 4).toInt(); // assuming "/h/00042.dat"
            if (num > maxIdx) maxIdx = num;
        }
        file = root.openNextFile();
    }

    int stored = controller->getSettings().getHistoryIndex();
    if (maxIdx > stored) {
        controller->getSettings().setHistoryIndex(maxIdx);
    }
}