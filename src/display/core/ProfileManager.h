#pragma once
#ifndef PROFILEMANAGER_H
#define PROFILEMANAGER_H
#include "PluginManager.h"
#include <FS.h>
#include <display/core/Settings.h>
#include <display/core/utils.h>
#include <display/models/profile.h>

class ProfileManager {
  public:
    ProfileManager(fs::FS &fs, String dir, Settings &settings, PluginManager *plugin_manager);

    void setup();
    std::vector<String> listProfiles();
    bool loadProfile(const String &uuid, Profile &outProfile);
    bool saveProfile(Profile &profile);
    bool deleteProfile(const String &uuid);
    bool profileExists(const String &uuid);
    void selectProfile(const String &uuid);
    Profile getSelectedProfile() const;
    void loadSelectedProfile(Profile &outProfile);
    std::vector<String> getFavoritedProfiles(bool validate = false);

  private:
    Profile selectedProfile{};
    PluginManager *_plugin_manager;
    Settings &_settings;
    fs::FS &_fs;
    String _dir;
    bool ensureDirectory() const;
    String profilePath(const String &uuid) const;
    void migrate();

    // WebDAV helper methods
    String baseUrl() const;
    String httpGetString(const String& path) const;
    bool httpPostJson(const String& path, const String& json) const;
    bool httpDelete(const String& path) const;
    bool uploadProfileToWebDAV(const String& uuid);
    
    // Enhanced methods with WebDAV support
    std::vector<String> listRemoteProfiles();
    bool loadRemoteProfile(const String& uuid, Profile& outProfile);
    bool saveProfileToWebDAV(const Profile& profile);    
};

#endif // PROFILEMANAGER_H
