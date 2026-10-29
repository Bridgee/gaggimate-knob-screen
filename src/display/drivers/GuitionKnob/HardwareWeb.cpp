#ifdef GAGGIMATE_KNOB
#include "Hardware.h"
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

namespace guition_knob {
void Hardware::bindWeb(AsyncWebServer &server) {
    server.on("/hardware", HTTP_GET, [](AsyncWebServerRequest *request) {
        // Retire the bench page explicitly: the ordinary SPA fallback would
        // otherwise serve index.html for this former URL. Keep callable APIs below.
        request->send(404, "text/plain; charset=utf-8", "Not found");
    });
    server.on("/api/hardware", HTTP_GET, [this](AsyncWebServerRequest *request) {
        const auto s = snapshot();
        JsonDocument doc;
        doc["ready"] = s.ready; doc["busy"] = s.busy;
        doc["railMv"] = s.railMv; doc["batteryPercent"] = s.batteryPercent;
        doc["powerSource"] = s.powerSource == PowerSource::External ? "external" : s.powerSource == PowerSource::Battery ? "battery" : "unknown";
        doc["usbHostActive"] = s.usbHostActive;
        doc["powerSourceMethod"] = s.usbHostActive ? "USB SOF activity" : "SYS voltage estimate";
        doc["encoderReady"] = s.encoderReady; doc["encoderTicks"] = s.encoderTicks;
        doc["encoderSamples"]=s.encoderSamples;doc["encoderRightSteps"]=s.encoderRightSteps;doc["encoderLeftSteps"]=s.encoderLeftSteps;
        doc["encoderAmbiguousPulses"]=s.encoderAmbiguousPulses;doc["encoderShortPulses"]=s.encoderShortPulses;
        doc["encoderDetents"]=s.encoderDetents;doc["encoderInvalidTransitions"]=s.encoderInvalidTransitions;doc["encoderStablePhase"]=s.encoderStablePhase;
        doc["encoderPcntTicks"] = s.encoderPcntTicks;
        doc["encoderPinA"] = s.encoderPinA; doc["encoderPinB"] = s.encoderPinB;
        doc["encoderEdgesA"] = s.encoderEdgesA; doc["encoderEdgesB"] = s.encoderEdgesB;
        doc["ledClockHz"] = s.ledClockHz;
        doc["ledMode"] = ledModeName(s.ledConfig.mode);
        doc["ledBackend"]="SPI3 DMA";doc["ledCount"]=s.ledConfig.count;
        char white[7];snprintf(white,sizeof(white),"%06x",unsigned(s.ledConfig.white));doc["ledWhite"]=white;
        doc["ledTopIndex"] = s.ledConfig.topIndex; doc["ledClockwise"] = s.ledConfig.clockwise;
        doc["ledBrightness"] = s.ledConfig.brightness; doc["ledUiActive"] = s.ledUiActive;
        doc["ledGaugeCount"] = s.ledGaugeCount; doc["ledError"] = s.ledError; doc["ledUpdatedMs"] = s.ledUpdatedMs;
        for (auto color : s.ledFrame) { char hex[8]; snprintf(hex, sizeof(hex), "#%06x", unsigned(color)); doc["ledPixels"].add(hex); }
        doc["jobId"] = s.jobId; doc["completedJobId"] = s.completedJobId;
        doc["lastError"] = s.lastError; doc["result"] = s.result;
        doc["micSamples"] = s.micSamples; doc["micNonzero"] = s.micNonzero;
        doc["micRms"] = s.micRms; doc["micPeakToPeak"] = s.micPeakToPeak;
        doc["uptimeMs"] = millis(); doc["freeHeap"] = ESP.getFreeHeap();
        auto *response = request->beginResponseStream("application/json");
        response->addHeader("Cache-Control", "no-store");
        serializeJson(doc, *response);
        request->send(response);
    });
    server.on("/api/hardware/led", HTTP_POST, [this](AsyncWebServerRequest *request) {
        for (const char *name : {"mode", "top", "clockwise", "brightness"}) {
            if (!request->hasParam(name, true)) { request->send(400, "text/plain", "Missing LED setting"); return; }
        }
        const String mode = request->getParam("mode", true)->value();
        const String direction = request->getParam("clockwise", true)->value();
        auto number = [&](const char *name, unsigned maximum, unsigned &value) {
            const String text = request->getParam(name, true)->value();
            if (!text.length() || text.length() > 3) return false;
            value = 0;
            for (unsigned i = 0; i < text.length(); ++i) {
                if (text[i] < '0' || text[i] > '9') return false;
                value = value * 10 + unsigned(text[i] - '0');
            }
            return value <= maximum;
        };
        unsigned top, brightness, count=snapshot().ledConfig.count;
        if(request->hasParam("count",true) && !number("count",RingLedCount,count)){request->send(400,"text/plain","Invalid LED count");return;}
        if ((mode != "off" && mode != "ui" && mode != "identify" && mode != "red" && mode != "green" && mode != "blue" && mode != "white") || (direction != "0" && direction != "1") ||
            !count || !number("top", count - 1, top) || !number("brightness", 255, brightness)) {
            request->send(400, "text/plain", "Invalid LED setting"); return;
        }
        LedConfig config;
        config.mode=LedMode::Off;
        for(auto candidate:{LedMode::Ui,LedMode::Identify,LedMode::Red,LedMode::Green,LedMode::Blue,LedMode::White})
            if(mode==ledModeName(candidate))config.mode=candidate;
        config.count=count;config.white=snapshot().ledConfig.white;
        if(request->hasParam("white",true)) {
            const String text=request->getParam("white",true)->value();
            if(text.length()!=6){request->send(400,"text/plain","Invalid white color");return;}
            config.white=0;
            for(unsigned i=0;i<6;++i) {
                const char c=text[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:-1;
                if(n<0){request->send(400,"text/plain","Invalid white color");return;}
                config.white=(config.white<<4)|unsigned(n);
            }
        }
        config.topIndex = top; config.brightness = brightness; config.clockwise = direction == "1";
        if (!configureLeds(config)) { request->send(409, "text/plain", "Hardware busy or unavailable"); return; }
        request->send(202, "application/json", "{\"accepted\":true}");
    });
    server.on("/api/hardware/test", HTTP_POST, [this](AsyncWebServerRequest *request) {
        if (!request->hasParam("action", true)) { request->send(400, "text/plain", "Missing action"); return; }
        const String action = request->getParam("action", true)->value();
        HardwareTest test;
        if (action == "led") test = HardwareTest::Led;
        else if (action == "speaker") test = HardwareTest::Speaker;
        else if (action == "mic") test = HardwareTest::Microphone;
        else { request->send(400, "text/plain", "Unknown test"); return; }
        uint32_t id = 0;
        if (!requestTest(test, id)) { request->send(409, "text/plain", "Hardware busy or unavailable"); return; }
        request->send(202, "application/json", String("{\"jobId\":") + id + "}");
    });
}
} // namespace guition_knob
#endif
