#include "ProfileManager.h"
#include <ArduinoJson.h>
#include <display/core/WebDAVUtils.h>
#include <utility>
#include <set>

ProfileManager::ProfileManager(fs::FS &fs, String dir, Settings &settings, PluginManager *plugin_manager)
    : _plugin_manager(plugin_manager), _settings(settings), _fs(fs), _dir(std::move(dir)) {}

void ProfileManager::setup() {
    ensureDirectory();
    auto profiles = listProfiles();
    if (!_settings.isProfilesMigrated() || profiles.empty()) {
        migrate();
        _settings.setProfilesMigrated(true);
    }
    loadSelectedProfile(selectedProfile);
    _settings.setFavoritedProfiles(getFavoritedProfiles(true));
}

bool ProfileManager::ensureDirectory() const {
    if (!_fs.exists(_dir)) {
        return _fs.mkdir(_dir);
    }
    return true;
}

String ProfileManager::profilePath(const String &uuid) const { return _dir + "/" + uuid + ".json"; }

void ProfileManager::migrate() {
    Profile profile{};
    profile.id = generateShortID();
    profile.label = "Default";
    profile.description = "Default profile generated from previous settings";
    profile.temperature = _settings.getTargetBrewTemp();
    profile.type = "standard";
    if (_settings.getPressurizeTime() > 0) {
        Phase pressurizePhase1{};
        pressurizePhase1.name = "Pressurize";
        pressurizePhase1.phase = PhaseType::PHASE_TYPE_PREINFUSION;
        pressurizePhase1.valve = 0;
        pressurizePhase1.duration = _settings.getPressurizeTime() / 1000;
        pressurizePhase1.pumpIsSimple = true;
        pressurizePhase1.pumpSimple = 100;
        profile.phases.push_back(pressurizePhase1);
    }
    if (_settings.getInfusePumpTime() > 0) {
        Phase infusePumpPhase{};
        infusePumpPhase.name = "Bloom";
        infusePumpPhase.phase = PhaseType::PHASE_TYPE_BREW;
        infusePumpPhase.valve = 1;
        infusePumpPhase.duration = _settings.getInfusePumpTime() / 1000;
        infusePumpPhase.pumpIsSimple = true;
        infusePumpPhase.pumpSimple = 100;
        profile.phases.push_back(infusePumpPhase);
    }
    if (_settings.getInfuseBloomTime() > 0) {
        Phase infuseBloomPhase1{};
        infuseBloomPhase1.name = "Bloom";
        infuseBloomPhase1.phase = PhaseType::PHASE_TYPE_BREW;
        infuseBloomPhase1.valve = 1;
        infuseBloomPhase1.duration = _settings.getInfuseBloomTime() / 1000;
        infuseBloomPhase1.pumpIsSimple = true;
        infuseBloomPhase1.pumpSimple = 0;
        profile.phases.push_back(infuseBloomPhase1);
    }
    if (_settings.getPressurizeTime() > 0) {
        Phase pressurizePhase1{};
        pressurizePhase1.name = "Pressurize";
        pressurizePhase1.phase = PhaseType::PHASE_TYPE_BREW;
        pressurizePhase1.valve = 0;
        pressurizePhase1.duration = _settings.getPressurizeTime() / 1000;
        pressurizePhase1.pumpIsSimple = true;
        pressurizePhase1.pumpSimple = 100;
        profile.phases.push_back(pressurizePhase1);
    }
    Phase brewPhase{};
    brewPhase.name = "Brew";
    brewPhase.phase = PhaseType::PHASE_TYPE_BREW;
    brewPhase.valve = 1;
    brewPhase.duration = _settings.getTargetDuration() / 1000;
    brewPhase.pumpIsSimple = true;
    brewPhase.pumpSimple = 100;
    Target target{};
    target.type = TargetType::TARGET_TYPE_VOLUMETRIC;
    target.value = _settings.getTargetVolume();
    brewPhase.targets.push_back(target);
    profile.phases.push_back(brewPhase);
    saveProfile(profile);
    _settings.setSelectedProfile(profile.id);
    _settings.addFavoritedProfile(profile.id);
}

