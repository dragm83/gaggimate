#include "GitHubOTA.h"
#include "EspHttpTransport.h"
#include "ResumableDownloader.h"
#include "common.h"
#include "semver_extensions.h"
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <sdkconfig.h>

// Flash erases park core 0 and slow every other task, so IDLE0 and the AsyncTCP service task (both watched by
// the 5 s task watchdog) can miss their deadline mid-update (seen twice in field coredumps). Widen the timeout for
// the update instead of unsubscribing anyone: a real hang still reboots, just later.
struct OtaWatchdogRelax {
    static constexpr uint32_t UPDATE_TIMEOUT_S = 60;
    OtaWatchdogRelax() { esp_task_wdt_init(UPDATE_TIMEOUT_S, true); }
    ~OtaWatchdogRelax() { esp_task_wdt_init(CONFIG_ESP_TASK_WDT_TIMEOUT_S, true); }
};

GitHubOTA::GitHubOTA(const String &display_version, const String &controller_version, const String &release_url,
                     const phase_callback_t &phase_callback, const progress_callback_t &progress_callback,
                     const String &firmware_name, const String &filesystem_name, const String &controller_firmware_name) {
    _version = from_string(display_version.substring(1).c_str());
    _controller_version = from_string(controller_version.substring(1).c_str());

    _release_url = release_url;
    _firmware_name = firmware_name;
    _filesystem_name = filesystem_name;
    _controller_firmware_name = controller_firmware_name;
    _phase_callback = phase_callback;
    _progress_callback = progress_callback;

    _wifi_client.setCACertBundle(x509_crt_imported_bundle_bin_start);
}

void GitHubOTA::init() {
    _controller_ota.init([this](int progress) { _progress_callback(PHASE_CONTROLLER_FW, progress); });
}

void GitHubOTA::checkForUpdates() {
    const char *TAG = "checkForUpdates";

    // A failed check or channel change must never leave another release eligible for installation.
    _latest_url = "";
    _latest_version_string = "";
    refreshUpdateAvailability();

    String baseUrl = get_updated_base_url_via_redirect(_wifi_client, _release_url);
    String version;
    if (!baseUrl.isEmpty()) {
        auto lastSlash = baseUrl.lastIndexOf('/', baseUrl.length() - 2);
        String tag = baseUrl.substring(lastSlash + 1);
        tag.replace("/", "");
        if (tag.startsWith("v"))
            version = tag.substring(1);
    } else {
        baseUrl = _release_url + "/";
        baseUrl.replace("/tag/", "/download/");
    }
    if (!baseUrl.startsWith("https://")) {
        ESP_LOGW(TAG, "Refusing non-HTTPS release URL");
        return;
    }
    // Rolling beta tags are fixed names; their actual firmware version lives in version.txt.
    if (version.isEmpty()) {
        version = get_updated_version_via_txt_file(_wifi_client, baseUrl);
        version.trim();
        if (version.startsWith("v"))
            version = version.substring(1);
    }
    unsigned major, minor, patch;
    if (sscanf(version.c_str(), "%u.%u.%u", &major, &minor, &patch) != 3) {
        ESP_LOGW(TAG, "No valid HTTPS firmware release found");
        return;
    }
    _latest_url = baseUrl;
    _latest_version_string = version;
    semver_free(&_latest_version);
    _latest_version = from_string(version.c_str());
    refreshUpdateAvailability();
}

void GitHubOTA::refreshUpdateAvailability() {
    _screen_update_required = false;
    _controller_update_required = false;
    if (_latest_version_string.isEmpty())
        return;
    if (_allowDowngrade) {
        // Returning to official firmware always installs a matched display/controller pair.
        const bool differs = semver_compare(_latest_version, _version) != 0 ||
                             semver_compare(_latest_version, _controller_version) != 0;
        _screen_update_required = _controller_update_required = differs;
    } else {
        _screen_update_required = update_required(_latest_version, _version);
        _controller_update_required = update_required(_latest_version, _controller_version);
    }
}

String GitHubOTA::getCurrentVersion() const { return _latest_version_string; }

bool GitHubOTA::isUpdateAvailable(bool controller) const {
    if (controller) {
        return _controller_update_required;
    }
    return _screen_update_required;
}

void GitHubOTA::setPhase(uint8_t newPhase) {
    phase = newPhase;
    _phase_callback(newPhase);
}

