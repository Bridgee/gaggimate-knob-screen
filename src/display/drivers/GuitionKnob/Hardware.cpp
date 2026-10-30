#ifdef GAGGIMATE_KNOB
#include "Hardware.h"
#include "HardwarePolicy.h"
#include "Diagnostics.h"
#include <driver/pcnt.h>
#include "LedSpiEncoding.h"
#include <esp_heap_caps.h>
#include <driver/i2s.h>
#include <soc/usb_serial_jtag_reg.h>
#include <soc/gpio_struct.h>
#include <cmath>
#include <algorithm>

namespace guition_knob {
namespace {
constexpr int BatteryAdc = 6, EncoderA = 2, EncoderB = 1, LedPin = 0;
constexpr int SpeakerBck = 3, SpeakerWs = 45, SpeakerData = 42, SpeakerEnable = 46;
constexpr int MicClock = 5, MicData = 4;
constexpr i2s_port_t SpeakerPort = I2S_NUM_1, MicPort = I2S_NUM_0;
DRAM_ATTR const int8_t EncoderTransitions[] = {0, 1, -1, 0, -1, 0, 0, 1, 1, 0, 0, -1, 0, -1, 1, 0};

i2s_config_t audioConfig(bool microphone) {
    i2s_config_t config{};
    config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | (microphone ? I2S_MODE_RX | I2S_MODE_PDM : I2S_MODE_TX));
    config.sample_rate = microphone ? 16000 : 44100;
    config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    config.channel_format = microphone ? I2S_CHANNEL_FMT_ONLY_LEFT : I2S_CHANNEL_FMT_RIGHT_LEFT;
    config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    config.dma_buf_count = 4;
    config.dma_buf_len = 256;
    config.tx_desc_auto_clear = true;
    return config;
}
}

void Hardware::begin() {
    if (commands) return;
    digitalWrite(SpeakerEnable, LOW);
    pinMode(SpeakerEnable, OUTPUT);
    pinMode(BatteryAdc, INPUT);
    analogSetPinAttenuation(BatteryAdc, ADC_11db);
    pinMode(EncoderA, INPUT_PULLUP);
    pinMode(EncoderB, INPUT_PULLUP);
    pcnt_config_t enc{};
    enc.unit = PCNT_UNIT_0;
    enc.channel = PCNT_CHANNEL_0;
    enc.pulse_gpio_num = EncoderA;
    enc.ctrl_gpio_num = EncoderB;
    enc.pos_mode = PCNT_COUNT_INC;
    enc.neg_mode = PCNT_COUNT_DEC;
    enc.lctrl_mode = PCNT_MODE_REVERSE;
    enc.hctrl_mode = PCNT_MODE_KEEP;
    enc.counter_h_lim = 32767;
    enc.counter_l_lim = -32768;
    esp_err_t error = pcnt_unit_config(&enc);
    enc.channel = PCNT_CHANNEL_1;
    enc.pulse_gpio_num = EncoderB;
    enc.ctrl_gpio_num = EncoderA;
    enc.pos_mode = PCNT_COUNT_DEC;
    enc.neg_mode = PCNT_COUNT_INC;
    if (error == ESP_OK) error = pcnt_unit_config(&enc);
    if (error == ESP_OK) error = pcnt_set_filter_value(PCNT_UNIT_0, 1000);
    if (error == ESP_OK) error = pcnt_filter_enable(PCNT_UNIT_0);
    if (error == ESP_OK) error = pcnt_counter_clear(PCNT_UNIT_0);
    if (error == ESP_OK) error = pcnt_counter_resume(PCNT_UNIT_0);
    encoderPcntReady = error == ESP_OK;
    pinMode(EncoderA, INPUT_PULLUP);
    pinMode(EncoderB, INPUT_PULLUP);
    encoderPrevious = ((gpio_get_level(GPIO_NUM_2) != 0) << 1) | (gpio_get_level(GPIO_NUM_1) != 0);
    attachInterruptArg(EncoderA, encoderEdge, this, CHANGE);
    attachInterruptArg(EncoderB, encoderEdge, this, CHANGE);
    detentDecoder.reset(encoderPrevious);
    esp_timer_create_args_t timer{};
    timer.callback=sampleDetents;timer.arg=this;timer.dispatch_method=ESP_TIMER_TASK;
    timer.name="K718 encoder";timer.skip_unhandled_events=true;
    error=esp_timer_create(&timer,&encoderTimer);
    if(error==ESP_OK)error=esp_timer_start_periodic(encoderTimer,500);
    state.encoderReady=error==ESP_OK;
    commands = xQueueCreate(1, sizeof(HardwareTest));
    if (!commands || xTaskCreatePinnedToCore(worker, "K718::hardware", 8192, this, 1, nullptr, 0) != pdPASS) {
        if (commands) vQueueDelete(commands);
        commands = nullptr;
        return;
    }
    knobDiagnosticPrintf("K718 hardware: encoder GPIO2/1 %s; SYS ADC GPIO6; LED13 GPIO0; audio tests idle\n",
                         state.encoderReady ? "ready" : "failed");
}

