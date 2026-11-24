#ifdef GAGGIMATE_KNOB_VALIDATION
#include "ReplayPeer.h"
#include <Protocol.h>
#include <MachineHardwareSettings.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <algorithm>
#include <cstring>

namespace knob_validation {
namespace {
struct Point { uint32_t ms; float temperature, pressure, weight; };
constexpr Point warmup[] = {{0,25,0,0},{4000,50,0,0},{10000,80,0,0},{16000,93,0,0},{24000,93,0,0}};
constexpr Point shot[] = {{0,93,0,0},{3000,93,3,2},{8000,92,9,8},{18000,92,9,25},{25000,93,6,36},{28000,93,0,36}};
Point sample(Trace trace, uint32_t ms) {
    if (trace == Trace::Idle) return {ms,25,0,0};
    const auto *points = trace == Trace::Warmup ? warmup : shot;
    const size_t count = trace == Trace::Warmup ? std::size(warmup) : std::size(shot);
    if (ms >= points[count-1].ms) return points[count-1];
    for (size_t i=1;i<count;++i) if (ms <= points[i].ms) {
        const auto &a=points[i-1], &b=points[i];
        const float f=float(ms-a.ms)/float(b.ms-a.ms);
        return {ms,a.temperature+(b.temperature-a.temperature)*f,a.pressure+(b.pressure-a.pressure)*f,a.weight+(b.weight-a.weight)*f};
    }
    return points[0];
}
}
ReplayPeer &peer() { static ReplayPeer value; return value; }
const char *traceName(Trace t) { return t==Trace::Shot ? "shot" : t==Trace::Warmup ? "warmup" : "idle"; }
bool ReplayPeer::request(Action value, Trace trace) {
    if ((value != Action::Play && value != Action::Pause && value != Action::Reset && value != Action::Disconnect) ||
        (trace != Trace::Idle && trace != Trace::Warmup && trace != Trace::Shot)) return false;
    std::lock_guard<std::mutex> guard(mutex);
    if (pending) return false;
    pending=true; action=value; requestedTrace=trace; return true;
}
ReplayStatus ReplayPeer::snapshot() const { std::lock_guard<std::mutex> guard(mutex); return status; }
bool ReplayPeer::quiescent() const { std::lock_guard<std::mutex> guard(mutex); return !pending && !status.connected; }
bool ReplayPeer::send(const uint8_t *data, size_t bytes) {
    std::lock_guard<std::mutex> guard(mutex);
    if (!connected.load()) return false;
    gm::Frame frame = gaggimate_Frame_init_zero;
    auto input=pb_istream_from_buffer(data,bytes);
    if (!data || !bytes || bytes>maxDatagram() || !pb_decode(&input,gaggimate_Frame_fields,&frame)) {
        ++status.malformed; return false;
    }
    if (frame.id) {
        if (ackCount==acknowledgements.size()) return false;
        acknowledgements[ackCount++]=frame.id;
        if (frame.id==lastCommandId) return true;
        lastCommandId=frame.id;
    }
    for (pb_size_t i=0;i<frame.payloads_count;++i) {
        ++status.commands;
        const auto &p=frame.payloads[i];
        if (p.which_content==gaggimate_Payload_machine_hardware_tag) {
            hardwarePending=true; hardwareHash=machineHardwareHash(p.content.machine_hardware);
            hardwareReady=validMachineHardware(p.content.machine_hardware);
        }
    }
    return true;
}
void ReplayPeer::deliver(const gm::Frame &frame) {
    uint8_t bytes[512]; auto output=pb_ostream_from_buffer(bytes,sizeof(bytes));
    if (pb_encode(&output,gaggimate_Frame_fields,&frame)) {
        emitData(bytes,output.bytes_written);
        std::lock_guard<std::mutex> guard(mutex); ++status.emitted;
    }
}
void ReplayPeer::identity() {
    gm::Frame f=gaggimate_Frame_init_zero; f.payloads_count=1;
    auto &p=f.payloads[0];p.which_content=gaggimate_Payload_system_info_tag;
    auto &info=p.content.system_info;
    std::strcpy(info.hardware,"SIMULATED - no controller"); std::strcpy(info.version,"simulated");
    info.protocol_version=gm_proto::PROTOCOL_VERSION; info.has_capabilities=true;
    info.capabilities.pressure=true; info.capabilities.dimming=true;
    info.capabilities.addons_count=1; info.capabilities.addons[0].type=8;
    deliver(f);
}
void ReplayPeer::tick(uint32_t now) {
    bool notify=false, state=false;
    std::array<uint32_t,32> acks{}; size_t count=0;
    bool hw=false, ready=false; uint32_t hash=0;
    ReplayStatus current;
    {
        std::lock_guard<std::mutex> guard(mutex);
        if (pending) {
            pending=false;
            if (action==Action::Disconnect) {
                notify=status.connected; status.connected=status.playing=false;
                connected=false;ackCount=0;lastCommandId=0;hardwarePending=false;
            } else if (action==Action::Play) {
                notify=!status.connected; status.connected=status.playing=true;
                if (status.trace!=requestedTrace || notify) status.positionMs=0;
                status.trace=requestedTrace; connected=true;
            } else if (action==Action::Pause) status.playing=false;
            else status.positionMs=0;
            lastTick=now;
        }
        state=status.connected;
        if (state && status.playing) status.positionMs=std::min(uint32_t(60000),status.positionMs+std::min(uint32_t(now-lastTick),uint32_t(1000)));
        lastTick=now;
        const auto point=sample(status.trace,status.positionMs);
        status.temperature=point.temperature;status.pressure=point.pressure;status.weight=point.weight;
        current=status;
        count=ackCount;std::copy_n(acknowledgements.begin(),count,acks.begin());ackCount=0;
        hw=hardwarePending;hash=hardwareHash;ready=hardwareReady;hardwarePending=false;
    }
    // All callbacks happen on the tick task without the peer mutex held.
    if (notify) { emitConnection(state); lastIdentity=now-2000; lastSensor=now-100; }
    if (!state) return;
    for (size_t i=0;i<count;++i) {gm::Frame f=gaggimate_Frame_init_zero;f.ack=acks[i];deliver(f);}
    if (uint32_t(now-lastIdentity)>=2000) {identity();lastIdentity=now;}
    if (hw) {
        gm::Frame f=gaggimate_Frame_init_zero;f.payloads_count=1;
        f.payloads[0].which_content=gaggimate_Payload_machine_hardware_status_tag;
        f.payloads[0].content.machine_hardware_status={hash,ready,!ready};deliver(f);
    }
    if (uint32_t(now-lastSensor)<100) return;
    lastSensor=now;
    gm::Frame f=gaggimate_Frame_init_zero;f.payloads_count=2;
    auto &p=f.payloads[0];p.which_content=gaggimate_Payload_sensor_tag;
    auto &s=p.content.sensor;s.boilers_count=1;s.boilers[0].temperature=current.temperature;s.boilers[0].pressure=current.pressure;
    auto &scale=f.payloads[1];scale.which_content=gaggimate_Payload_scale_tag;
    scale.content.scale.weight=current.weight;scale.content.scale.cell1_weight=current.weight;scale.content.scale.cell1_valid=true;
    deliver(f);
}
} // namespace knob_validation
#endif
