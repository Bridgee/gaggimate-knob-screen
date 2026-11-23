#include "WebUIPlugin.h"
#include "OtaPolicy.h"
#include <cstring>
#include <DNSServer.h>
#include <LittleFS.h>
#include <SD_MMC.h>
#include <algorithm>
#include <display/core/Controller.h>
#include <display/core/MachineProfileStore.h>
#include <display/core/MachineProfiles.h>
#include <display/core/ProfileImage.h>
#include <display/core/ProfileManager.h>
#include <display/models/profile.h>
#include <display/plugins/BLEScalePlugin.h>
#include <display/plugins/ShotHistoryPlugin.h>
#include <display/webassets/web_ui_manifest.h>
#include <esp32-hal-psram.h>
#include <esp_core_dump.h>
#include <esp_err.h>
#include <esp_heap_caps.h>
#include <esp_partition.h>
#include <mbedtls/platform.h>
#include <vector>
#include <version.h>
#ifdef GAGGIMATE_KNOB
#include <display/drivers/GuitionKnob/Hardware.h>
#include <Protocol.h>
#include "ScreenOtaPolicy.h"
#include "NetworkWorker.h"
#include "BufferedProfileImage.h"
#include <display/drivers/GuitionKnob/BootHealth.h>
#include <esp_ota_ops.h>
#endif

static WebUIPlugin *g_webUIPlugin = nullptr;

// Route mbedTLS allocations to PSRAM.
static void *mbedtlsPsramCalloc(size_t n, size_t size) { // NOSONAR
    void *p = heap_caps_calloc(n, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == nullptr) {
        p = heap_caps_calloc(n, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    }
    return p;
}
static void mbedtlsPsramFree(void *p) { heap_caps_free(p); } // NOSONAR

WebUIPlugin::WebUIPlugin()
#ifdef GAGGIMATE_KNOB
    : matchedReleases(std::strcmp(BUILD_RELEASE_CHANNEL, "candidate") == 0),
      screenInstaller(BUILD_SCREEN_OTA_QUALIFICATION != 0), server(80)
#else
    : server(80)
#endif
{ g_webUIPlugin = this; }

bool WebUIPlugin::anyUpdateAvailable() const {
#ifdef GAGGIMATE_KNOB
    const auto pair = matchedReleases.snapshot();
    if (pair.status == "ready" &&
        (matched_release::order(pair.pair.screenVersion, BUILD_PORT_VERSION) > 0 ||
         matched_release::order(pair.pair.controllerVersion, controller->getSystemInfo().version.c_str()) > 0)) return true;
    const auto official = officialReleases.snapshot();
    return official.displayUpdate || official.controllerUpdate;
#else
    return ota->isUpdateAvailable() || ota->isUpdateAvailable(true);
#endif
}

void WebUIPlugin::setup(Controller *_controller, PluginManager *_pluginManager) {
    // Redirect mbedTLS allocations to PSRAM before any TLS (OTA) handshake runs, so the
    // ~32 KB handshake buffers don't exhaust the scarce internal-DRAM pool. See mbedtlsPsramCalloc.
    (void)mbedtls_platform_set_calloc_free(mbedtlsPsramCalloc, mbedtlsPsramFree);
#ifdef GAGGIMATE_KNOB
    // Reserve the sole HTTPS task before Wi-Fi/BLE and UI heap fragmentation.
    network_work::begin();
#endif
    this->controller = _controller;
    this->profileManager = _controller->getProfileManager();
    this->pluginManager = _pluginManager;
    this->ota = new GitHubOTA(
        FirmwareOtaPolicy::displayComparisonVersion, controller->getSystemInfo().version,
        RELEASE_URL + (controller->getSettings().getOTAChannel() == "latest" ? "latest" : "tag/nightly"),
        [this](uint8_t phase) {
            pluginManager->trigger("ota:update:phase", "phase", phase);
            updateOTAProgress(phase, 0);
        },
        [this](uint8_t phase, int progress) {
            pluginManager->trigger("ota:update:progress", "progress", progress);
            updateOTAProgress(phase, progress);
        },
        OTA_DISPLAY_FIRMWARE, OTA_DISPLAY_FILESYSTEM, "board-firmware.bin");
    pluginManager->on("controller:wifi:connect", [this](Event const &event) {
        apMode = event.getInt("AP");
        start();
    });
    // Intentionally do NOT stop the server on a WiFi disconnect: the listen
    // socket survives a reconnect, and tearing it down only to rebind moments
    // later races AsyncTCP's async close (bind: -8) and churns sockets in the
    // recovery path. The server keeps listening; clients reconnect on their own.
    pluginManager->on("controller:wifi:disconnect", [this](Event const &) {
        wsHandler.cleanupClients(); // drop dead websocket clients; keep the listener up
    });
    pluginManager->on("controller:ready", [this](Event const &) {
        ota->setControllerVersion(controller->getSystemInfo().version);
        ota->init();
#ifdef GAGGIMATE_KNOB
        officialRefreshRequested = true;
#endif
    });
    wsHandler.setup(controller, pluginManager);
    wsHandler.onOtaSettings([this](JsonDocument &request) { handleOTASettings(request); });
    wsHandler.onOtaStart([this](JsonDocument &request) { handleOTAStart(request); });
    wsHandler.setUpdateAvailableProvider([this] { return anyUpdateAvailable(); });

#ifdef GAGGIMATE_KNOB
    wsHandler.onScreenOtaStart([this](JsonDocument &request) { handleScreenOTAStart(request); });
#endif
    setupServer();
}

void WebUIPlugin::loop() {
#ifdef GAGGIMATE_KNOB
    screen_ota::InstallJob screenJob;
    if (screenInstaller.takeQueued(screenJob)) {
        const bool healthy = !guition_knob::bootHealthPending() &&
            (std::strcmp(guition_knob::bootHealthState(), "healthy") == 0 ||
             std::strcmp(guition_knob::bootHealthState(), "confirmed") == 0);
        if (!healthy || !controller->tryBeginScreenUpdate()) screenInstaller.fail("standby-or-readiness-changed");
        else {
            screenUpdateActive = true;
            pluginManager->trigger("ota:update:start", "component", "display");
            screenInstaller.start(screenJob, [this, protocol = screenJob.plan.protocol, version = screenJob.plan.controllerVersion] {
                return controller->canCommitScreenUpdate(protocol, version.c_str());
            });
        }
    }
    if (screenUpdateActive && !screenInstaller.snapshot().busy) {
        pluginManager->trigger("ota:update:end"); screenUpdateActive = false;
    }
    if (screenInstaller.takeChanged()) updateOTAStatus(ota->getCurrentVersion());
#endif
    if (updating) {
        // Pass which component is being flashed: a controller update streams the
        // firmware over the controller link (BLE wants low latency), a display update
        // is over Wi-Fi (wants BLE to stay out of the radio's way). "" = both.
        pluginManager->trigger("ota:update:start", "component", updateComponent);
        ota->update(updateComponent != "display", updateComponent != "controller",
                    controller->getClientController()->getOtaSender());
        pluginManager->trigger("ota:update:end");
        updating = false;
    }
    if (!serverRunning) {
        return;
    }
    const unsigned long now = millis();
    // HTTPS must not stall controller initialization, telemetry or UI state.
    // Only scheduling happens here on K718; both release sources use workers.
    bool mayCheck = !controller->isActive();
#ifdef GAGGIMATE_KNOB
    mayCheck = mayCheck && !controller->isUpdating();
#endif
    if (mayCheck && (lastUpdateCheck == 0 || now - lastUpdateCheck > UPDATE_CHECK_INTERVAL)) {
#ifdef GAGGIMATE_KNOB
        officialRefreshRequested = true;
        matchedReleases.request();
#else
        ota->checkForUpdates();
        pluginManager->trigger("ota:update:status", "value", ota->isUpdateAvailable() || ota->isUpdateAvailable(true));
#endif
        lastUpdateCheck = now;
        updateOTAStatus(ota->getCurrentVersion());
    }
#ifdef GAGGIMATE_KNOB
    if (!controller->isUpdating() && !controller->isActive() && officialRefreshRequested.exchange(false))
        officialReleases.request(controller->getSettings().getOTAChannel().c_str(), controller->getSystemInfo().version.c_str());
    if (!controller->isUpdating() && !controller->isActive() && pairRefreshRequested.exchange(false)) matchedReleases.request();
    const bool pairChanged = matchedReleases.takeChanged();
    const bool officialChanged = officialReleases.takeChanged();
    if (pairChanged || officialChanged) {
        pluginManager->trigger("ota:update:status", "value", anyUpdateAvailable());
        updateOTAStatus(ota->getCurrentVersion());
    }
#endif
    wsHandler.loop(now);
    if (now > lastDns + DNS_PERIOD && dnsServer != nullptr) {
        lastDns = now;
        dnsServer->processNextRequest();
    }
}

// Linear lookup over the embedded asset table (~60 entries) — a couple of
// strcmps per request, negligible next to the network round-trip.
static const WebAsset *findWebAsset(const String &path) {
    for (size_t i = 0; i < WEB_ASSETS_COUNT; i++) {
        if (path == WEB_ASSETS[i].path) {
            return &WEB_ASSETS[i];
        }
    }
    return nullptr;
}

void WebUIPlugin::serveWebAsset(AsyncWebServerRequest *request) {
    String path = request->url();
    if (path.isEmpty() || path == "/") {
        path = WEB_UI_INDEX_PATH;
    }

    const WebAsset *asset = findWebAsset(path);
    if (asset == nullptr && !path.startsWith("/assets/")) {
        // SPA client-side routes (e.g. /settings, /profiles) aren't real files —
        // fall back to index.html. A miss under /assets/ is a genuine 404, not a
        // route, so it is not rewritten.
        asset = findWebAsset(WEB_UI_INDEX_PATH);
    }
    if (asset == nullptr) {
        request->send(404, "text/plain", "Not found");
        return;
    }

    // Serve straight from the memory-mapped flash blob — no copy into RAM, no
    // filesystem read. AsyncProgmemResponse streams from the pointer in chunks.
    AsyncWebServerResponse *response =
        request->beginResponse(200, asset->contentType, gWebUiBlobStart + asset->offset, asset->length);
    if (asset->gzip) {
        response->addHeader("Content-Encoding", "gzip");
    }
    // Content-hashed build assets (/assets/<hash>.js) never change for a given URL — cache them forever. index.html and
    // other unhashed files must revalidate so a new build is picked up after an update. [GM-83]
    if (path.startsWith("/assets/")) {
        response->addHeader("Cache-Control", "public, max-age=31536000, immutable");
    } else {
        response->addHeader("Cache-Control", "no-cache");
    }
    request->send(response);
}

void WebUIPlugin::setupServer() {
#ifdef GAGGIMATE_KNOB
    guition_knob::Hardware::instance().bindWeb(server);
#endif
    server.on("/connecttest.txt", [](AsyncWebServerRequest *request) {
        request->redirect("http://logout.net");
    }); // windows 11 captive portal workaround
    server.on("/wpad.dat", [](AsyncWebServerRequest *request) {
        request->send(404);
    }); // Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32
        // :)
    server.on("/generate_204",
              [](AsyncWebServerRequest *request) { request->redirect(LOCAL_URL); }); // android captive portal redirect
    server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(LOCAL_URL); });            // microsoft redirect
    server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(LOCAL_URL); }); // apple call home
    server.on("/canonical.html",
              [](AsyncWebServerRequest *request) { request->redirect(LOCAL_URL); });       // firefox captive portal call home
    server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); }); // firefox captive portal call home
    server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(LOCAL_URL); }); // windows call home
    server.on("/api/settings", [this](AsyncWebServerRequest *request) { handleSettings(request); });
    server.on("/api/status", [this](AsyncWebServerRequest *request) {
        AsyncResponseStream *response = request->beginResponseStream("application/json");
        JsonDocument doc(&psramAllocator);
        doc["mode"] = controller->getMode();
        doc["tt"] = controller->getTargetTemp();
        doc["ct"] = controller->getCurrentTemp();
        serializeJson(doc, *response);
        request->send(response);
    });
    server.on("/api/scales/list", [this](AsyncWebServerRequest *request) { handleBLEScaleList(request); });
    server.on("/api/scales/connect", [this](AsyncWebServerRequest *request) { handleBLEScaleConnect(request); });
    server.on("/api/scales/scan", [this](AsyncWebServerRequest *request) { handleBLEScaleScan(request); });
    server.on("/api/scales/info", [this](AsyncWebServerRequest *request) { handleBLEScaleInfo(request); });
    server.on("/api/scales/forget", [this](AsyncWebServerRequest *request) { handleBLEScaleForget(request); });
    FS *fs = &LittleFS;
    if (controller->isSDCard()) {
        fs = &SD_MMC;
    }
    machineProfiles = new MachineProfileStore(*fs);
    server.on("/api/machine-profiles", [this](AsyncWebServerRequest *request) { handleMachineProfiles(request); });
    server
        .on(AsyncURIMatcher::prefix("/api/history/"), HTTP_ANY,
            [](AsyncWebServerRequest *request) { request->send(503, "text/plain", "Update in progress"); })
        .setFilter([this](AsyncWebServerRequest *) { return controller->isUpdating(); });
    server.serveStatic("/api/history/", *fs, "/h/").setCacheControl("no-store");
    server.on("/api/history/index.bin", HTTP_GET, [this, fs](AsyncWebServerRequest *request) {
        // Serve the binary index file directly
        if (fs->exists("/h/index.bin")) {
            request->send(*fs, "/h/index.bin", "application/octet-stream");
        } else {
            request->send(404, "text/plain", "Index not found");
        }
    });
    server.on("/api/history/recent.bin", HTTP_GET, [this](AsyncWebServerRequest *request) {
        // The most recent non-deleted shots, newest first, as a regular shot
        // index (SIDX header + entries) — same binary format as index.bin,
        // just truncated, so clients reuse the index.bin parser.
        constexpr long MAX_RECENT_LIMIT = 50;
        long limit = 8;
        if (request->hasArg("limit")) {
            limit = constrain(request->arg("limit").toInt(), 1L, MAX_RECENT_LIMIT);
        }

        auto *entries = static_cast<ShotIndexEntry *>(ps_malloc(limit * sizeof(ShotIndexEntry)));
        if (entries == nullptr) {
            request->send(500, "text/plain", "Out of memory");
            return;
        }
        size_t count = ShotHistory.readRecentEntries(entries, limit);

        ShotIndexHeader header{};
        header.magic = SHOT_INDEX_MAGIC;
        header.version = SHOT_INDEX_VERSION;
        header.entrySize = SHOT_INDEX_ENTRY_SIZE;
        header.entryCount = count;
        header.nextId = 0; // meaningless for a partial view

        AsyncResponseStream *response = request->beginResponseStream("application/octet-stream");
        response->addHeader("Cache-Control", "no-store");
        response->write(reinterpret_cast<const uint8_t *>(&header), sizeof(header));
        response->write(reinterpret_cast<const uint8_t *>(entries), count * sizeof(ShotIndexEntry));
        free(entries);
        request->send(response);
    });
    server.on("/api/core-dump", HTTP_GET, [this](AsyncWebServerRequest *request) { handleCoreDumpDownload(request); });
    server.on(
        "/api/profiles/image", HTTP_ANY, [this](AsyncWebServerRequest *request) { handleProfileImage(request); }, nullptr,
        [this](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
            handleProfileImageBody(request, data, len, index, total);
        });
    // The web UI is embedded in firmware flash and served from the memory-mapped blob (see serveWebAsset). It is no
    // longer in LittleFS, so OTA never touches the partition holding profiles/shots. The catch-all onNotFound handles
    // every path not claimed by an explicit server.on()/api route above. [GM-106]
    server.onNotFound([this](AsyncWebServerRequest *request) { serveWebAsset(request); });
    wsHandler.attach(server);
}