std::vector<String> ProfileManager::listProfiles() {
    std::vector<String> uuids;
    std::set<String> uniqueUuids; // To avoid duplicates

    // First, try to get profiles from WebDAV
    std::vector<String> remoteUuids = listRemoteProfiles();
    for (const String& uuid : remoteUuids) {
        if (uniqueUuids.find(uuid.c_str()) == uniqueUuids.end()) {
            uuids.push_back(uuid);
            uniqueUuids.insert(uuid.c_str());
        }
    }

    // Then add local profiles that aren't already in the list
    File root = _fs.open(_dir);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            String name = file.name();
            if (name.endsWith(".json")) {
                int start = name.lastIndexOf('/') + 1;
                int end = name.lastIndexOf('.');
                String uuid = name.substring(start, end);
                if (uniqueUuids.find(uuid.c_str()) == uniqueUuids.end()) {
                    uuids.push_back(uuid);
                    uniqueUuids.insert(uuid.c_str());
                }
            }
            file = root.openNextFile();
        }
    }

    std::vector<String> ordered;
    auto stored = _settings.getProfileOrder();
    for (auto const &id : stored) {
        if (std::find(uuids.begin(), uuids.end(), id) != uuids.end() &&
            std::find(ordered.begin(), ordered.end(), id) == ordered.end()) {
            ordered.push_back(id);
        }
    }
    for (auto const &id : uuids) {
        if (std::find(ordered.begin(), ordered.end(), id) == ordered.end()) {
            ordered.push_back(id);
        }
    }
    return ordered;
}

bool ProfileManager::loadProfile(const String &uuid, Profile &outProfile) {
    // Try remote first
    if (loadRemoteProfile(uuid, outProfile)) {
        // Save a local backup if we got it from remote
        File file = _fs.open(profilePath(uuid), "w");
        if (file) {
            JsonDocument doc;
            JsonObject obj = doc.to<JsonObject>();
            writeProfile(obj, outProfile);
            serializeJson(doc, file);
            file.close();
        }
        
        // Set metadata
        outProfile.selected = outProfile.id == _settings.getSelectedProfile();
        std::vector<String> favoritedProfiles = _settings.getFavoritedProfiles();
        outProfile.favorite = std::find(favoritedProfiles.begin(), favoritedProfiles.end(), outProfile.id) != favoritedProfiles.end();
        return true;
    }
    
    // Fallback to local
    File file = _fs.open(profilePath(uuid), "r");
    if (!file)
        return false;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, file);
    file.close();
    if (err)
        return false;

    if (!parseProfile(doc.as<JsonObject>(), outProfile)) {
        return false;
    }
    outProfile.selected = outProfile.id == _settings.getSelectedProfile();
    std::vector<String> favoritedProfiles = _settings.getFavoritedProfiles();
    outProfile.favorite = std::find(favoritedProfiles.begin(), favoritedProfiles.end(), outProfile.id) != favoritedProfiles.end();
    return true;
}

bool ProfileManager::saveProfile(Profile &profile) {
    if (!ensureDirectory())
        return false;
    bool isNew = false;

    if (profile.id == nullptr || profile.id.isEmpty()) {
        profile.id = generateShortID();
        isNew = true;
    }

    ESP_LOGI("ProfileManager", "Saving profile %s", profile.id.c_str());

    // Save to local SPIFFS first
    File file = _fs.open(profilePath(profile.id), "w");
    if (!file)
        return false;

    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    writeProfile(obj, profile);

    bool localSaved = serializeJson(doc, file) > 0;
    file.close();
    
    if (!localSaved) {
        ESP_LOGW("ProfileManager", "Failed to save profile %s locally", profile.id.c_str());
        return false;
    }

    // Try to save to WebDAV
    bool remoteSaved = saveProfileToWebDAV(profile);
    if (remoteSaved) {
        ESP_LOGI("ProfileManager", "Profile %s saved to both local and remote", profile.id.c_str());
    } else {
        ESP_LOGW("ProfileManager", "Profile %s saved locally only, remote save failed", profile.id.c_str());
    }

    // Update internal state
    if (profile.id == selectedProfile.id) {
        selectedProfile = Profile{};
        loadSelectedProfile(selectedProfile);
    }
    selectProfile(_settings.getSelectedProfile());
    _plugin_manager->trigger("profiles:profile:save", "id", profile.id);
    if (isNew) {
        _settings.addFavoritedProfile(profile.id);
    }
    return true; // Return true if at least local save succeeded
}




void ProfileManager::selectProfile(const String &uuid) {
    ESP_LOGI("ProfileManager", "Selecting profile %s", uuid.c_str());
    _settings.setSelectedProfile(uuid);
    selectedProfile = Profile{};
    loadSelectedProfile(selectedProfile);
    _plugin_manager->trigger("profiles:profile:select", "id", uuid);
}

