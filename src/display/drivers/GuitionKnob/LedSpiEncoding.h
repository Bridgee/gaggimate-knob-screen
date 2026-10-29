#pragma once
#include "LedGaugePolicy.h"
#include <array>
namespace guition_knob {
constexpr unsigned LedResetBytes=128; //409.6us at2.5MHz, before and after the frame
constexpr unsigned LedWireBytes=LedResetBytes*2+RingLedCount*9;
constexpr unsigned LedDmaBytes=(LedWireBytes+3)&~3u;
using LedWireFrame=std::array<uint8_t,LedDmaBytes>;
inline LedWireFrame encodeLedSpi(const LedFrame &pixels) {
    LedWireFrame bytes{};
    unsigned cursor=LedResetBytes*8;
    for(auto color:pixels) {
        const uint32_t grb=((color&0xff00)<<8)|((color&0xff0000)>>8)|(color&0xff);
        for(int bit=23;bit>=0;--bit) {
            const unsigned symbol=(grb&(1u<<bit))?6:4; //1=110,0=100;400ns per SPI bit
            for(int n=2;n>=0;--n,++cursor)
                if(symbol&(1u<<n)) bytes[cursor/8]|=1u<<(7-cursor%8);
        }
    }
    return bytes;
}
} // namespace guition_knob
