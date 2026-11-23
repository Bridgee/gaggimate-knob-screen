#pragma once
#ifndef GAGGIMATE_KNOB_VALIDATION
#error ReplayPeer belongs only to display-knob-validation
#endif
#include <Transport.h>
#include <Messages.h>
#include <array>
#include <atomic>
#include <mutex>

namespace knob_validation {
enum class Action { Play, Pause, Reset, Disconnect };
enum class Trace { Idle, Warmup, Shot };
struct ReplayStatus {
    bool connected = false, playing = false;
    Trace trace = Trace::Idle;
    uint32_t positionMs = 0, commands = 0, malformed = 0, emitted = 0;
    float temperature = 25, pressure = 0, weight = 0;
};
// No network/serial output exists in this transport. Commands terminate here.
class ReplayPeer final : public Transport {
  public:
    bool request(Action, Trace = Trace::Idle);
    void tick(uint32_t now);
    ReplayStatus snapshot() const;
    bool quiescent() const;
    bool send(const uint8_t *, size_t) override;
    bool isConnected() const override { return connected.load(); }
    size_t maxDatagram() const override { return 512; }
  private:
    void deliver(const gm::Frame &);
    void identity();
    mutable std::mutex mutex;
    std::atomic<bool> connected{false};
    ReplayStatus status;
    Action action = Action::Disconnect;
    Trace requestedTrace = Trace::Idle;
    bool pending = false;
    uint32_t lastTick = 0, lastSensor = 0, lastIdentity = 0, lastCommandId = 0;
    std::array<uint32_t, 32> acknowledgements{};
    size_t ackCount = 0;
    bool hardwarePending = false, hardwareReady = false;
    uint32_t hardwareHash = 0;
};
ReplayPeer &peer();
const char *traceName(Trace);
} // namespace knob_validation