HardwareState Hardware::snapshot() {
    portENTER_CRITICAL(&lock);
    HardwareState copy = state;
    portEXIT_CRITICAL(&lock);
    return copy;
}

bool Hardware::requestTest(HardwareTest test, uint32_t &jobId) {
    portENTER_CRITICAL(&lock);
    const bool accept = state.ready && !state.busy;
    if (accept) {
        state.busy = true;
        jobId = ++state.jobId;
        snprintf(state.result, sizeof(state.result), "Running");
    }
    portEXIT_CRITICAL(&lock);
    if (!accept) return false;
    if (xQueueSend(commands, &test, 0) == pdTRUE) return true;
    portENTER_CRITICAL(&lock);
    state.busy = false;
    portEXIT_CRITICAL(&lock);
    return false;
}

bool Hardware::configureLeds(const LedConfig &config) {
    if (!config.count || config.count > RingLedCount || config.topIndex >= config.count || config.white > 0xffffff || config.mode > LedMode::White) return false;
    portENTER_CRITICAL(&lock);
    const bool accept = state.ready && !state.busy;
    if (accept) state.ledConfig = config;
    portEXIT_CRITICAL(&lock);
    return accept;
}

void Hardware::applyLedPreferences(const LedPreferences &value, bool force) {
    portENTER_CRITICAL(&lock);
    ledPreferenceApplier.update(value, [this](const LedConfig &config) {
        if (!state.ready || state.busy) return false;
        state.ledConfig = config;
        return true;
    }, force);
    portEXIT_CRITICAL(&lock);
}

void Hardware::publishLedGauges(const GaugeFrame &frame) {
    const uint32_t now = millis();
    portENTER_CRITICAL(&lock);
    ledGauges = frame;
    ledPublishedMs = now;
    state.ledUiActive = frame.active;
    state.ledGaugeCount = frame.count;
    portEXIT_CRITICAL(&lock);
}

void Hardware::worker(void *arg) { static_cast<Hardware *>(arg)->run(); }
void Hardware::run() {
    sampleRail();
    const int initialLedError = openLeds();
    closeLeds();
    portENTER_CRITICAL(&lock);
    state.ledError = initialLedError;
    state.ready = true;
    portEXIT_CRITICAL(&lock);
    uint32_t lastBattery = millis();
    for (;;) {
        HardwareTest test;
        if (xQueueReceive(commands, &test, pdMS_TO_TICKS(25)) == pdTRUE) {
            closeLeds(); // pause live mirroring while an explicit hardware test runs
            const int error = test == HardwareTest::Led ? testLed() : test == HardwareTest::Speaker ? testSpeaker() : testMicrophone();
            portENTER_CRITICAL(&lock);
            state.lastError = error;
            state.completedJobId = state.jobId;
            state.busy = false;
            snprintf(state.result, sizeof(state.result), "%s: %s", test == HardwareTest::Led ? "LED" : test == HardwareTest::Speaker ? "Speaker" : "Microphone", esp_err_to_name(error));
            portEXIT_CRITICAL(&lock);
            knobDiagnosticPrintf("K718 hardware test: %s\n", snapshot().result);
        }
        sampleEncoder();
        serviceLeds();
        if (millis() - lastBattery >= 2000) {
            lastBattery = millis();
            sampleRail();
        }
    }
}

