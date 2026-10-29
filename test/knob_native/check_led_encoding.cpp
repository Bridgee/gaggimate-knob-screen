#include "display/drivers/GuitionKnob/LedSpiEncoding.h"
#include <cassert>
#include <cstdio>
using namespace guition_knob;
int main(){
    LedFrame frame{};frame[0]=0x12ab34;frame[1]=0x00ff00;frame[2]=0x0000ff;frame[3]=0xffffff;
    const auto wire=encodeLedSpi(frame);
    for(unsigned i=0;i<LedResetBytes;++i)assert(wire[i]==0);
    unsigned cursor=LedResetBytes*8;
    for(unsigned pixel=0;pixel<RingLedCount;++pixel){
        uint32_t decoded=0;
        for(unsigned bit=0;bit<24;++bit){
            unsigned symbol=0;
            for(unsigned n=0;n<3;++n,++cursor)symbol=(symbol<<1)|((wire[cursor/8]>>(7-cursor%8))&1);
            assert(symbol==4 || symbol==6);decoded=(decoded<<1)|(symbol==6);
        }
        const uint32_t expected=pixel==0?0xab1234:pixel==1?0xff0000:pixel==2?0x0000ff:pixel==3?0xffffff:0;
        assert(decoded==expected);
    }
    assert(cursor==(LedResetBytes+RingLedCount*9)*8);
    for(unsigned i=cursor/8;i<wire.size();++i)assert(wire[i]==0);
    assert(LedResetBytes*8*400>=280000 && wire.size()%4==0);
    puts("PASS: complete GRB encoding, exact312 symbols,400/800ns pulse widths, leading/trailing reset, DMA alignment");
}
