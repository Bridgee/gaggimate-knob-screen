#include <display/validation/ReplayPeer.h>
#include <Protocol.h>
#include <MachineHardwareSettings.h>
#include <pb_decode.h>
#include <pb_encode.h>
#include <cassert>
#include <cmath>
#include <vector>
#include <thread>
#include <iostream>
using namespace knob_validation;
std::vector<uint8_t> encode(const gm::Frame &f) {
    std::vector<uint8_t> b(512);auto s=pb_ostream_from_buffer(b.data(),b.size());
    assert(pb_encode(&s,gaggimate_Frame_fields,&f));b.resize(s.bytes_written);return b;
}
int main() {
 ReplayPeer peer;bool linked=false;unsigned identities=0,sensors=0,ack=0;float pressure=0,temp=0;bool hw=false;
 peer.onConnectionChange([&](bool v){linked=v;});
 peer.onData([&](const uint8_t *b,size_t n){
    gm::Frame f=gaggimate_Frame_init_zero;auto s=pb_istream_from_buffer(b,n);assert(pb_decode(&s,gaggimate_Frame_fields,&f));
    if(f.ack==42)++ack;
    for(pb_size_t i=0;i<f.payloads_count;++i){const auto &p=f.payloads[i];
      if(p.which_content==gaggimate_Payload_system_info_tag){++identities;assert(p.content.system_info.protocol_version==16);assert(std::string(p.content.system_info.version)=="simulated");assert(p.content.system_info.has_capabilities);assert(p.content.system_info.capabilities.pressure && p.content.system_info.capabilities.dimming);assert(p.content.system_info.capabilities.addons_count==1 && p.content.system_info.capabilities.addons[0].type==8);}
      if(p.which_content==gaggimate_Payload_sensor_tag){++sensors;pressure=p.content.sensor.boilers[0].pressure;temp=p.content.sensor.boilers[0].temperature;}
      if(p.which_content==gaggimate_Payload_machine_hardware_status_tag){hw=p.content.machine_hardware_status.ready;}
    }
    // Snapshot must remain reentrant: emission cannot hold the peer mutex.
    (void)peer.snapshot();
 });
 assert(peer.quiescent());assert(!peer.send(nullptr,0));
 assert(!peer.request(static_cast<Action>(99)));assert(!peer.request(Action::Play,static_cast<Trace>(99)));
 assert(peer.request(Action::Play,Trace::Shot));assert(!peer.quiescent());assert(!peer.request(Action::Pause));
 peer.tick(0);assert(linked && identities==1 && sensors==1 && temp==93);
 for(unsigned t=100;t<=8000;t+=100)peer.tick(t);
 assert(std::fabs(pressure-9)<0.01f);
 gm::Frame command=gaggimate_Frame_init_zero;command.id=42;command.payloads_count=1;
 command.payloads[0].which_content=gaggimate_Payload_boiler_tag;
 command.payloads[0].content.boiler={0,gaggimate_BoilerMode_BOILER_MODE_TEMPERATURE,123};
 auto bytes=encode(command);assert(peer.send(bytes.data(),bytes.size()));assert(peer.send(bytes.data(),bytes.size()));
 assert(peer.snapshot().commands==1);peer.tick(8100);assert(ack==2); // ACK duplicates without reapplying commands
 assert(!peer.send(nullptr,2));assert(!peer.send(bytes.data(),513));
 const uint8_t bad[]={0x1a,0xff,0xff,0xff,0xff,0x7f};assert(!peer.send(bad,sizeof(bad)));
 assert(peer.snapshot().malformed==3);
 command.id=43;command.payloads[0].which_content=gaggimate_Payload_machine_hardware_tag;
 command.payloads[0].content.machine_hardware=defaultMachineHardware();bytes=encode(command);
 assert(peer.send(bytes.data(),bytes.size()));peer.tick(8200);assert(hw);
 assert(peer.request(Action::Pause));peer.tick(8300);auto paused=peer.snapshot();peer.tick(9900);assert(peer.snapshot().positionMs==paused.positionMs);
 assert(peer.request(Action::Reset));peer.tick(10000);assert(peer.snapshot().positionMs==0);
 assert(peer.request(Action::Disconnect));peer.tick(10100);assert(!linked && peer.quiescent());assert(!peer.send(bytes.data(),bytes.size()));
 unsigned emitted=peer.snapshot().emitted;peer.tick(15000);assert(peer.snapshot().emitted==emitted);
 assert(peer.request(Action::Play,Trace::Warmup));peer.tick(20000);
 for(unsigned t=20100;t<=36000;t+=100)peer.tick(t);assert(std::fabs(peer.snapshot().temperature-93)<0.01f);
 // Concurrent producer and tick have bounded queues and no data race in the peer.
 std::thread producer([&]{for(unsigned i=0;i<200;++i)(void)peer.send(bytes.data(),bytes.size());});
 for(unsigned t=36100;t<40000;t+=10)peer.tick(t);producer.join();
 assert(peer.request(Action::Disconnect));peer.tick(40100);assert(peer.quiescent());
 std::cout<<"PASS: real nanopb replay, traces, pause/reset/disconnect, ACK/dedup, captured commands, malformed bounds and concurrent producer; no physical endpoint\n";
}