Profile ProfileManager::getSelectedProfile() const { return selectedProfile; }

void ProfileManager::loadSelectedProfile(Profile &outProfile) { loadProfile(_settings.getSelectedProfile(), outProfile); }

std::vector<String> ProfileManager::getFavoritedProfiles(bool validate) {

    auto rawFavorites = _settings.getFavoritedProfiles();
    std::vector<String> result;

    auto storedProfileOrder = _settings.getProfileOrder();
    for (const auto &id : storedProfileOrder) {
        if (std::find(rawFavorites.begin(), rawFavorites.end(), id) != rawFavorites.end()) {
            if (!validate || profileExists(id)) {
                if (std::find(result.begin(), result.end(), id) == result.end()) {
                    result.push_back(id);
                }
            }
        }
    }

    for (const auto &fav : rawFavorites) {
        if (std::find(result.begin(), result.end(), fav) == result.end()) {
            if (!validate || profileExists(fav)) {
                result.push_back(fav);
            }
        }
    }

    if (result.empty()) {
        String sel = _settings.getSelectedProfile();
        bool selValid = (!validate) ||  profileExists(sel);
        if (selValid) {
            result.push_back(sel);
        }
    }
    return result;
}


// WebDAV helper methods
String ProfileManager::baseUrl() const {
    return _settings.getStoreServer();
}

String ProfileManager::httpGetString(const String& path) const {
    return WebDAVUtils::httpGetString(baseUrl(), path);
}

bool ProfileManager::httpPostJson(const String& path, const String& json) const {
    return WebDAVUtils::httpPostJson(baseUrl(), path, json);
}

bool ProfileManager::httpDelete(const String& path) const {
    return WebDAVUtils::httpDelete(baseUrl(), path);
}

bool ProfileManager::uploadProfileToWebDAV(const String& uuid) {
    File f = _fs.open(profilePath(uuid), FILE_READ);
    if (!f) return false;
    
    bool success = WebDAVUtils::httpUploadFile(baseUrl(), "/profiles/" + uuid, f);
    f.close();
    return success;
}

std::vector<String> ProfileManager::listRemoteProfiles() {
    std::vector<String> uuids;
    String remoteList = httpGetString("/profiles");
    
    if (remoteList.length() > 0) {
        JsonDocument remoteDoc;
        if (deserializeJson(remoteDoc, remoteList) == DeserializationError::Ok && remoteDoc.containsKey("profiles")) {
            JsonArray remoteArr = remoteDoc["profiles"];
            for (JsonVariant item : remoteArr) {
                if (item.containsKey("id")) {
                    uuids.push_back(item["id"].as<String>());
                }
            }
        }
    }
    return uuids;
}

bool ProfileManager::loadRemoteProfile(const String& uuid, Profile& outProfile) {
    String remoteData = httpGetString("/profiles/" + uuid);
    if (remoteData.length() == 0) return false;
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, remoteData);
    if (err) return false;
    
    return parseProfile(doc.as<JsonObject>(), outProfile);
}

bool ProfileManager::saveProfileToWebDAV(const Profile& profile) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    writeProfile(obj, profile);
    
    String json;
    serializeJson(doc, json);
    
    return httpPostJson("/profiles/" + profile.id, json);
}


// WebDAV helper methods
String ProfileManager::baseUrl() const {
    return _settings.getStoreServer();
}

String ProfileManager::httpGetString(const String& path) const {
    return WebDAVUtils::httpGetString(baseUrl(), path);
}

bool ProfileManager::httpPostJson(const String& path, const String& json) const {
    return WebDAVUtils::httpPostJson(baseUrl(), path, json);
}

bool ProfileManager::httpDelete(const String& path) const {
    return WebDAVUtils::httpDelete(baseUrl(), path);
}

bool ProfileManager::uploadProfileToWebDAV(const String& uuid) {
    File f = _fs.open(profilePath(uuid), FILE_READ);
    if (!f) return false;
    
    bool success = WebDAVUtils::httpUploadFile(baseUrl(), "/profiles/" + uuid, f);
    f.close();
    return success;
}