void Hardware::sampleRail() {
    // Read-only SOF counter: a running USB host is positive external-power
    // evidence even if SYS droops. No CDC reader or open terminal is required.
    const auto usbFrame = REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & USB_SERIAL_JTAG_SOF_FRAME_INDEX;
    std::array<uint16_t, 16> readings;
    for (auto &reading : readings) reading = analogReadMilliVolts(BatteryAdc);
    const uint16_t rail = trimmedRailMilliVolts(readings);
    vTaskDelay(pdMS_TO_TICKS(5));
    const bool usbHost = usbFrame != (REG_READ(USB_SERIAL_JTAG_FRAM_NUM_REG) & USB_SERIAL_JTAG_SOF_FRAME_INDEX);
    const auto old = snapshot();
    const auto source = estimatedPowerSource(rail, old.powerSource, usbHost);
    const int percentage = batteryEstimate.update(source, rail, millis());
    portENTER_CRITICAL(&lock);
    state.railMv = rail;
    state.batteryFilteredMv = batteryEstimate.filteredMilliVolts();
    state.powerSource = source;
    state.usbHostActive = usbHost;
    state.batteryPercent = percentage;
    portEXIT_CRITICAL(&lock);
    if (!old.railMv || old.powerSource != source)
        knobDiagnosticPrintf("K718 power: SYS=%umV source=%s usbHost=%d estimatedBattery=%d%%\n", rail,
            source == PowerSource::External ? "external" : source == PowerSource::Battery ? "battery" : "unknown", usbHost, percentage);
}

void Hardware::sampleEncoder() {
    if (!encoderPcntReady) return;
    int16_t count;
    if (pcnt_get_counter_value(PCNT_UNIT_0, &count) != ESP_OK) return;
    portENTER_CRITICAL(&lock);
    state.encoderPcntTicks = count;
    state.encoderPinA = gpio_get_level(GPIO_NUM_2);
    state.encoderPinB = gpio_get_level(GPIO_NUM_1);
    portEXIT_CRITICAL(&lock);
}

void Hardware::sampleDetents(void *arg) {
    auto *self=static_cast<Hardware *>(arg);
    const uint32_t pins=GPIO.in;
    const uint8_t phase=(((pins>>EncoderA)&1)<<1)|((pins>>EncoderB)&1);
    portENTER_CRITICAL(&self->lock);
    const int step=self->detentDecoder.sample(phase);
    self->state.encoderDetents=int32_t(uint32_t(self->state.encoderDetents)+uint32_t(step));
    ++self->state.encoderSamples;
    if(step>0)++self->state.encoderRightSteps;
    if(step<0)++self->state.encoderLeftSteps;
    self->state.encoderAmbiguousPulses=self->detentDecoder.ambiguousPulses();
    self->state.encoderShortPulses=self->detentDecoder.rejectedShortPulses();
    self->state.encoderStablePhase=self->detentDecoder.phase();
    portEXIT_CRITICAL(&self->lock);
}

void IRAM_ATTR Hardware::encoderEdge(void *arg) {
    auto *self = static_cast<Hardware *>(arg);
    const uint32_t pins = GPIO.in;
    const uint8_t next = (((pins >> EncoderA) & 1) << 1) | ((pins >> EncoderB) & 1);
    portENTER_CRITICAL_ISR(&self->lock);
    const uint8_t previous = self->encoderPrevious;
    self->encoderPrevious = next;
    if ((previous ^ next) & 2) ++self->state.encoderEdgesA;
    if ((previous ^ next) & 1) ++self->state.encoderEdgesB;
    self->state.encoderTicks += EncoderTransitions[(previous << 2) | next];
    portEXIT_CRITICAL_ISR(&self->lock);
}