void WebUIPlugin::start() {
    if (serverRunning) {
        // Already listening. The 0.0.0.0:80 listen socket survives a WiFi
        // reconnect, so re-running end()+begin() only races AsyncTCP's async
        // socket close and fails to rebind ("bind: -8, port in use"). A transient
        // STA reconnect needs nothing done here.
        return;
    }
    if (!pluginHooksRegistered) {
        // Fired here, not in setup(), so plugins set up after this one still get to register before the server listens
        Event serverEvent;
        serverEvent.id = "webui:server:register";
        serverEvent.setPointer("server", &server);
        pluginManager->trigger(serverEvent);
        Event wsEvent;
        wsEvent.id = "webui:ws:register";
        wsEvent.setPointer("ws", &wsHandler);
        pluginManager->trigger(wsEvent);
        pluginHooksRegistered = true;
    }
    server.begin();
    ESP_LOGI("WebUIPlugin", "Started webserver");
    if (apMode) {
        dnsServer = std::make_unique<DNSServer>();
        dnsServer->setTTL(3600);
        dnsServer->start(53, "*", WIFI_AP_IP);
        ESP_LOGI("WebUIPlugin", "Started catchall DNS for captive portal");
    }
    lastUpdateCheck = millis();
    serverRunning = true;
}

void WebUIPlugin::stop() {
    if (!serverRunning)
        return;
    wsHandler.closeAll();
    server.end();
    if (dnsServer != nullptr) {
        dnsServer->stop();
        dnsServer.reset();
    }
    serverRunning = false;
    ESP_LOGI("WebUIPlugin", "WebUIPlugin stopped (wifi disconnected)");
}

void WebUIPlugin::handleOTASettings(JsonDocument &request) {
#ifdef GAGGIMATE_KNOB
    if (controller->isUpdating()) { updateOTAStatus(ota->getCurrentVersion()); return; }
#endif
#ifdef GAGGIMATE_KNOB
    if (request["refreshPair"].as<bool>()) pairRefreshRequested = true;
#endif
    if (request["update"].as<bool>()) {
        if (!request["channel"].isNull()) {
            controller->getSettings().setOTAChannel(request["channel"].as<String>() == "latest" ? "latest" : "nightly");
#ifdef GAGGIMATE_KNOB
            officialRefreshRequested = true;
#else
            ota->setReleaseUrl(RELEASE_URL + (controller->getSettings().getOTAChannel() == "latest" ? "latest" : "tag/nightly"));
            lastUpdateCheck = 0;
#endif
        }
    }
    updateOTAStatus("Checking...");
}

void WebUIPlugin::handleOTAStart(JsonDocument &request) {
    if (!FirmwareOtaPolicy::installationAllowed) {
        updateOTAStatus(ota->getCurrentVersion());
        return;
    }
    updating = true;
    if (request["cp"].is<String>()) {
        updateComponent = request["cp"].as<String>();
    } else {
        updateComponent = "";
    }
}