void GitHubOTA::update(bool controller, bool display, NimBLEClient *client) {
    const char *TAG = "update";
    OtaWatchdogRelax watchdogRelax;

    bool updateExecuted = false;

    // checkForUpdates() stores a server-provided Location; never let a downgraded redirect reach the downloads.
    if (!_latest_url.startsWith("https://")) {
        ESP_LOGE(TAG, "Refusing non-HTTPS release URL: %s", _latest_url.c_str());
        setPhase(PHASE_FAILED);
        return;
    }

    if (_allowDowngrade)
        controller = display = true;

    if (controller && _controller_update_required) {
        ESP_LOGI(TAG, "Controller update is required, running firmware update.");
        setPhase(PHASE_CONTROLLER_FW);
        if (!_controller_ota.update(client, _latest_url + _controller_firmware_name)) {
            ESP_LOGE(TAG, "Controller update failed");
            setPhase(PHASE_FAILED);
            return;
        }
        ESP_LOGI(TAG, "Controller update successful.");
        updateExecuted = true;
    }

    if (display && _screen_update_required) {
        ESP_LOGI(TAG, "Update is required, running firmware update.");
        setPhase(PHASE_DISPLAY_FW);
        if (!flashDisplayFirmware(_latest_url + _firmware_name)) {
            ESP_LOGE(TAG, "Display update failed");
            setPhase(PHASE_FAILED);
            return;
        }
        // The web UI ships inside the app image (GM-106); LittleFS (profiles + shots) is never touched by OTA.
        ESP_LOGI(TAG, "Update successful. Restarting...\n");
        updateExecuted = true;
    }
    setPhase(PHASE_FINISHED);

    if (updateExecuted) {
        delay(1000);
        ESP.restart();
    }

    ESP_LOGI(TAG, "No updates found\n");
}

void GitHubOTA::setReleaseUrl(const String &release_url, bool allowDowngrade, const String &controller_firmware_name) {
    _release_url = release_url;
    _allowDowngrade = allowDowngrade;
    _controller_firmware_name = controller_firmware_name;
    _latest_url = "";
    _latest_version_string = "";
    refreshUpdateAvailability();
}

bool GitHubOTA::flashDisplayFirmware(const String &url) {
    const char *TAG = "flashDisplayFirmware";
    ESP_LOGI(TAG, "Download URL: %s", url.c_str());

    const esp_partition_t *partition = esp_ota_get_next_update_partition(nullptr);
    if (partition == nullptr) {
        ESP_LOGE(TAG, "No OTA partition available");
        return false;
    }
    esp_ota_handle_t handle = 0;
    auto abort = [&]() {
        if (handle != 0) {
            esp_ota_abort(handle);
            handle = 0;
        }
    };

    DownloadSink sink;
    // Erasing the whole image range once up front (cooperatively, in yielding slices) beats one sector erase per
    // 4 KB write: far fewer core-0 stalls during the download. Unknown size falls back to erase-as-you-go.
    sink.prepare = [&](size_t total) {
        abort();
        esp_err_t err = esp_ota_begin(partition, total > 0 ? total : OTA_WITH_SEQUENTIAL_WRITES, &handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_begin(%u) failed: %s", static_cast<unsigned>(total), esp_err_to_name(err));
            handle = 0;
        }
        return err == ESP_OK;
    };
    sink.write = [&](const uint8_t *data, size_t len) {
        if (handle == 0) {
            return false;
        }
        esp_err_t err = esp_ota_write(handle, data, len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_write failed: %s", esp_err_to_name(err));
        }
        return err == ESP_OK;
    };
    sink.restart = [&]() {
        abort(); // the next full response prepares a fresh handle
        return true;
    };
    EspHttpTransport transport;
    EspDownloadEnv env;
    ResumableDownloader downloader(transport, env, url.c_str(), sink, [this](size_t received, size_t total) {
        if (total > 0 || received == 0) {
            _progress_callback(phase, total > 0 ? static_cast<int>((static_cast<uint64_t>(received) * 100) / total) : 0);
        }
    });
    if (!downloader.run() || downloader.received() == 0 || handle == 0) {
        abort();
        return false;
    }

    // esp_ota_end verifies the complete image (header, checksum, appended SHA-256) before it can be booted.
    esp_err_t err = esp_ota_end(handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Image verification failed: %s", esp_err_to_name(err));
        return false;
    }
    err = esp_ota_set_boot_partition(partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "Wrote %u bytes to %s", static_cast<unsigned>(downloader.received()), partition->label);
    return true;
}

void GitHubOTA::setControllerVersion(const String &controller_version) {
    semver_free(&_controller_version);
    _controller_version = from_string(controller_version.substring(1).c_str());
    refreshUpdateAvailability();
}