int Hardware::openLeds() {
    if (ledInstalled) return ESP_OK;
    // The LCD owns SPI2. Reserve only unused SPI3; fail without touching an
    // already-owned bus. DMA clocks the complete WS2812 frame without RMT ISR refills.
    spi_bus_config_t bus{};
    bus.mosi_io_num=LedPin;bus.miso_io_num=bus.sclk_io_num=-1;
    bus.quadwp_io_num=bus.quadhd_io_num=-1;
    bus.data4_io_num=bus.data5_io_num=bus.data6_io_num=bus.data7_io_num=-1;
    bus.max_transfer_sz=LedDmaBytes;
    bus.flags=SPICOMMON_BUSFLAG_MASTER|SPICOMMON_BUSFLAG_MOSI;
    esp_err_t error=spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO);
    if(error!=ESP_OK)return error;
    ledDma=static_cast<uint8_t *>(heap_caps_malloc(LedDmaBytes,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL));
    if(!ledDma){spi_bus_free(SPI3_HOST);return ESP_ERR_NO_MEM;}
    spi_device_interface_config_t device{};
    device.clock_speed_hz=2500000;device.mode=0;device.spics_io_num=-1;device.queue_size=1;
    error=spi_bus_add_device(SPI3_HOST,&device,&ledSpi);
    if(error!=ESP_OK){heap_caps_free(ledDma);ledDma=nullptr;spi_bus_free(SPI3_HOST);return error;}
    ledInstalled=true;ledFrameSent=false;
    portENTER_CRITICAL(&lock);
    state.ledClockHz=spi_get_actual_clock(80000000,2500000,128);
    portEXIT_CRITICAL(&lock);
    return ESP_OK;
}

int Hardware::writeLeds(const LedFrame &frame) {
    if(!ledInstalled)return ESP_ERR_INVALID_STATE;
    const auto encoded=encodeLedSpi(frame);
    memcpy(ledDma,encoded.data(),encoded.size());
    spi_transaction_t transaction{};
    transaction.length=LedDmaBytes*8;transaction.tx_buffer=ledDma;
    const esp_err_t error=spi_device_transmit(ledSpi,&transaction);
    // Final encoded bits are zero; keep the line low after each transfer.
    vTaskDelay(pdMS_TO_TICKS(2));
    portENTER_CRITICAL(&lock);
    state.ledError=error;
    if(error==ESP_OK){state.ledFrame=frame;state.ledUpdatedMs=millis();}
    portEXIT_CRITICAL(&lock);
    ledFrameSent=error==ESP_OK;
    return error;
}

void Hardware::closeLeds() {
    if(!ledInstalled)return;
    writeLeds(LedFrame{});
    spi_bus_remove_device(ledSpi);ledSpi=nullptr;
    spi_bus_free(SPI3_HOST);
    heap_caps_free(ledDma);ledDma=nullptr;
    ledInstalled=false;ledFrameSent=false;
    pinMode(LedPin,OUTPUT);digitalWrite(LedPin,LOW);
}

void Hardware::serviceLeds() {
    const uint32_t now = millis();
    if (now - lastLedService < 100) return; // at most10Hz; hardware task only
    lastLedService = now;
    GaugeFrame gauges;
    LedConfig config;
    uint32_t published;
    portENTER_CRITICAL(&lock);
    gauges = ledGauges; config = state.ledConfig; published = ledPublishedMs;
    portEXIT_CRITICAL(&lock);
    const bool fresh = now - published <= 1000;
    const auto frame = renderGaugeLeds(gauges, config, fresh);
    if (config.mode == LedMode::Off || (config.mode == LedMode::Ui && (!fresh || !gauges.active))) {
        closeLeds(); return;
    }
    const int error = openLeds();
    if (error != ESP_OK) {
        portENTER_CRITICAL(&lock); state.ledError = error; portEXIT_CRITICAL(&lock);
        return;
    }
    if (!ledFrameSent || frame != snapshot().ledFrame) {
        if (writeLeds(frame) != ESP_OK) closeLeds();
    }
}

