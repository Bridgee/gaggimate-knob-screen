#include "display/drivers/GuitionKnob/EncoderDetents.h"
#include "display/ui/default/KnobAdjustmentPolicy.h"
#include "display/ui/default/StandbyDisplayPolicy.h"
#include <cassert>
#include <cstdio>
#include <climits>
#include <cstring>
#include "display/core/constants.h"
#include "display/ui/default/GrindKnobPolicy.h"
using namespace guition_knob;
using namespace gaggimate_ui;
int main() {
    EncoderDetents encoder;encoder.reset(3);
    auto samples=[&](uint8_t phase,int count){int total=0;for(int i=0;i<count;++i)total+=encoder.sample(phase);return total;};
    assert(samples(3,4)==0); // First release only arms the detector.
    for(int detent=0;detent<5;++detent){assert(samples(1,12)==0);assert(samples(3,4)==1);}
    assert(samples(2,12)==0);assert(samples(3,4)==-1);
    assert(samples(3,20000)==0); // Idle cannot generate repeats.
    // Stable low pulse counts only once on release, even when held for seconds.
    assert(samples(1,4000)==0);assert(samples(3,4)==1);assert(samples(3,400)==0);
    // Press/release chatter and short spikes are rejected before qualification.
    assert(samples(1,2)==0);assert(samples(3,1)==0);assert(samples(1,5)==0);assert(samples(3,4)==0);
    assert(encoder.rejectedShortPulses()==1);
    // Release chatter shorter than2ms must not split one accepted pulse.
    assert(samples(2,12)==0);assert(samples(3,2)==0);assert(samples(2,3)==0);assert(samples(3,4)==-1);
    // If both directions qualify before release, discard the ambiguous pair.
    assert(samples(0,12)==0);assert(samples(3,4)==0);assert(encoder.ambiguousPulses()==1);
    assert(samples(1,12)==0);assert(samples(0,12)==0);assert(samples(2,12)==0);assert(samples(3,4)==0);
    assert(encoder.ambiguousPulses()==2);
    // Booting with a contact held cannot change a parameter on its first release.
    encoder.reset(1);assert(samples(1,100)==0);assert(samples(3,4)==0);
    assert(samples(1,12)==0);assert(samples(3,4)==1);
    KnobStepDecoder decoder;decoder.calibrate(1,-1);
    assert(decoder.consume(10,false)==0);assert(decoder.consume(8,false)==0);
    assert(decoder.consume(7,true)==1);assert(decoder.consume(8,true)==-1);
    assert(decoder.consume(-100,true)==0); // stale/reset jump is discarded
    assert(decoder.consume(-101,true)==1);
    KnobStepDecoder wrap;wrap.calibrate(1,1);assert(wrap.consume(INT_MAX,true)==0);assert(wrap.consume(INT_MIN,true)==1);
    KnobStepDecoder quarters;quarters.calibrate(4,1);assert(quarters.consume(0,true)==0);
    assert(quarters.consume(3,true)==0);quarters.clearFraction();assert(quarters.consume(4,true)==0);
    assert(quarters.consume(7,true)==1);assert(quarters.consume(15,false)==0);assert(quarters.consume(19,true)==1);
    assert(grindKnobDuration(25000,1,BREW_MIN_DURATION_MS,BREW_MAX_DURATION_MS)==25500);
    assert(grindKnobDuration(25500,-1,BREW_MIN_DURATION_MS,BREW_MAX_DURATION_MS)==25000);
    assert(grindKnobDuration(59500,1,BREW_MIN_DURATION_MS,BREW_MAX_DURATION_MS)==60000);
    assert(grindKnobDuration(5000,-1,BREW_MIN_DURATION_MS,BREW_MAX_DURATION_MS)==5000);
    assert(grindKnobDuration(INT_MAX,1,BREW_MIN_DURATION_MS,BREW_MAX_DURATION_MS)==BREW_MAX_DURATION_MS);
    assert(grindKnobWeight(18,1,BREW_MIN_VOLUMETRIC,BREW_MAX_VOLUMETRIC)==18.5);
    assert(grindKnobWeight(18.5,-1,BREW_MIN_VOLUMETRIC,BREW_MAX_VOLUMETRIC)==18);
    assert(grindKnobWeight(5,-1,BREW_MIN_VOLUMETRIC,BREW_MAX_VOLUMETRIC)==5);
    assert(grindKnobWeight(250,1,BREW_MIN_VOLUMETRIC,BREW_MAX_VOLUMETRIC)==250);
    char grindTime[12];formatGrindKnobDuration(25500,grindTime,sizeof(grindTime));assert(!std::strcmp(grindTime,"0:25.5"));
    formatGrindKnobDuration(60000,grindTime,sizeof(grindTime));assert(!std::strcmp(grindTime,"1:00.0"));
    formatGrindKnobDuration(300000,grindTime,sizeof(grindTime));assert(!std::strcmp(grindTime,"5:00.0"));
    assert(validStandbyContent("logo") && validStandbyContent("clock") && validStandbyContent("text"));
    assert(!validStandbyContent("other") && !validStandbyContent(nullptr));
    assert(validStandbyText("My Coffee 123!") && validStandbyText("") && validStandbyText("ABCDEFGHIJKLMNOPQRSTUVWX"));
    assert(!validStandbyText("ABCDEFGHIJKLMNOPQRSTUVWXY") && !validStandbyText("one\ntwo") && !validStandbyText("\x7f") && !validStandbyText("\xe4\xb8\xad"));
    puts("PASS: independent direction pulses, debounce/conflicts/held-at-boot, reverse/wrap/no backlog, printable ASCII and24-character bounds");
}
