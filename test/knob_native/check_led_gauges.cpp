#include "display/drivers/GuitionKnob/LedGaugePolicy.h"
#include <cassert>
#include <cstdio>
using namespace guition_knob;
int main() {
    LedConfig config; config.mode = LedMode::Ui; config.brightness = 64; config.count = 13;
    GaugeFrame gauges; gauges.active = true; gauges.count = 2;
    gauges.arcs[0] = {0,160,0,80,120,120,0xff0000};
    gauges.arcs[1] = {160,0,0,80,300,120,0x0000ff};
    const auto half = renderGaugeLeds(gauges,config,true);
    assert(half[8] == 0x400000 && half[9] == 0x400000 && half[10] == 0x404040 && half[11] == 0x404040);
    assert(half[4] == 0x000040 && half[5] == 0x000040 && half[2] == 0x404040 && half[3] == 0x404040);
    for (unsigned i : {0u,1u,6u,7u,12u}) assert(half[i] == 0); // no UI gauge in these angular gaps
    for (int value=0; value<=160; ++value) {
        gauges.arcs[0].end = gauges.arcs[1].end = value;
        auto frame = renderGaugeLeds(gauges,config,true);
        for (unsigned i=0;i<RingLedCount;++i) {
            assert(frame[i]==0 || frame[i]==0x404040 || frame[i]==0x400000 || frame[i]==0x000040);
            if(value==0)assert(frame[i]==0 || frame[i]==0x404040);
            if(i==0)assert(frame[i]==0);
        }
    }
    // Calibration permutes physical indices; it must not change progress/colors.
    gauges.arcs[0].end = gauges.arcs[1].end = 80;
    for(unsigned top=0;top<RingLedCount;++top)for(bool clockwise:{false,true}) {
        config.topIndex=top;config.clockwise=clockwise;
        auto frame=renderGaugeLeds(gauges,config,true);
        for(unsigned i=0;i<RingLedCount;++i) assert(frame[(top+(clockwise?i:RingLedCount-i))%RingLedCount]==half[i]);
    }
    config.topIndex=0;config.clockwise=true;
    assert(renderGaugeLeds(gauges,config,false)==LedFrame{});
    gauges.active=false;assert(renderGaugeLeds(gauges,config,true)==LedFrame{});gauges.active=true;
    config.mode=LedMode::Off;assert(renderGaugeLeds(gauges,config,true)==LedFrame{});
    config.mode=LedMode::Identify;config.topIndex=12;auto single=renderGaugeLeds({},config,false);
    for(unsigned i=0;i<RingLedCount;++i)assert(single[i]==(i==12?0x404040u:0u));
    config.topIndex=13;assert(renderGaugeLeds(gauges,config,true)==LedFrame{});
    config.topIndex=0;config.brightness=0;assert(renderGaugeLeds(gauges,config,true)==LedFrame{});
    config.mode=LedMode::Ui;config.brightness=32;gauges.arcs[0].end=1000;gauges.arcs[1].end=-20;
    auto extremes=renderGaugeLeds(gauges,config,true);assert(extremes[8]==0x200000 && extremes[4]==0x202020);
    // Both themes supply their own current UI color, rather than hardcoded RGB.
    gauges.arcs[0].color=0xf62c2c;assert(renderGaugeLeds(gauges,config,true)[8]==dimLed(0xf62c2c,32));
    //12 equally spaced emitters place zero-value gauge endpoints exactly at5/7 o'clock.
    config.count=12;config.white=0xffffff;config.brightness=32;
    gauges.arcs[0]={0,160,0,25,120,120,0xff0000};gauges.arcs[1]={160,0,0,0,300,120,0x0000ff};
    const auto twelve=renderGaugeLeds(gauges,config,true);
    assert(twelve[7]==0x200000 && twelve[8]==0x202020 && twelve[5]==0x000020 && twelve[4]==0x202020);
    for(unsigned i:{0u,6u,12u})assert(twelve[i]==0);
    config.white=0xc0ff80;auto corrected=renderGaugeLeds(gauges,config,true);
    assert(corrected[4]==0x182010 && corrected[7]==twelve[7] && corrected[5]==twelve[5]);
    for(auto mode:{LedMode::Red,LedMode::Green,LedMode::Blue,LedMode::White}) {
        config.mode=mode;auto solid=renderGaugeLeds({},config,false);
        assert(solid[12]==0 && solid[0]!=0 && solid[0]==solid[11]);
    }
    config.mode=LedMode::Identify;config.topIndex=12;assert(renderGaugeLeds({},config,true)==LedFrame{});
    config.count=0;assert(renderGaugeLeds({},config,true)==LedFrame{});
    config.count=12;config.topIndex=0;config.mode=LedMode::Ui;config.white=0xffffff;config.brightness=255;
    const auto full=renderGaugeLeds(gauges,config,true);
    assert(full[7]==0xff0000 && full[5]==0x0000ff && full[4]==0xffffff);
    for(unsigned i:{0u,6u,12u})assert(full[i]==0);
    assert(dimLed(0x12ab34,255)==0x12ab34 && dimLed(0xffffff,0)==0);
    LedFrame previous{};
    for(unsigned level=0;level<=255;++level){
        config.brightness=level;const auto current=renderGaugeLeds(gauges,config,true);
        for(unsigned i=0;i<RingLedCount;++i)for(unsigned shift:{0u,8u,16u})
            assert(((current[i]>>shift)&255)>=((previous[i]>>shift)&255));
        previous=current;
    }
    puts("PASS: actual120-degree arcs, ascending temperature/descending pressure, monotonic fill, white remainder/dark gaps,12/13 calibration, theme colors, off/stale/invalid limits");
}