std::vector<String> ProfileManager::listRemoteProfiles() {
    std::vector<String> uuids;
    String baseUrlStr = baseUrl();
    
    ESP_LOGI("ProfileManager", "Listing remote profiles from: %s", baseUrlStr.c_str());
    
    if (baseUrlStr.isEmpty()) {
        ESP_LOGW("ProfileManager", "WebDAV base URL is empty - check settings");
        return uuids;
    }
    
    // FIX: Server expects /profiles/list
    String remoteList = httpGetString("/profiles/list");
    ESP_LOGI("ProfileManager", "Remote profiles response length: %d", remoteList.length());
    
    if (remoteList.length() > 0) {
        ESP_LOGI("ProfileManager", "Remote profiles response: %s", remoteList.substring(0, 200).c_str());
        JsonDocument remoteDoc;
        DeserializationError err = deserializeJson(remoteDoc, remoteList);
        // FIX: Server returns "files" not "profiles"
        if (err == DeserializationError::Ok && remoteDoc.containsKey("files")) {
            JsonArray remoteArr = remoteDoc["files"];
            ESP_LOGI("ProfileManager", "Found %d remote profiles", remoteArr.size());
            for (JsonVariant item : remoteArr) {
                // FIX: Files are filenames like "profile.json", extract ID
                String filename = item.as<String>();
                if (filename.endsWith(".json")) {
                    String uuid = filename.substring(0, filename.length() - 5); // Remove .json
                    ESP_LOGI("ProfileManager", "Remote profile ID: %s", uuid.c_str());
                    uuids.push_back(uuid);
                }
            }
        } else {
            ESP_LOGW("ProfileManager", "Failed to parse remote profiles JSON: %s", err.c_str());
            ESP_LOGW("ProfileManager", "Raw response: %s", remoteList.c_str());
        }
    } else {
        ESP_LOGW("ProfileManager", "Empty response from WebDAV /profiles/list endpoint");
    }
    
    ESP_LOGI("ProfileManager", "Total remote profiles found: %d", uuids.size());
    return uuids;
}

bool ProfileManager::loadRemoteProfile(const String& uuid, Profile& outProfile) {
    String baseUrlStr = baseUrl();
    ESP_LOGI("ProfileManager", "Loading remote profile %s from %s", uuid.c_str(), baseUrlStr.c_str());
    
    if (baseUrlStr.isEmpty()) {
        ESP_LOGW("ProfileManager", "WebDAV base URL is empty");
        return false;
    }
    
    // FIX: Server expects /profiles/get/ID
    String remoteData = httpGetString("/profiles/get/" + uuid);
    ESP_LOGI("ProfileManager", "Remote profile data length: %d", remoteData.length());
    
    if (remoteData.length() == 0) {
        ESP_LOGW("ProfileManager", "No data received for profile %s", uuid.c_str());
        return false;
    }
    
    ESP_LOGI("ProfileManager", "Remote profile data preview: %s", remoteData.substring(0, 100).c_str());
    
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, remoteData);
    if (err) {
        ESP_LOGW("ProfileManager", "Failed to parse remote profile JSON: %s", err.c_str());
        return false;
    }
    
    bool success = parseProfile(doc.as<JsonObject>(), outProfile);
    ESP_LOGI("ProfileManager", "Remote profile parse result: %s", success ? "SUCCESS" : "FAILED");
    return success;
}

bool ProfileManager::saveProfileToWebDAV(const Profile& profile) {
    JsonDocument doc;
    JsonObject obj = doc.to<JsonObject>();
    writeProfile(obj, profile);
    
    String json;
    serializeJson(doc, json);
    
    // FIX: Server expects /profiles/put/ID
    return httpPostJson("/profiles/put/" + profile.id, json);
}

bool ProfileManager::deleteProfile(const String &uuid) {
    _settings.removeFavoritedProfile(uuid);
    
    // FIX: Server expects /profiles/delete/ID
    bool remoteDeleted = httpDelete("/profiles/delete/" + uuid);
    
    // Always try to delete locally
    bool localDeleted = _fs.remove(profilePath(uuid));
    
    if (remoteDeleted && localDeleted) {
        ESP_LOGI("ProfileManager", "Profile %s deleted from both local and remote", uuid.c_str());
    } else if (localDeleted) {
        ESP_LOGW("ProfileManager", "Profile %s deleted locally only", uuid.c_str());
    } else {
        ESP_LOGW("ProfileManager", "Failed to delete profile %s locally", uuid.c_str());
    }
    
    return localDeleted;
}

bool ProfileManager::profileExists(const String &uuid) {
    // Check remote first - server expects /profiles/get/ID
    String remoteData = httpGetString("/profiles/get/" + uuid);
    if (remoteData.length() > 0) {
        return true;
    }
    
    // Fallback to local
    return _fs.exists(profilePath(uuid));
}