// Booleans are only touched when present, so a partial POST keeps the rest (GM-214).
static void writeMachineHardware(JsonObject object, const gm::MachineHardwareSettings &value) {
    object["ntcBrewNominalResistance"] = value.ntc_brew_nominal_resistance;
    object["ntcBrewSeriesResistance"] = value.ntc_brew_series_resistance;
    object["ntcBrewSupplyVoltage"] = value.ntc_brew_supply_voltage;
    object["ntcBrewBeta"] = value.ntc_brew_beta;
    object["ntcSteamNominalResistance"] = value.ntc_steam_nominal_resistance;
    object["ntcSteamSeriesResistance"] = value.ntc_steam_series_resistance;
    object["ntcSteamSupplyVoltage"] = value.ntc_steam_supply_voltage;
    object["ntcSteamBeta"] = value.ntc_steam_beta;
    object["tankLevelEnabled"] = value.tank_level_enabled;
    object["steamLevelEnabled"] = value.steam_level_enabled;
    object["steamLevelDebounceChecks"] = value.steam_level_debounce_checks;
}

static bool parseBoolArg(const String &value) {
    return !(value.isEmpty() || value == "0" || value.equalsIgnoreCase("false") || value.equalsIgnoreCase("off"));
}

void WebUIPlugin::handleMachineProfiles(AsyncWebServerRequest *request) {
    if (controller->isUpdating()) {
        request->send(503, "application/json", "{\"error\":\"Update in progress\"}");
        return;
    }
    JsonDocument doc(&psramAllocator);
    String error;
    bool ok = false;
    int status = 400;
    if (request->method() == HTTP_GET) {
        if (request->hasArg("id")) {
            ok = loadBuiltinMachineProfile(request->arg("id"), doc) || machineProfiles->load(request->arg("id"), doc, error);
            status = 404;
        } else {
            ok = machineProfiles->list(doc.to<JsonArray>(), error);
            if (ok)
                addBuiltinMachineProfiles(doc.as<JsonArray>());
            status = 500;
        }
    } else if (request->method() == HTTP_POST && request->hasArg("profile")) {
        const String raw = request->arg("profile");
        if (raw.length() > 3500 || deserializeJson(doc, raw))
            error = "Invalid machine profile JSON";
        else
            ok = machineProfiles->save(doc.as<JsonVariantConst>(), error, request->arg("replace") == "1");
    } else if (request->method() == HTTP_DELETE && request->hasArg("id")) {
        JsonDocument active(&psramAllocator);
        const String selection = controller->getSettings().getMachineProfileSelection();
        if (selection.length() && deserializeJson(active, selection))
            error = "Could not verify active machine profile";
        else if (active["id"].as<String>() == request->arg("id"))
            error = "Cannot delete the active machine profile";
        else
            ok = machineProfiles->remove(request->arg("id"), error);
        doc["ok"] = ok;
    } else
        error = "Invalid machine profile request";
    if (!ok) {
        doc.clear();
        doc["error"] = error;
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    response->setCode(ok ? 200 : status);
    response->addHeader("Cache-Control", "no-store");
    serializeJson(doc, *response);
    request->send(response);
}

void WebUIPlugin::handleSettings(AsyncWebServerRequest *request) const {
#ifdef GAGGIMATE_KNOB
    if (controller->isUpdating() && request->method() != HTTP_GET) {
        request->send(503, "application/json", "{\"error\":\"Screen update in progress\"}"); return;
    }
#endif
    const uint32_t previousHardwareHash = machineHardwareHash(controller->getSettings().getMachineHardware());
    // Dual-boiler fields are only exposed when the POST validator will accept them back.
    const bool hardwareSupported =
        controller->getSystemInfo().capabilities.dualBoiler && !controller->getSystemInfo().protocolMismatch;
    if (request->method() == HTTP_POST) {
#ifdef GAGGIMATE_KNOB
        const auto validLedArg = [request](const char *name, bool (*validate)(const char *)) {
            if (!request->hasArg(name)) return true;
            const String value = request->arg(name);
            return value.length() == std::strlen(value.c_str()) && validate(value.c_str());
        };
        if (!validLedArg("knobLedEnabled", guition_knob::validLedEnabled) ||
            !validLedArg("knobLedBrightness", guition_knob::validLedPercent)) {
            request->send(400, "text/plain", "LED enabled must be 0/1; brightness must be an integer from 0 to 100.");
            return;
        }
#endif
        if ((request->hasArg("standbyContent") && (request->arg("standbyContent").length()!=std::strlen(request->arg("standbyContent").c_str()) || !gaggimate_ui::validStandbyContent(request->arg("standbyContent").c_str()))) ||
            (request->hasArg("standbyText") && (request->arg("standbyText").length()!=std::strlen(request->arg("standbyText").c_str()) || !gaggimate_ui::validStandbyText(request->arg("standbyText").c_str())))) {
            request->send(400,"text/plain","Standby content must be logo/text/clock; text allows at most24 printable ASCII characters.");return;
        }
        int appearanceArgs = int(request->hasArg("standbyContent")) + int(request->hasArg("standbyText"));
#ifdef GAGGIMATE_KNOB
        appearanceArgs += int(request->hasArg("knobLedEnabled")) + int(request->hasArg("knobLedBrightness"));
#endif
        const bool appearanceOnly = appearanceArgs > 0 && request->args() == appearanceArgs;
        if (request->hasArg("setupStep")) {
            const String step = request->arg("setupStep");
            if (step != "machine" && step != "tests" && step != "plugins" && step != "wifi" && step != "done" &&
                step != "custom:plugins" && step != "custom:wifi" && step != "custom:done") {
                request->send(400, "application/json", "{\"error\":\"Invalid setup step\"}");
                return;
            }
        }
        String profileError;
        if (!validateMachineProfileRequest(request, controller->getSystemInfo(), hardwareSupported, profileError)) {
            JsonDocument failure;
            failure["error"] = profileError;
            String body;
            serializeJson(failure, body);
            request->send(400, "application/json", body);
            return;
        }
        bool hardwareRequest = false;
        JsonDocument currentHardware;
        writeMachineHardware(currentHardware.to<JsonObject>(), controller->getSettings().getMachineHardware());
        for (auto field : currentHardware.as<JsonObject>()) {
            if (!request->hasArg(field.key().c_str()))
                continue;
            const String raw = request->arg(field.key().c_str());
            hardwareRequest |= field.value().is<bool>() ? parseBoolArg(raw) != field.value().as<bool>()
                                                        : std::fabs(raw.toFloat() - field.value().as<float>()) > 0.000001f;
        }
        const auto &currentSettings = controller->getSettings();
        const bool refillRequest =
            (request->hasArg("steamRefillMode") &&
             request->arg("steamRefillMode").toInt() != currentSettings.getSteamRefillMode()) ||
            (request->hasArg("steamRefillPostClearSeconds") &&
             request->arg("steamRefillPostClearSeconds").toInt() != currentSettings.getSteamRefillPostClearSeconds());
        const bool profileChanged =
            request->hasArg("machineProfileSelection") &&
            request->arg("machineProfileSelection") != controller->getSettings().getMachineProfileSelection();
        if ((profileChanged || hardwareRequest || refillRequest) &&
            (controller->isActive() || controller->isUpdating() || controller->isAutotuning())) {
            request->send(409, "application/json",
                          "{\"error\":\"Stop the current process before applying a machine configuration\"}");
            return;
        }
        if (profileChanged || hardwareRequest || refillRequest)
            controller->activateStandby();
        bool saved = false;
        controller->getSettings().batchUpdate([request, &saved, &profileError](Settings *settings) {
#ifdef GAGGIMATE_KNOB
            if (request->hasArg("knobLedEnabled") || request->hasArg("knobLedBrightness")) {
                auto value = settings->getKnobLedPreferences();
                if (request->hasArg("knobLedEnabled")) value.enabled = request->arg("knobLedEnabled") == "1";
                if (request->hasArg("knobLedBrightness")) value.brightness = request->arg("knobLedBrightness").toInt();
                settings->setKnobLedPreferences(value);
            }
#endif
            if (request->hasArg("machineProfileSelection") &&
                !settings->setMachineProfileSelection(request->arg("machineProfileSelection"))) {
                profileError = "Could not create machine profile reference; previous selection retained";
                return;
            }
            if (request->hasArg("ntcBrewNominalResistance"))
                settings->setNtcBrewNominalResistance(request->arg("ntcBrewNominalResistance").toFloat());
            if (request->hasArg("ntcBrewSeriesResistance"))
                settings->setNtcBrewSeriesResistance(request->arg("ntcBrewSeriesResistance").toFloat());
            if (request->hasArg("ntcBrewSupplyVoltage"))
                settings->setNtcBrewSupplyVoltage(request->arg("ntcBrewSupplyVoltage").toFloat());
            if (request->hasArg("ntcBrewBeta"))
                settings->setNtcBrewBeta(request->arg("ntcBrewBeta").toFloat());
            if (request->hasArg("ntcSteamNominalResistance"))
                settings->setNtcSteamNominalResistance(request->arg("ntcSteamNominalResistance").toFloat());
            if (request->hasArg("ntcSteamSeriesResistance"))
                settings->setNtcSteamSeriesResistance(request->arg("ntcSteamSeriesResistance").toFloat());
            if (request->hasArg("ntcSteamSupplyVoltage"))
                settings->setNtcSteamSupplyVoltage(request->arg("ntcSteamSupplyVoltage").toFloat());
            if (request->hasArg("ntcSteamBeta"))
                settings->setNtcSteamBeta(request->arg("ntcSteamBeta").toFloat());
            if (request->hasArg("tankLevelEnabled"))
                settings->setTankLevelEnabled(request->arg("tankLevelEnabled") == "1");
            if (request->hasArg("steamLevelEnabled"))
                settings->setSteamLevelEnabled(request->arg("steamLevelEnabled") == "1");
            if (request->hasArg("systemLedFunction"))
                settings->setSystemLedFunction(request->arg("systemLedFunction").toInt());
            if (request->hasArg("baristaLedFunction"))
                settings->setBaristaLedFunction(request->arg("baristaLedFunction").toInt());
            if (request->hasArg("systemLedBrightness"))
                settings->setSystemLedBrightness(request->arg("systemLedBrightness").toInt());
            if (request->hasArg("systemLedMinBrightness"))
                settings->setSystemLedMinBrightness(request->arg("systemLedMinBrightness").toInt());
            if (request->hasArg("systemLedCycleTimeMs"))
                settings->setSystemLedCycleTimeMs(request->arg("systemLedCycleTimeMs").toInt());
            if (request->hasArg("baristaLedBrightness"))
                settings->setBaristaLedBrightness(request->arg("baristaLedBrightness").toInt());
            if (request->hasArg("baristaLedMinBrightness"))
                settings->setBaristaLedMinBrightness(request->arg("baristaLedMinBrightness").toInt());
            if (request->hasArg("baristaLedCycleTimeMs"))
                settings->setBaristaLedCycleTimeMs(request->arg("baristaLedCycleTimeMs").toInt());
            if (request->hasArg("steamRefillMode"))
                settings->setSteamRefillMode(request->arg("steamRefillMode").toInt());
            if (request->hasArg("steamRefillPostClearSeconds"))
                settings->setSteamRefillPostClearSeconds(request->arg("steamRefillPostClearSeconds").toInt());
            if (request->hasArg("steamLevelDebounceChecks"))
                settings->setSteamLevelDebounceChecks(request->arg("steamLevelDebounceChecks").toInt());
            if (request->hasArg("defaultSteamMode"))
                settings->setDefaultSteamMode(request->arg("defaultSteamMode").toInt());
            if (request->hasArg("startupMode"))
                settings->setStartupMode(request->arg("startupMode") == "brew" ? MODE_BREW : MODE_STANDBY);
            if (request->hasArg("startupProfile"))
                settings->setStartupProfile(request->arg("startupProfile"));
            if (request->hasArg("targetSteamTemp"))
                settings->setTargetSteamTemp(request->arg("targetSteamTemp").toInt());
            if (request->hasArg("targetWaterTemp"))
                settings->setTargetWaterTemp(request->arg("targetWaterTemp").toInt());
            if (request->hasArg("temperatureOffset"))
                settings->setTemperatureOffset(request->arg("temperatureOffset").toInt());
            if (request->hasArg("pressureOffset"))
                settings->setPressureOffset(request->arg("pressureOffset").toFloat());
            if (request->hasArg("pressureScaling"))
                settings->setPressureScaling(request->arg("pressureScaling").toFloat());
            if (request->hasArg("scaleFactor1") || request->hasArg("scaleFactor2")) {
                float sf1 = settings->getScaleFactor1();
                float sf2 = settings->getScaleFactor2();
                if (request->hasArg("scaleFactor1")) {
                    sf1 = request->arg("scaleFactor1").toFloat();
                }
                if (request->hasArg("scaleFactor2")) {
                    sf2 = request->arg("scaleFactor2").toFloat();
                }
                settings->setScaleFactors(sf1, sf2);
            }
            if (request->hasArg("hardwareScaleSampleRateSps") || request->hasArg("hardwareScaleIdleAlpha") ||
                request->hasArg("hardwareScaleActiveAlpha")) {
                const uint16_t sampleRate = request->hasArg("hardwareScaleSampleRateSps")
                                                ? static_cast<uint16_t>(request->arg("hardwareScaleSampleRateSps").toInt())
                                                : settings->getHardwareScaleSampleRateSps();
                const float idleAlpha = request->hasArg("hardwareScaleIdleAlpha")
                                            ? request->arg("hardwareScaleIdleAlpha").toFloat()
                                            : settings->getHardwareScaleIdleAlpha();
                const float activeAlpha = request->hasArg("hardwareScaleActiveAlpha")
                                              ? request->arg("hardwareScaleActiveAlpha").toFloat()
                                              : settings->getHardwareScaleActiveAlpha();
                settings->setHardwareScaleConfiguration(sampleRate, idleAlpha, activeAlpha);
            }
            if (request->hasArg("preferredScaleSource"))
                settings->setPreferredScaleSource(request->arg("preferredScaleSource"));
            if (request->hasArg("heaterCoordinationEnabled"))
                settings->setHeaterCoordinationEnabled(parseBoolArg(request->arg("heaterCoordinationEnabled")));
            if (request->hasArg("heaterHandoverMs"))
                settings->setHeaterHandoverMs(request->arg("heaterHandoverMs").toInt());
            if (request->hasArg("steamPid"))
                settings->setSteamPid(request->arg("steamPid"));
            if (request->hasArg("pid"))
                settings->setPid(request->arg("pid"));
            if (request->hasArg("pumpModelCoeffs"))
                settings->setPumpModelCoeffs(request->arg("pumpModelCoeffs"));
            if (request->hasArg("pumpSlipCoeffs"))
                settings->setPumpSlipCoeffs(request->arg("pumpSlipCoeffs"));
            if (request->hasArg("wifiSsid"))
                settings->setWifiSsid(request->arg("wifiSsid"));
            if (request->hasArg("mdnsName"))
                settings->setMdnsName(request->arg("mdnsName"));
            if (request->hasArg("wifiPassword") && request->arg("wifiPassword") != "---unchanged---")
                settings->setWifiPassword(request->arg("wifiPassword"));
            if (request->hasArg("apPassword") && request->arg("apPassword").length() >= WIFI_AP_PASSWORD_MIN_LENGTH)
                settings->setWifiApPassword(request->arg("apPassword"));
            if (request->hasArg("setupStep"))
                settings->setSetupStep(request->arg("setupStep"));
            if (request->hasArg("setupDone"))
                settings->setSetupDone(parseBoolArg(request->arg("setupDone")));
            if (request->hasArg("homekit"))
                settings->setHomekit(parseBoolArg(request->arg("homekit")));
            if (request->hasArg("boilerFillActive"))
                settings->setBoilerFillActive(parseBoolArg(request->arg("boilerFillActive")));
            if (request->hasArg("startupFillTime"))
                settings->setStartupFillTime(request->arg("startupFillTime").toInt() * 1000);
            if (request->hasArg("steamFillTime"))
                settings->setSteamFillTime(request->arg("steamFillTime").toInt() * 1000);
            if (request->hasArg("smartDoseActive"))
                settings->setSmartDoseActive(parseBoolArg(request->arg("smartDoseActive")));
            if (request->hasArg("smartDoseAutomagic"))
                settings->setSmartDoseAutomagic(parseBoolArg(request->arg("smartDoseAutomagic")));
            if (request->hasArg("dosingCupEnabled"))
                settings->setDosingCupEnabled(parseBoolArg(request->arg("dosingCupEnabled")));
            if (request->hasArg("portafilterEnabled"))
                settings->setPortafilterEnabled(parseBoolArg(request->arg("portafilterEnabled")));
            if (request->hasArg("portafilterScale"))
                settings->setPortafilterScale(request->arg("portafilterScale"));
            if (request->hasArg("smartDoseMode"))
                settings->setSmartDoseMode(request->arg("smartDoseMode"));
            if (request->hasArg("manualDoseScale"))
                settings->setManualDoseScale(request->arg("manualDoseScale"));
            if (request->hasArg("smartDoseScale"))
                settings->setSmartDoseScale(request->arg("smartDoseScale"));
            if (request->hasArg("smartGrindActive"))
                settings->setSmartGrindActive(parseBoolArg(request->arg("smartGrindActive")));
            if (request->hasArg("smartGrindIp"))
                settings->setSmartGrindIp(request->arg("smartGrindIp"));
            if (request->hasArg("smartGrindMode"))
                settings->setSmartGrindMode(request->arg("smartGrindMode").toInt());
            if (request->hasArg("mahlkonigActive"))
                settings->setMahlkonigActive(parseBoolArg(request->arg("mahlkonigActive")));
            if (request->hasArg("mahlkonigRecipeProfiles"))
                settings->setMahlkonigRecipeProfiles(request->arg("mahlkonigRecipeProfiles"));
            if (request->hasArg("homeAssistant"))
                settings->setHomeAssistant(parseBoolArg(request->arg("homeAssistant")));
            if (request->hasArg("haUser"))
                settings->setHomeAssistantUser(request->arg("haUser"));
            if (request->hasArg("haPassword"))
                settings->setHomeAssistantPassword(request->arg("haPassword"));
            if (request->hasArg("haIP"))
                settings->setHomeAssistantIP(request->arg("haIP"));
            if (request->hasArg("haPort"))
                settings->setHomeAssistantPort(request->arg("haPort").toInt());
            if (request->hasArg("haTopic"))
                settings->setHomeAssistantTopic(request->arg("haTopic"));
            if (request->hasArg("momentaryButtons"))
                settings->setMomentaryButtons(parseBoolArg(request->arg("momentaryButtons")));
            if (request->hasArg("flushDuration"))
                settings->setFlushDuration(request->arg("flushDuration").toInt());
            if (request->hasArg("warnWaterLevel"))
                settings->setWarnWaterLevel(request->arg("warnWaterLevel").toInt());
            if (request->hasArg("warnFlush"))
                settings->setWarnFlush(request->arg("warnFlush").toInt());
            if (request->hasArg("warnSteamSwitch"))
                settings->setWarnSteamSwitch(request->arg("warnSteamSwitch").toInt());
            if (request->hasArg("warnScaleConnected"))
                settings->setWarnScaleConnected(request->arg("warnScaleConnected").toInt());
            if (request->hasArg("warnScaleBattery"))
                settings->setWarnScaleBattery(request->arg("warnScaleBattery").toInt());
            if (request->hasArg("warnTemperature"))
                settings->setWarnTemperature(request->arg("warnTemperature").toInt());
            if (request->hasArg("warnBackflush"))
                settings->setWarnBackflush(request->arg("warnBackflush").toInt());
            if (request->hasArg("warnDescaling"))
                settings->setWarnDescaling(request->arg("warnDescaling").toInt());
            if (request->hasArg("backflushIntervalDays"))
                settings->setBackflushIntervalDays(request->arg("backflushIntervalDays").toInt());
            if (request->hasArg("backflushIntervalShots"))
                settings->setBackflushIntervalShots(request->arg("backflushIntervalShots").toInt());
            if (request->hasArg("descalingIntervalWeeks"))
                settings->setDescalingIntervalWeeks(request->arg("descalingIntervalWeeks").toInt());
            if (request->hasArg("descalingIntervalShots"))
                settings->setDescalingIntervalShots(request->arg("descalingIntervalShots").toInt());
            if (request->hasArg("delayAdjust"))
                settings->setDelayAdjust(parseBoolArg(request->arg("delayAdjust")));
            if (request->hasArg("brewDelay"))
                settings->setBrewDelay(request->arg("brewDelay").toDouble());
            if (request->hasArg("grindDelay"))
                settings->setGrindDelay(request->arg("grindDelay").toDouble());
            if (request->hasArg("timezone"))
                settings->setTimezone(request->arg("timezone"));
            if (request->hasArg("clock24hFormat"))
                settings->setClockFormat(parseBoolArg(request->arg("clock24hFormat")));
            if(request->hasArg("standbyContent"))settings->setStandbyContent(request->arg("standbyContent"));
            if(request->hasArg("standbyText"))settings->setStandbyText(request->arg("standbyText"));
            if (request->hasArg("standbyTimeout"))
                settings->setStandbyTimeout(request->arg("standbyTimeout").toInt() * 1000);
            if (request->hasArg("mainBrightness"))
                settings->setMainBrightness(request->arg("mainBrightness").toInt());
            if (request->hasArg("standbyBrightness"))
                settings->setStandbyBrightness(request->arg("standbyBrightness").toInt());
            if (request->hasArg("standbyBrightnessTimeout"))
                settings->setStandbyBrightnessTimeout(request->arg("standbyBrightnessTimeout").toInt() * 1000);
            if (request->hasArg("steamPumpPercentage"))
                settings->setSteamPumpPercentage(request->arg("steamPumpPercentage").toFloat());
            if (request->hasArg("steamPumpCutoff"))
                settings->setSteamPumpCutoff(request->arg("steamPumpCutoff").toFloat());
            if (request->hasArg("themeMode"))
                settings->setThemeMode(request->arg("themeMode").toInt());
            if (request->hasArg("statusDisplayMode"))
                settings->setStatusDisplayMode(request->arg("statusDisplayMode").toInt());
            if (request->hasArg("albaRefillInterlock"))
                settings->setAlbaRefillInterlock(request->arg("albaRefillInterlock") == "1");
            if (request->hasArg("sunriseIdle"))
                settings->setSunriseIdle(request->arg("sunriseIdle"));
            if (request->hasArg("sunriseActive"))
                settings->setSunriseActive(request->arg("sunriseActive"));
            if (request->hasArg("sunriseFinished"))
                settings->setSunriseFinished(request->arg("sunriseFinished"));
            if (request->hasArg("sunriseError"))
                settings->setSunriseError(request->arg("sunriseError"));
            if (request->hasArg("sunriseExtBrightness"))
                settings->setSunriseExtBrightness(request->arg("sunriseExtBrightness").toInt());
            if (request->hasArg("emptyTankDistance"))
                settings->setEmptyTankDistance(request->arg("emptyTankDistance").toInt());
            if (request->hasArg("fullTankDistance"))
                settings->setFullTankDistance(request->arg("fullTankDistance").toInt());
            if (request->hasArg("altRelayFunction"))
                settings->setAltRelayFunction(request->arg("altRelayFunction").toInt());
            if (request->hasArg("buttonBehavior"))
                settings->setButtonBehaviorList(explode(request->arg("buttonBehavior"), ','));
            if (request->hasArg("commutationGain"))
                settings->setCommutationGain(request->arg("commutationGain").toFloat());
            if (request->hasArg("convergenceGain"))
                settings->setConvergenceGain(request->arg("convergenceGain").toFloat());
            if (request->hasArg("integralGain"))
                settings->setIntegralGain(request->arg("integralGain").toFloat());
            if (request->hasArg("maxPumpPower"))
                settings->setMaxPumpPower(request->arg("maxPumpPower").toFloat());
            if (request->hasArg("savedScale"))
                settings->setSavedScale(request->arg("savedScale"));
            if (request->hasArg("savedScaleName"))
                settings->setSavedScaleName(request->arg("savedScaleName"));
            if (request->hasArg("autowakeupEnabled"))
                settings->setAutoWakeupEnabled(parseBoolArg(request->arg("autowakeupEnabled")));
            if (request->hasArg("autowakeupSchedules")) {
                // Handle schedule format with days
                String schedulesStr = request->arg("autowakeupSchedules");
                std::vector<AutoWakeupSchedule> schedules;

                if (schedulesStr.length() > 0) {
                    // Split semicolon-separated schedules
                    int start = 0;
                    int end = schedulesStr.indexOf(';');

                    while (end != -1 || start < schedulesStr.length()) {
                        String scheduleStr = (end != -1) ? schedulesStr.substring(start, end) : schedulesStr.substring(start);

                        int pipePos = scheduleStr.indexOf('|');
                        if (pipePos != -1) {
                            String timeStr = scheduleStr.substring(0, pipePos);
                            String daysStr = scheduleStr.substring(pipePos + 1);

                            AutoWakeupSchedule schedule;
                            schedule.time = timeStr;

                            if (daysStr.length() == 7) {
                                for (int i = 0; i < 7; i++) {
                                    schedule.days[i] = (daysStr.charAt(i) == '1');
                                }
                            }

                            schedules.push_back(schedule);
                        }

                        if (end == -1)
                            break;
                        start = end + 1;
                        end = schedulesStr.indexOf(';', start);
                    }
                }

                if (schedules.empty()) {
                    schedules.push_back(AutoWakeupSchedule("07:00")); // Default fallback
                }
                settings->setAutoWakeupSchedules(schedules);
            }
            saved = settings->save(true);
        });
        if (!saved) {
            JsonDocument failure;
            failure["error"] = (profileError.isEmpty() ? controller->getSettings().getSaveError() : profileError) +
                               ". Restart cancelled; retry saving.";
            String body;
            serializeJson(failure, body);
            request->send(500, "application/json", body);
            return;
        }
        pluginManager->trigger("settings:changed");
#ifdef GAGGIMATE_KNOB
        if (request->hasArg("knobLedEnabled") || request->hasArg("knobLedBrightness")) {
            guition_knob::Hardware::instance().applyLedPreferences(controller->getSettings().getKnobLedPreferences(), true);
        }
#endif
        if (!appearanceOnly) {
        controller->setTargetTemp(controller->getTargetTemp());
        controller->setPressureScale();
        controller->setMachineHardware();
        controller->setScaleFactors();
        controller->setHeaterSettings();
        controller->setPumpModelCoeffs();
        }

    }

    JsonDocument doc(&psramAllocator);
    Settings const &settings = controller->getSettings();
    doc["controllerError"] = controller->getError();
    doc["machineHardwareConfiguration"] = hardwareSupported;
    doc["machineHardwareReady"] = controller->isMachineHardwareReady();
    if (request->method() == HTTP_POST)
        doc["machineHardwareApplyPending"] =
            previousHardwareHash != machineHardwareHash(settings.getMachineHardware()) && !controller->isMachineHardwareReady();
    doc["machineHardwareError"] = controller->isMachineHardwareError();
    doc["steamControlsAvailable"] = controller->isSteamControlsAvailable();
    if (hardwareSupported) {
        doc["defaultSteamMode"] = settings.getDefaultSteamMode();
        doc["systemLedFunction"] = settings.getSystemLedFunction();
        doc["baristaLedFunction"] = settings.getBaristaLedFunction();
        doc["systemLedBrightness"] = settings.getSystemLedBrightness();
        doc["systemLedMinBrightness"] = settings.getSystemLedMinBrightness();
        doc["systemLedCycleTimeMs"] = settings.getSystemLedCycleTimeMs();
        doc["baristaLedBrightness"] = settings.getBaristaLedBrightness();
        doc["baristaLedMinBrightness"] = settings.getBaristaLedMinBrightness();
        doc["baristaLedCycleTimeMs"] = settings.getBaristaLedCycleTimeMs();
        doc["steamRefillMode"] = settings.getSteamRefillMode();
        doc["steamRefillPostClearSeconds"] = settings.getSteamRefillPostClearSeconds();
        writeMachineHardware(doc.as<JsonObject>(), settings.getMachineHardware());
    }
    doc["startupMode"] = settings.getStartupMode() == MODE_BREW ? "brew" : "standby";
    doc["startupProfile"] = settings.getStartupProfile();
    doc["targetSteamTemp"] = settings.getTargetSteamTemp();
    doc["targetWaterTemp"] = settings.getTargetWaterTemp();
    if (!addMachineProfileSettings(doc, settings, controller->getSystemInfo(), hardwareSupported)) {
        request->send(500, "application/json", "{\"error\":\"Could not read machine profile settings\"}");
        return;
    }
    doc["setupDone"] = settings.isSetupDone();
    doc["setupStep"] = settings.getSetupStep();
    doc["homekit"] = settings.isHomekit();
    doc["homeAssistant"] = settings.isHomeAssistant();
    doc["haUser"] = settings.getHomeAssistantUser();
    doc["haPassword"] = settings.getHomeAssistantPassword();
    doc["haIP"] = settings.getHomeAssistantIP();
    doc["haPort"] = settings.getHomeAssistantPort();
    doc["haTopic"] = settings.getHomeAssistantTopic();
    doc["heaterCoordinationEnabled"] = settings.isHeaterCoordinationEnabled();
    doc["heaterHandoverMs"] = settings.getHeaterHandoverMs();
    doc["pid"] = settings.getPid();
    doc["steamPid"] = settings.getSteamPid();
    doc["pumpModelCoeffs"] = settings.getPumpModelCoeffs();
    doc["pumpSlipCoeffs"] = settings.getPumpSlipCoeffs();
    doc["wifiSsid"] = settings.getWifiSsid();
    doc["wifiPassword"] = apMode ? "---unchanged---" : settings.getWifiPassword();
    doc["apPassword"] = settings.getWifiApPassword();
    doc["mdnsName"] = settings.getMdnsName();
    doc["temperatureOffset"] = String(settings.getTemperatureOffset());
    doc["pressureOffset"] = String(settings.getPressureOffset());
    doc["pressureScaling"] = String(settings.getPressureScaling());
    doc["scaleFactor1"] = settings.getScaleFactor1();
    doc["scaleFactor2"] = settings.getScaleFactor2();
    doc["hardwareScaleSampleRateSps"] = settings.getHardwareScaleSampleRateSps();
    doc["hardwareScaleIdleAlpha"] = settings.getHardwareScaleIdleAlpha();
    doc["hardwareScaleActiveAlpha"] = settings.getHardwareScaleActiveAlpha();
    doc["preferredScaleSource"] = settings.getPreferredScaleSource();
    doc["boilerFillActive"] = settings.isBoilerFillActive();
    doc["startupFillTime"] = settings.getStartupFillTime() / 1000;
    doc["steamFillTime"] = settings.getSteamFillTime() / 1000;
    doc["smartDoseActive"] = settings.isSmartDoseActive();
    doc["smartDoseAutomagic"] = settings.isSmartDoseAutomagic();
    doc["dosingCupEnabled"] = settings.isDosingCupEnabled();
    doc["portafilterEnabled"] = settings.isPortafilterEnabled();
    doc["portafilterScale"] = settings.getPortafilterScale();
    doc["portafilterWeight"] = settings.getPortafilterWeight();
    doc["smartDoseMode"] = settings.getSmartDoseMode();
    doc["smartDoseScale"] = settings.getSmartDoseScale();
    doc["manualDoseScale"] = settings.getManualDoseScale();
    doc["dosingCupWeight"] = settings.getDosingCupWeight();
    doc["smartGrindActive"] = settings.isSmartGrindActive();
    doc["smartGrindIp"] = settings.getSmartGrindIp();
    doc["smartGrindMode"] = settings.getSmartGrindMode();
    doc["mahlkonigActive"] = settings.isMahlkonigActive();
    doc["mahlkonigRecipeProfiles"] = settings.getMahlkonigRecipeProfiles();
    doc["apMode"] = apMode; // read-only, the setup wizard offers Wi-Fi setup in AP mode
    doc["deviceIp"] = apMode ? WIFI_AP_IP.toString() : WiFi.localIP().toString(); // read-only, for plugin setup hints
    doc["momentaryButtons"] = settings.isMomentaryButtons();
    doc["flushDuration"] = settings.getFlushDuration();
    doc["warnWaterLevel"] = settings.getWarnWaterLevel();
    doc["warnFlush"] = settings.getWarnFlush();
    doc["warnSteamSwitch"] = settings.getWarnSteamSwitch();
    doc["warnScaleConnected"] = settings.getWarnScaleConnected();
    doc["warnScaleBattery"] = settings.getWarnScaleBattery();
    doc["warnTemperature"] = settings.getWarnTemperature();
    doc["warnBackflush"] = settings.getWarnBackflush();
    doc["warnDescaling"] = settings.getWarnDescaling();
    doc["backflushIntervalDays"] = settings.getBackflushIntervalDays();
    doc["backflushIntervalShots"] = settings.getBackflushIntervalShots();
    doc["descalingIntervalWeeks"] = settings.getDescalingIntervalWeeks();
    doc["descalingIntervalShots"] = settings.getDescalingIntervalShots();
    doc["lastBackflushTime"] = settings.getLastBackflushTime();
    doc["lastDescalingTime"] = settings.getLastDescalingTime();
    doc["shotsSinceBackflush"] = settings.getShotsSinceBackflush();
    doc["shotsSinceDescaling"] = settings.getShotsSinceDescaling();
    doc["brewDelay"] = settings.getBrewDelay();
    doc["grindDelay"] = settings.getGrindDelay();
    doc["delayAdjust"] = settings.isDelayAdjust();
    doc["timezone"] = settings.getTimezone();
    doc["clock24hFormat"] = settings.isClock24hFormat();
    const auto standbyAppearance=settings.getStandbyAppearance();
    doc["standbyContent"]=gaggimate_ui::standbyContentName(standbyAppearance.content);
    doc["standbyText"]=standbyAppearance.text;
    doc["standbyTimeout"] = settings.getStandbyTimeout() / 1000;
    doc["mainBrightness"] = settings.getMainBrightness();
    doc["standbyBrightness"] = settings.getStandbyBrightness();
    doc["standbyBrightnessTimeout"] = settings.getStandbyBrightnessTimeout() / 1000;
    doc["steamPumpPercentage"] = settings.getSteamPumpPercentage();
    doc["steamPumpCutoff"] = settings.getSteamPumpCutoff();
    doc["themeMode"] = settings.getThemeMode();
#ifdef GAGGIMATE_KNOB
    const auto ledPreferences = settings.getKnobLedPreferences();
    doc["knobLedSupported"] = true;
    doc["knobLedEnabled"] = ledPreferences.enabled;
    doc["knobLedBrightness"] = ledPreferences.brightness;
#endif
    doc["statusDisplayMode"] = settings.getStatusDisplayMode();
    doc["albaRefillInterlock"] = settings.getAlbaRefillInterlock();
    doc["sunriseIdle"] = settings.getSunriseIdle();
    doc["sunriseActive"] = settings.getSunriseActive();
    doc["sunriseFinished"] = settings.getSunriseFinished();
    doc["sunriseError"] = settings.getSunriseError();
    doc["sunriseExtBrightness"] = settings.getSunriseExtBrightness();
    doc["emptyTankDistance"] = settings.getEmptyTankDistance();
    doc["fullTankDistance"] = settings.getFullTankDistance();
    doc["altRelayFunction"] = settings.getAltRelayFunction();
    // Add auto-wakeup settings to response
    doc["autowakeupEnabled"] = settings.isAutoWakeupEnabled();
    doc["buttonBehavior"] = implode(settings.getButtonBehaviorList(), ",");
    doc["commutationGain"] = settings.getCommutationGain();
    doc["convergenceGain"] = settings.getConvergenceGain();
    doc["integralGain"] = settings.getIntegralGain();
    doc["maxPumpPower"] = settings.getMaxPumpPower();
    doc["savedScale"] = settings.getSavedScale();
    doc["savedScaleName"] = settings.getSavedScaleName();

    // Add schedule format with days
    std::vector<AutoWakeupSchedule> autowakeupSchedules = settings.getAutoWakeupSchedules();
    String schedulesStr = "";
    for (size_t i = 0; i < autowakeupSchedules.size(); i++) {
        if (i > 0)
            schedulesStr += ";";
        schedulesStr += autowakeupSchedules[i].time + "|";

        // Convert days array to 7-bit string
        for (int j = 0; j < 7; j++) {
            schedulesStr += autowakeupSchedules[i].days[j] ? "1" : "0";
        }
    }
    doc["autowakeupSchedules"] = schedulesStr;
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);

    if (request->method() == HTTP_POST && request->hasArg("restart"))
        ESP.restart();
}

