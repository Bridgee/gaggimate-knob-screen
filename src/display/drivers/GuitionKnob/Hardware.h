#pragma once
#ifdef GAGGIMATE_KNOB
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "HardwarePolicy.h"
#include "LedGaugePolicy.h"
#include <driver/spi_master.h>
#include <esp_timer.h>
#include "EncoderDetents.h"

class AsyncWebServer;
namespace guition_knob {
enum class HardwareTest : uint8_t { Led, Speaker, Microphone };
struct HardwareState {
    bool ready = false, encoderReady = false, busy = false;
    uint16_t railMv = 0;
    PowerSource powerSource = PowerSource::Unknown;
    bool usbHostActive = false;
    int batteryPercent = -1;
    int32_t encoderTicks = 0;
    int32_t encoderDetents = 0;
    uint32_t encoderInvalidTransitions = 0; // Legacy quadrature diagnostic, retained as0.
    uint32_t encoderSamples = 0, encoderRightSteps = 0, encoderLeftSteps = 0;
    uint32_t encoderAmbiguousPulses = 0, encoderShortPulses = 0;
    uint8_t encoderStablePhase = 3;
    int32_t encoderPcntTicks = 0;
    uint32_t encoderEdgesA = 0, encoderEdgesB = 0;
    uint8_t encoderPinA = 1, encoderPinB = 1;
    uint32_t ledClockHz = 0;
    LedConfig ledConfig;
    bool ledUiActive = false;
    uint8_t ledGaugeCount = 0;
    int ledError = 0;
    uint32_t ledUpdatedMs = 0;
    LedFrame ledFrame{};
    uint32_t jobId = 0, completedJobId = 0;
    int lastError = 0;
    uint32_t micSamples = 0, micNonzero = 0;
    float micRms = 0;
    int micPeakToPeak = 0;
    char result[96] = "Not tested";
};

class Hardware {
  public:
    static Hardware &instance() { static Hardware h; return h; }
    void begin();
    HardwareState snapshot();
    bool requestTest(HardwareTest test, uint32_t &jobId);
    void bindWeb(AsyncWebServer &server);
    bool configureLeds(const LedConfig &config);
    void publishLedGauges(const GaugeFrame &frame);
  private:
    HardwareState state;
    portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
    QueueHandle_t commands = nullptr;
    static void worker(void *arg);
    void run();
    void sampleRail();
    void sampleEncoder();
    int testLed();
    int openLeds();
    int writeLeds(const LedFrame &frame);
    void closeLeds();
    void serviceLeds();
    GaugeFrame ledGauges;
    uint32_t ledPublishedMs = 0, lastLedService = 0;
    bool ledInstalled = false, ledFrameSent = false;
    spi_device_handle_t ledSpi = nullptr;
    uint8_t *ledDma = nullptr;
    int testSpeaker();
    int testMicrophone();
    static void IRAM_ATTR encoderEdge(void *arg);
    uint8_t encoderPrevious = 0;
    bool encoderPcntReady = false;
    esp_timer_handle_t encoderTimer = nullptr;
    EncoderDetents detentDecoder;
    static void sampleDetents(void *arg);
};
} // namespace guition_knob
#endif
