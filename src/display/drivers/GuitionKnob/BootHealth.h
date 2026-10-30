#pragma once
#ifdef GAGGIMATE_KNOB
#include <cstdint>
namespace guition_knob {
void bootHealthBegin();
void bootHealthSetup(bool storage, bool settings);
void bootHealthPanel(bool touch);
void bootHealthFrame();
void bootHealthMainPulse();
void bootHealthUiPulse();
void bootHealthLogicPulse();
const char *bootHealthState();
uint32_t bootHealthSlot();
bool bootHealthPending();
} // namespace guition_knob
#endif
