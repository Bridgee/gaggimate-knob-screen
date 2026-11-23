// Host codec/queue checks, linked to the exact generated firmware schema.
#include "Protocol.h"
#include "CoalescingPriorityQueue.h"
#include <pb_decode.h>
#include <pb_encode.h>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

static std::vector<uint8_t> encode(const gm::Frame &frame) {
    std::array<uint8_t, 256> bytes{}; // Actual Endpoint frame-buffer bound.
    auto stream = pb_ostream_from_buffer(bytes.data(), bytes.size());
    assert(pb_encode(&stream, gaggimate_Frame_fields, &frame));
    return {bytes.begin(), bytes.begin() + stream.bytes_written};
}
static bool decode(const std::vector<uint8_t> &bytes, gm::Frame &frame) {
    frame = gm::Frame{};
    auto stream = pb_istream_from_buffer(bytes.data(), bytes.size());
    return pb_decode(&stream, gaggimate_Frame_fields, &frame);
}
static gm::Frame roundtrip(const gm::Frame &frame) {
    const auto bytes = encode(frame);
    gm::Frame received{};
    assert(decode(bytes, received));
    assert(encode(received) == bytes);
    assert(received.id == frame.id && received.ack == frame.ack);
    assert(received.payloads_count == frame.payloads_count);
    return received;
}
static gm::Payload message(pb_size_t tag) {
    gm::Payload p{};
    p.which_content = tag;
    return p;
}
int main() {
    static_assert(gm_proto::PROTOCOL_VERSION == REVIEWED_PROTOCOL);
    gm::Frame frame{};
    frame.id = 1;
    frame.payloads_count = 1;
    auto &p = frame.payloads[0];
    p = message(gaggimate_Payload_system_info_tag);
    auto &info = p.content.system_info;
    std::strcpy(info.hardware, "GaggiMate test controller");
    std::strcpy(info.version, "v1.10.0");
    info.protocol_version = gm_proto::PROTOCOL_VERSION;
    info.has_capabilities = true;
    info.capabilities.pressure = true;
    info.capabilities.dual_boiler = true;
    info.capabilities.addons_count = 2;
    info.capabilities.addons[0].type = 1;
    info.capabilities.addons[1].type = 2;
    auto peer = roundtrip(frame).payloads[0].content.system_info;
    assert(peer.protocol_version == gm_proto::PROTOCOL_VERSION);
    assert(peer.capabilities.dual_boiler && peer.capabilities.pressure);
    assert(peer.capabilities.addons_count == 2 && peer.capabilities.addons[1].type == 2);
    assert(std::strcmp(peer.version, info.version) == 0);

    // Dynamic telemetry, indexed boilers, optional level flags and fractional weight.
    frame.id = 2;
    frame.payloads_count = 2;
    p = message(gaggimate_Payload_sensor_tag);
    auto &sensor = p.content.sensor;
    sensor.boilers_count = 2;
    sensor.boilers[0] = {0, 25.5f, 0.04f, 0.0f};
    sensor.boilers[1] = {1, 125.5f, 1.5f, 34.0f};
    sensor.puck_flow = 2.5f;
    sensor.water_pumped = 36.5f;
    sensor.has_tank_level = true;
    sensor.tank_level_ok = true;
    sensor.has_steam_boiler_low = true;
    sensor.steam_boiler_low = false;
    frame.payloads[1] = message(gaggimate_Payload_scale_tag);
    frame.payloads[1].content.scale = {18.5f, 9.0f, 9.5f, true, true};
    auto received = roundtrip(frame);
    assert(received.payloads[0].which_content == gaggimate_Payload_sensor_tag);
    const auto &s = received.payloads[0].content.sensor;
    assert(s.boilers_count == 2 && s.boilers[1].index == 1);
    assert(s.boilers[0].temperature == 25.5f && s.boilers[0].pressure == 0.04f);
    assert(s.boilers[1].temperature == 125.5f && s.boilers[1].power == 34.0f);
    assert(s.has_tank_level && s.tank_level_ok && s.has_steam_boiler_low && !s.steam_boiler_low);
    assert(s.water_pumped == 36.5f && received.payloads[1].content.scale.weight == 18.5f);

    // Commands are encoded into memory only; no actuator or device is involved.
    frame = gm::Frame{};
    frame.id = 3;
    frame.payloads_count = 6;
    frame.payloads[0] = message(gaggimate_Payload_heater_tag);
    frame.payloads[0].content.heater = {1.5f, 0.2f, 0.3f, 0.4f, true, 200, 2.5f, 0.5f, 0.6f};
    frame.payloads[1] = message(gaggimate_Payload_boiler_tag);
    frame.payloads[1].content.boiler = {1, gaggimate_BoilerMode_BOILER_MODE_PRESSURE, 1.5f};
    frame.payloads[2] = message(gaggimate_Payload_pump_tag);
    frame.payloads[2].content.pump = {0, gaggimate_PumpMode_PUMP_MODE_FLOW, 0, 9, 2.5f};
    frame.payloads[3] = message(gaggimate_Payload_relay_tag);
    frame.payloads[3].content.relay = {1, true};
    frame.payloads[4] = message(gaggimate_Payload_tare_tag);
    frame.payloads[5] = message(gaggimate_Payload_hardware_scale_tare_tag);
    received = roundtrip(frame);
    assert(received.payloads[0].content.heater.handover_ms == 200);
    assert(received.payloads[0].content.heater.steam_kp == 2.5f);
    assert(received.payloads[1].content.boiler.mode == gaggimate_BoilerMode_BOILER_MODE_PRESSURE);
    assert(received.payloads[2].content.pump.flow == 2.5f);
    assert(received.payloads[3].content.relay.open);
    assert(received.payloads[4].which_content != received.payloads[5].which_content);

    // ACK-only, forward-compatible unknown fields, malformed and bounded inputs.
    frame = gm::Frame{};
    frame.ack = 300;
    auto bytes = encode(frame);
    auto extended = bytes;
    extended.insert(extended.end(), {0xf8, 0x07, 0x01}); // Unknown field 127.
    assert(decode(extended, received) && received.ack == 300 && received.payloads_count == 0);
    bytes.pop_back();
    assert(!decode(bytes, received));
    assert(!decode({0x1a, 0xff, 0xff, 0xff, 0x7f}, received));
    frame.id = 1;
    frame.ack = 0;
    frame.payloads_count = 7; // Schema capacity is six; reject before indexing.
    std::array<uint8_t, 256> buffer{};
    auto stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    assert(!pb_encode(&stream, gaggimate_Frame_fields, &frame));
    frame.payloads_count = 1;
    frame.payloads[0] = message(gaggimate_Payload_sensor_tag);
    frame.payloads[0].content.sensor.boilers_count = 5; // Capacity four.
    stream = pb_ostream_from_buffer(buffer.data(), buffer.size());
    assert(!pb_encode(&stream, gaggimate_Frame_fields, &frame));

    // Actual coalescing queue must retain separate boiler indices and tare families.
    CoalescingPrioQueue<16, uint16_t, gm::Payload, 256> queue;
    auto enqueue = [&](const gm::Payload &value) {
        assert(queue.upsert(gm_proto::coalescingKey(value), gm_proto::defaultPriority(value.which_content), value));
    };
    auto boiler = message(gaggimate_Payload_boiler_tag);
    boiler.content.boiler = {0, gaggimate_BoilerMode_BOILER_MODE_TEMPERATURE, 90};
    enqueue(boiler);
    boiler.content.boiler.setpoint = 93;
    enqueue(boiler);
    boiler.content.boiler.index = 1;
    boiler.content.boiler.setpoint = 125;
    enqueue(boiler);
    enqueue(message(gaggimate_Payload_tare_tag));
    enqueue(message(gaggimate_Payload_hardware_scale_tare_tag));
    enqueue(message(gaggimate_Payload_sensor_tag));
    enqueue(message(gaggimate_Payload_error_tag));
    assert(queue.size() == 6);
    assert(queue.pop()->payload.which_content == gaggimate_Payload_error_tag);
    auto steam = queue.pop()->payload;
    auto brew = queue.pop()->payload;
    assert(steam.content.boiler.index == 1 && steam.content.boiler.setpoint == 125);
    assert(brew.content.boiler.index == 0 && brew.content.boiler.setpoint == 93);
    assert(queue.pop()->payload.which_content == gaggimate_Payload_hardware_scale_tare_tag);
    assert(queue.pop()->payload.which_content == gaggimate_Payload_tare_tag);
    assert(queue.pop()->payload.which_content == gaggimate_Payload_sensor_tag);
    assert(queue.empty());
    std::puts("PASS: firmware nanopb handshake/telemetry/commands, bounds/malformed inputs and indexed queue; no BLE/RTOS/device qualification.");
}