int Hardware::testLed() {
    int error = openLeds();
    if (error != ESP_OK) return error;
    for (int color = 0; color < 3 && error == ESP_OK; ++color) {
        LedFrame frame;
        frame.fill(color == 0 ? 0x400000 : color == 1 ? 0x004000 : 0x000040);
        error = writeLeds(frame);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    closeLeds();
    return error;
}

int Hardware::testSpeaker() {
    auto config = audioConfig(false);
    esp_err_t error = i2s_driver_install(SpeakerPort, &config, 0, nullptr);
    if (error != ESP_OK) return error;
    i2s_pin_config_t pins{};
    pins.mck_io_num = I2S_PIN_NO_CHANGE; pins.bck_io_num = SpeakerBck; pins.ws_io_num = SpeakerWs;
    pins.data_out_num = SpeakerData; pins.data_in_num = I2S_PIN_NO_CHANGE;
    error = i2s_set_pin(SpeakerPort, &pins);
    if (error == ESP_OK) error = i2s_zero_dma_buffer(SpeakerPort);
    if (error == ESP_OK) {
        digitalWrite(SpeakerEnable, HIGH);
        vTaskDelay(pdMS_TO_TICKS(20));
        int16_t samples[256 * 2];
        constexpr int frames = 17640; //400ms,880Hz, about2% full scale with ramps
        for (int start = 0; start < frames && error == ESP_OK; start += 256) {
            const int count = std::min(256, frames - start);
            for (int i = 0; i < count; ++i) {
                int n = start + i;
                float ramp = std::min(1.0f, std::min(n, frames - 1 - n) / 882.0f);
                samples[2 * i] = samples[2 * i + 1] = static_cast<int16_t>(700 * ramp * std::sin(n * (2.0 * 3.141592653589793 * 880 / 44100)));
            }
            size_t written = 0;
            error = i2s_write(SpeakerPort, samples, count * 4, &written, pdMS_TO_TICKS(100));
            if (error == ESP_OK && written != size_t(count * 4)) error = ESP_ERR_TIMEOUT;
        }
        vTaskDelay(pdMS_TO_TICKS(35)); // drain at most4x256 frames before muting
    }
    digitalWrite(SpeakerEnable, LOW);
    i2s_zero_dma_buffer(SpeakerPort);
    i2s_driver_uninstall(SpeakerPort);
    return error;
}

int Hardware::testMicrophone() {
    portENTER_CRITICAL(&lock);
    state.micSamples = state.micNonzero = 0;
    state.micRms = 0; state.micPeakToPeak = 0;
    portEXIT_CRITICAL(&lock);
    auto config = audioConfig(true);
    esp_err_t error = i2s_driver_install(MicPort, &config, 0, nullptr);
    if (error != ESP_OK) return error;
    i2s_pin_config_t pins{};
    pins.mck_io_num = I2S_PIN_NO_CHANGE; pins.bck_io_num = I2S_PIN_NO_CHANGE;
    pins.ws_io_num = MicClock; pins.data_out_num = I2S_PIN_NO_CHANGE; pins.data_in_num = MicData;
    error = i2s_set_pin(MicPort, &pins);
    if (error == ESP_OK) error = i2s_set_pdm_rx_down_sample(MicPort, I2S_PDM_DSR_8S);
    int16_t samples[256];
    uint32_t count = 0, nonzero = 0;
    int64_t sum = 0, squares = 0;
    int minimum = 32767, maximum = -32768;
    const uint32_t started = millis();
    for (int block = 0; block < 133 && error == ESP_OK; ++block) {
        if (millis() - started > 3500) { error = ESP_ERR_TIMEOUT; break; }
        size_t bytes = 0;
        error = i2s_read(MicPort, samples, sizeof(samples), &bytes, pdMS_TO_TICKS(100));
        if (error == ESP_OK && bytes == 0) error = ESP_ERR_TIMEOUT;
        if (block < 8) continue; // let microphone/filter settle128ms
        for (size_t i = 0; i < bytes / sizeof(int16_t); ++i) {
            const int value = samples[i];
            ++count; nonzero += value != 0; sum += value; squares += int64_t(value) * value;
            minimum = std::min(minimum, value); maximum = std::max(maximum, value);
        }
    }
    i2s_driver_uninstall(MicPort);
    const double mean = count ? double(sum) / count : 0;
    const float rms = count ? std::sqrt(std::max(0.0, double(squares) / count - mean * mean)) : 0;
    portENTER_CRITICAL(&lock);
    state.micSamples = count; state.micNonzero = nonzero;
    state.micRms = rms; state.micPeakToPeak = count ? maximum - minimum : 0;
    portEXIT_CRITICAL(&lock);
    knobDiagnosticPrintf("K718 microphone: samples=%u nonzero=%u rms=%.1f peakToPeak=%d\n", count, nonzero, rms, count ? maximum - minimum : 0);
    if (error == ESP_OK && (!count || !nonzero || maximum == minimum)) error = ESP_ERR_INVALID_RESPONSE;
    return error;
}
} // namespace guition_knob
#endif
