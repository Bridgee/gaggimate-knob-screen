#pragma once
#include <cstdint>
namespace guition_knob {
// K718 vendor bidi_switch_knob.c treats A(GPIO2) as RIGHT and B(GPIO1)
// as LEFT, independently. These are direction contacts, not quadrature.
// At500us/sample qualify6ms low (vendor: two3ms low samples), then2ms
// with BOTH contacts high. Count once on release; reject a conflicting
// pair and never count a contact held during boot.
class EncoderDetents {
  public:
    void reset(uint8_t phase) {
        stable=phase&3;qualified=0;armed=false;sawLow=false;
        low[0]=low[1]=high[0]=high[1]=0;ambiguous=shortPulses=0;
    }
    int sample(uint8_t phase) {
        phase &= 3;
        for(unsigned channel=0;channel<2;++channel) {
            const uint8_t bit=channel==0?2:1;
            if(phase&bit) {
                low[channel]=0;
                if(high[channel]<HighSamples)++high[channel];
                if(high[channel]==HighSamples)stable|=bit;
            } else {
                high[channel]=0;
                if(armed)sawLow=true;
                if(low[channel]<LowSamples)++low[channel];
                if(low[channel]==LowSamples) {
                    stable&=uint8_t(~bit);
                    if(armed)qualified|=bit;
                }
            }
        }
        if(high[0]<HighSamples || high[1]<HighSamples)return 0;
        int step=0;
        if(armed && sawLow) {
            if(qualified==2)step=1;
            else if(qualified==1)step=-1;
            else if(qualified==3)++ambiguous;
            else ++shortPulses;
        }
        armed=true;qualified=0;sawLow=false;
        return step;
    }
    uint32_t ambiguousPulses()const{return ambiguous;}
    uint32_t rejectedShortPulses()const{return shortPulses;}
    uint8_t phase()const{return stable;}
  private:
    static constexpr uint8_t LowSamples=12,HighSamples=4;
    uint8_t stable=3,qualified=0,low[2]{},high[2]{};
    bool armed=false,sawLow=false;
    uint32_t ambiguous=0,shortPulses=0;
};
} // namespace guition_knob
