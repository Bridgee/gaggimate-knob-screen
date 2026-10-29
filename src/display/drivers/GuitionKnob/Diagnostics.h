#pragma once
#include <Arduino.h>

#ifdef GAGGIMATE_KNOB
void knobDiagnosticBegin();
void knobDiagnosticPrintf(const char *format, ...) __attribute__((format(printf, 1, 2)));
uint32_t knobDiagnosticDropped();
#else
#define knobDiagnosticPrintf Serial.printf
#endif