void WebUIPlugin::handleBLEScaleList(AsyncWebServerRequest *request) {
    JsonDocument doc(&psramAllocator);
    JsonArray scalesArray = doc.to<JsonArray>();
    std::vector<DiscoveredDevice> devices = BLEScales.getDiscoveredScales();
    for (const DiscoveredDevice &device : BLEScales.getDiscoveredScales()) {
        JsonDocument scale(&psramAllocator);
        scale["uuid"] = device.getAddress().toString();
        scale["name"] = device.getName();
        scale["rssi"] = device.getRSSI();
        scalesArray.add(scale);
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

void WebUIPlugin::handleBLEScaleScan(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_POST) {
        request->send(404);
        return;
    }
    BLEScales.scan();
    JsonDocument doc(&psramAllocator);
    doc["success"] = true;
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

void WebUIPlugin::handleBLEScaleConnect(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_POST) {
        request->send(404);
        return;
    }
    BLEScales.connect(request->arg("uuid").c_str());
    JsonDocument doc(&psramAllocator);
    doc["success"] = true;
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

void WebUIPlugin::handleBLEScaleForget(AsyncWebServerRequest *request) {
    if (request->method() != HTTP_POST) {
        request->send(404);
        return;
    }
    BLEScales.forget();
    JsonDocument doc(&psramAllocator);
    doc["success"] = true;
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

void WebUIPlugin::handleBLEScaleInfo(AsyncWebServerRequest *request) {
    JsonDocument doc(&psramAllocator);
    doc["connected"] = BLEScales.isConnected();
    doc["name"] = BLEScales.getName();
    doc["uuid"] = BLEScales.getUUID();
    doc["rssi"] = BLEScales.getRSSI();
    doc["saved"] = controller->getSettings().getSavedScale();
    doc["savedName"] = controller->getSettings().getSavedScaleName();
    doc["hasBattery"] = BLEScales.hasBatteryLevel();
    // Only surface the numeric when the scale reports one — a 255 sentinel
    // (REMOTE_SCALES_BATTERY_UNKNOWN) would otherwise render as a fake "255%".
    if (BLEScales.hasBatteryLevel()) {
        const uint8_t pct = BLEScales.getBatteryLevel();
        if (pct != REMOTE_SCALES_BATTERY_UNKNOWN) {
            doc["battery"] = pct;
        }
    }
    AsyncResponseStream *response = request->beginResponseStream("application/json");
    serializeJson(doc, *response);
    request->send(response);
}

#ifdef GAGGIMATE_KNOB
screen_ota::Preflight WebUIPlugin::screenPreflight() const {
    const auto pair = matchedReleases.snapshot();
    if (pair.status != "ready") return {};
    if (!screen_ota::metadataFresh(pair.validatedAt, millis())) return {"refresh-required", {}};
    const auto system = controller->getSystemInfo();
    screen_ota::Context local;
    local.portVersion = BUILD_PORT_VERSION; local.controllerVersion = system.version.c_str();
    local.localProtocol = gm_proto::PROTOCOL_VERSION; local.controllerProtocol = system.protocolVersion;
    local.controllerReady = controller->getSystemState() == SYSTEM_READY;
    local.activeProcess = controller->isActive() || controller->isGrindActive();
    local.updating = updating || controller->isUpdating() || screenInstaller.snapshot().busy;
    local.bootHealthy = !guition_knob::bootHealthPending() &&
        (std::strcmp(guition_knob::bootHealthState(), "healthy") == 0 ||
         std::strcmp(guition_knob::bootHealthState(), "confirmed") == 0);
    local.standby = controller->getMode() == MODE_STANDBY;
    const auto *running = esp_ota_get_running_partition();
    const auto *next = esp_ota_get_next_update_partition(nullptr);
    const auto *boot = esp_ota_get_boot_partition();
    if (running) { local.runningOffset = running->address; local.runningBytes = running->size; }
    if (next) { local.updateOffset = next->address; local.updateBytes = next->size; }
    esp_ota_img_states_t runningState;
    local.bootSelectionCurrent = running && boot && running->address == boot->address &&
        esp_ota_get_state_partition(running, &runningState) == ESP_OK &&
        (runningState == ESP_OTA_IMG_VALID || runningState == ESP_OTA_IMG_UNDEFINED);
    return screen_ota::inspect(true, pair.release, pair.pair, local,
        std::strcmp(BUILD_RELEASE_CHANNEL, "candidate") == 0);
}
void WebUIPlugin::handleScreenOTAStart(JsonDocument &request) {
    const auto preflight = screenPreflight();
    screen_ota::Request selected;
    selected.component = matched_release::text(request["component"]);
    selected.tag = matched_release::text(request["tag"]);
    selected.manifestSha = matched_release::text(request["manifestSha256"]);
    selected.imageSha = matched_release::text(request["imageSha256"]);
    bool valid = request["url"].isNull() && request["downloadUrl"].isNull();
    for (const char *key : {"qualification", "verifyOnly"})
        if (!request[key].isNull() && !request[key].is<bool>()) valid = false;
    selected.qualification = request["qualification"].is<bool>() && request["qualification"].as<bool>();
    selected.verifyOnly = request["verifyOnly"].is<bool>() && request["verifyOnly"].as<bool>();
    const bool accepted = valid && screenInstaller.queue(preflight, selected);
    JsonDocument response(&psramAllocator);
    response["tp"] = "res:screen-ota-start";
    if (request["rid"].is<const char *>()) response["rid"] = request["rid"];
    response["accepted"] = accepted;
    const bool permission = selected.qualification ? BUILD_SCREEN_OTA_QUALIFICATION != 0 :
        screen_ota::screenInstallationAllowed && !selected.verifyOnly;
    response["state"] = accepted ? "queued" : (!valid ? "invalid-request" :
        (!permission ? "installation-disabled" : (!preflight.ready() ? preflight.state : "selection-changed")));
    wsHandler.broadcastJson(response);
    updateOTAStatus(ota->getCurrentVersion());
}
#endif

void WebUIPlugin::updateOTAStatus(const String &version) {
    if (!wsHandler.hasClients())
        return;
    Settings const &settings = controller->getSettings();
    JsonDocument doc(&psramAllocator);
    doc["tp"] = "res:ota-settings";
#ifdef GAGGIMATE_KNOB
    const auto official = officialReleases.snapshot();
    doc["latestVersion"] = official.version;
    doc["displayUpdateAvailable"] = official.displayUpdate;
    doc["controllerUpdateAvailable"] = official.controllerUpdate;
    doc["officialCheckStatus"] = official.status;
    doc["officialCheckMessage"] = official.message;
#else
    doc["latestVersion"] = ota->getCurrentVersion();
    doc["displayUpdateAvailable"] = ota->isUpdateAvailable(false);
    doc["controllerUpdateAvailable"] = ota->isUpdateAvailable(true);
#endif
    doc["otaAllowed"] = FirmwareOtaPolicy::installationAllowed;
    doc["otaDisabledReason"] = FirmwareOtaPolicy::disabledReason;
    doc["displayVersion"] = BUILD_GIT_VERSION;
    doc["controllerVersion"] = controller->getSystemInfo().version;
    doc["hardware"] = controller->getSystemInfo().hardware;
    doc["channel"] = settings.getOTAChannel();
    doc["updating"] = updating;
#ifdef GAGGIMATE_KNOB
    const auto pair = matchedReleases.snapshot();
    const auto system = controller->getSystemInfo();
    const bool ready = pair.status == "ready";
    doc["displayPortVersion"] = BUILD_PORT_VERSION;
    doc["displayUpstreamVersion"] = BUILD_UPSTREAM_VERSION;
    doc["officialDisplayVersion"] = BUILD_UPSTREAM_VERSION;
    doc["displayProtocol"] = gm_proto::PROTOCOL_VERSION;
    doc["controllerProtocol"] = system.protocolVersion;
    const bool controllerReady = controller->getSystemState() == SYSTEM_READY;
    doc["controllerConnected"] = controllerReady;
    if (!controllerReady) doc["controllerVersion"] = "";
    doc["installedCompatibility"] = matched_release::installedCompatibility(BUILD_UPSTREAM_VERSION,
        gm_proto::PROTOCOL_VERSION, system.protocolVersion, controller->getSystemState() == SYSTEM_READY,
        system.version.c_str());
    doc["pairSupported"] = true;
    doc["pairStatus"] = pair.status;
    doc["pairMessage"] = pair.message;
    doc["pairSource"] = "https://github.com/Bridgee/gaggimate-knob-screen/releases";
    doc["pairIncludesPrereleases"] = std::strcmp(BUILD_RELEASE_CHANNEL, "candidate") == 0;
    doc["pairReleaseTag"] = pair.release.tag;
    doc["pairPrerelease"] = pair.release.preview;
    doc["pairReleaseUrl"] = ready ? std::string(matched_release::ReleaseRoot) + "tag/" + pair.release.tag : "";
    doc["pairDisplayVersion"] = pair.pair.screenVersion;
    doc["pairControllerVersion"] = pair.pair.controllerVersion;
    doc["pairProtocol"] = pair.pair.protocol;
    doc["pairUpstreamVersion"] = pair.pair.upstreamVersion;
    doc["pairDisplayState"] = ready ? matched_release::componentState(BUILD_PORT_VERSION, pair.pair.screenVersion) : "unknown";
    doc["pairControllerState"] = ready && controllerReady ? matched_release::componentState(system.version.c_str(), pair.pair.controllerVersion) : "unknown";
    doc["pairCompatibility"] = ready ? matched_release::compatibility(pair.pair, gm_proto::PROTOCOL_VERSION,
        system.protocolVersion, controller->getSystemState() == SYSTEM_READY, system.version.c_str()) : "unknown";
    const auto preflight = screenPreflight();
    doc["screenOtaInstallationAllowed"] = screen_ota::screenInstallationAllowed;
    doc["controllerOtaInstallationAllowed"] = screen_ota::controllerInstallationAllowed;
    doc["screenOtaPreflight"] = preflight.state;
    doc["screenBootHealth"] = guition_knob::bootHealthState();
    doc["screenBootPending"] = guition_knob::bootHealthPending();
    doc["screenBootSlot"] = guition_knob::bootHealthSlot();
    const auto screenStatus = screenInstaller.snapshot();
    doc["screenOtaQualificationAvailable"] = BUILD_SCREEN_OTA_QUALIFICATION != 0;
    doc["screenOtaSelectionTag"] = ready ? pair.release.tag : "";
    doc["screenOtaManifestSha256"] = ready ? pair.release.manifest.sha : "";
    doc["screenOtaImageSha256"] = ready ? pair.release.screen.sha : "";
    doc["screenOtaPhase"] = screenStatus.phase;
    doc["screenOtaProgress"] = screenStatus.progress;
    doc["screenOtaError"] = screenStatus.error;
    doc["screenOtaVerified"] = screenStatus.verified;
    doc["screenOtaVerifiedSha256"] = screenStatus.verifiedSha;
    doc["updating"] = updating || screenStatus.busy || controller->isUpdating();

#endif
    // LittleFS usage metrics
    {
        size_t total = LittleFS.totalBytes();
        size_t used = LittleFS.usedBytes();
        size_t freeBytes = total > used ? (total - used) : 0;
        doc["spiffsTotal"] = static_cast<uint32_t>(total);
        doc["spiffsUsed"] = static_cast<uint32_t>(used);
        doc["spiffsFree"] = static_cast<uint32_t>(freeBytes);
        if (total > 0) {
            doc["spiffsUsedPct"] = static_cast<uint8_t>((used * 100) / total);
        }
    }
    // Memory usage metrics
    {
        size_t free = heap_caps_get_free_size(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
        size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
        size_t total = heap_caps_get_total_size(MALLOC_CAP_DEFAULT | MALLOC_CAP_INTERNAL);
        doc["heapFree"] = static_cast<uint32_t>(free);
        doc["heapLargest"] = static_cast<uint32_t>(largest);
        doc["heapTotal"] = static_cast<uint32_t>(total);
    }
    doc["controllerTaskHealth"] = controller->isTaskHealthy();
#ifndef GAGGIMATE_HEADLESS
    doc["uiTaskHealth"] = controller->getUI()->isTaskHealthy();
#endif
    if (controller->isSDCard()) {
        const uint64_t total = SD_MMC.cardSize();
        const uint64_t used = SD_MMC.usedBytes();
        const uint64_t freeBytes = total > used ? (total - used) : 0;
        doc["sdTotal"] = total;
        doc["sdUsed"] = used;
        doc["sdFree"] = freeBytes;
        if (total > 0) {
            // Provide integer percentage to avoid float JSON
            doc["sdUsedPct"] = static_cast<uint8_t>((used * 100) / total);
        }
    }
    wsHandler.broadcastJson(doc);
}

void WebUIPlugin::updateOTAProgress(uint8_t phase, int progress) {
    if (!wsHandler.hasClients())
        return;
    JsonDocument doc(&psramAllocator);
    doc["tp"] = "evt:ota-progress";
    doc["phase"] = phase;
    doc["progress"] = progress;
    wsHandler.broadcastJson(doc);
}

static String profileImageTempPath(const String &id) { return String(profile_image::DIR) + "/" + id + ".tmp"; }

void WebUIPlugin::handleProfileImageBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    if (request->method() != HTTP_POST)
        return;
    const String id = request->arg("id");
    if (index == 0) {
        if (imageUploadFile)
            imageUploadFile.close();
        imageUploadRequest = request;
        imageUploadError = "";
        if (!controller->isSDCard())
            imageUploadError = "No SD card";
        else if (controller->isUpdating())
            imageUploadError = "Update in progress";
        else if (!profile_image::isValidId(id) || !profileManager->profileExists(id))
            imageUploadError = "Unknown profile";
        else if (total != profile_image::FILE_BYTES || len < profile_image::HEADER_BYTES)
            imageUploadError = "Image must be 300x300";
        else {
            uint32_t header = 0;
            memcpy(&header, data, sizeof(header));
            if (header != profile_image::HEADER)
                imageUploadError = "Invalid image header";
        }
        if (imageUploadError.isEmpty()) {
            SD_MMC.mkdir(profile_image::DIR);
            imageUploadFile = SD_MMC.open(profileImageTempPath(id), FILE_WRITE);
            if (!imageUploadFile)
                imageUploadError = "Could not write to SD card";
        }
    }
    if (request != imageUploadRequest || !imageUploadError.isEmpty() || !imageUploadFile)
        return;
    if (imageUploadFile.write(data, len) != len) {
        imageUploadError = "Could not write to SD card";
        imageUploadFile.close();
        SD_MMC.remove(profileImageTempPath(id));
        return;
    }
    if (index + len == total) {
        imageUploadFile.close();
        SD_MMC.remove(profile_image::path(id));
        if (!SD_MMC.rename(profileImageTempPath(id).c_str(), profile_image::path(id).c_str()))
            imageUploadError = "Could not write to SD card";
    }
}

void WebUIPlugin::handleProfileImage(AsyncWebServerRequest *request) {
    const String id = request->arg("id");
    if (!controller->isSDCard()) {
        request->send(404, "text/plain", "No SD card");
        return;
    }
    if (controller->isUpdating()) {
        request->send(503, "text/plain", "Update in progress");
        return;
    }
    if (!profile_image::isValidId(id)) {
        request->send(400, "text/plain", "Invalid profile id");
        return;
    }
    switch (request->method()) {
    case HTTP_GET:
#ifdef GAGGIMATE_KNOB
        {
            File file = SD_MMC.open(profile_image::path(id), FILE_READ);
            if (!file) { request->send(404, "text/plain", "No image"); return; }
            auto image = profile_http::BufferedImage::load(file, profile_image::FILE_BYTES, profile_image::HEADER,
                [](size_t size) {
                    auto *bytes = static_cast<uint8_t *>(heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                    return std::shared_ptr<uint8_t>(bytes, [](uint8_t *p) { heap_caps_free(p); });
                });
            file.close();
            if (!image) { request->send(503, "text/plain", "Could not buffer profile image"); return; }
            static_assert(profile_http::ImageTransfer::Retry == RESPONSE_TRY_AGAIN, "Review server retry semantics");
            static profile_http::SendSlots slots(2);
            auto transfer = std::make_shared<profile_http::ImageTransfer>(image, slots);
            request->send(request->beginResponse("application/octet-stream", profile_image::FILE_BYTES,
                [transfer](uint8_t *buffer, size_t length, size_t offset) {
                    return transfer->copy(buffer, length, offset);
                }));
        }
#else
        if (SD_MMC.exists(profile_image::path(id)))
            request->send(SD_MMC, profile_image::path(id), "application/octet-stream");
        else
            request->send(404, "text/plain", "No image");
#endif
        return;
    case HTTP_DELETE:
        SD_MMC.remove(profile_image::path(id));
        pluginManager->trigger("profiles:image:change", "id", id);
        request->send(200, "application/json", "{}");
        return;
    case HTTP_POST: {
        if (request != imageUploadRequest) {
            request->send(400, "text/plain", "Image must be 300x300");
            return;
        }
        imageUploadRequest = nullptr;
        if (imageUploadFile) {
            imageUploadFile.close(); // body ended short of Content-Length
            imageUploadError = "Incomplete upload";
        }
        if (!imageUploadError.isEmpty()) {
            request->send(400, "text/plain", imageUploadError);
            return;
        }
        pluginManager->trigger("profiles:image:change", "id", id);
        request->send(200, "application/json", "{}");
        return;
    }
    default:
        request->send(405);
    }
}

void WebUIPlugin::handleCoreDumpDownload(AsyncWebServerRequest *request) {
    // Check if core dump is available
    size_t coreAddr, coreSize;
    if (esp_core_dump_image_get(&coreAddr, &coreSize) != ESP_OK || coreSize == 0) {
        request->send(404, "text/plain", "No core dump available");
        return;
    }

    // Find the coredump partition
    const esp_partition_t *coredump_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_COREDUMP, nullptr);
    if (coredump_partition == nullptr) {
        request->send(500, "text/plain", "Core dump partition not found");
        return;
    }

    ESP_LOGI("WebUIPlugin", "Streaming core dump: %d bytes from 0x%x", coreSize, coreAddr);

    // Create a streaming response
    AsyncWebServerResponse *response =
        request->beginResponse("application/octet-stream", coreSize,
                               [coredump_partition, coreSize](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
                                   // Calculate how much to read
                                   size_t remaining = coreSize - index;
                                   size_t toRead = (remaining < maxLen) ? remaining : maxLen;

                                   if (toRead == 0)
                                       return 0;

                                   // Read from partition
                                   esp_err_t err = esp_partition_read(coredump_partition, index, buffer, toRead);
                                   if (err != ESP_OK) {
                                       ESP_LOGE("WebUIPlugin", "Failed to read core dump: %s", esp_err_to_name(err));
                                       return 0;
                                   }

                                   return toRead;
                               });

    // Set appropriate headers
    response->addHeader("Content-Disposition", "attachment; filename=\"coredump.bin\"");
    response->addHeader("Cache-Control", "no-cache");

    request->send(response);
}
