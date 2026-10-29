"""Compile real logger with an unresponsive USB host and a full mock queue."""
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[2]
build = root / 'dist/diagnostics-test'
for p in (build, build / 'freertos', build / 'rom'): p.mkdir(parents=True, exist_ok=True)
(build / 'Arduino.h').write_text('''#pragma once
#include <cstdint>
#include <cstddef>
#include <cassert>
#define IRAM_ATTR
inline unsigned usbWrites=0, timeoutSet=0;
struct USB {
 void setTxTimeoutMs(uint32_t n) { timeoutSet=n; }
 void setDebugOutput(bool) {}
 explicit operator bool() const { return false; }
 int availableForWrite() { assert(false && "producer queried USB"); return 0; }
 size_t write(const uint8_t *, size_t) { ++usbWrites; assert(false && "producer wrote USB"); return 0; }
};
inline USB Serial;
''')
(build / 'freertos/FreeRTOS.h').write_text('''#pragma once
#include <cassert>
#include <cstdint>
#include <cstddef>
using BaseType_t=int;
constexpr int pdTRUE=1, pdPASS=1, configMINIMAL_STACK_SIZE=768;
inline bool mockIsISR=false;
inline unsigned tasks=0;
inline int xPortInIsrContext() { return mockIsISR; }
inline unsigned pdMS_TO_TICKS(unsigned n) { return n; }
inline void vTaskDelay(unsigned) { assert(false && "producer delayed"); }
inline int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,int,void*,int) { ++tasks; return pdPASS; }
''')
(build / 'freertos/queue.h').write_text('''#pragma once
#include "FreeRTOS.h"
struct Queue { unsigned size=0,capacity=0; };
using QueueHandle_t=Queue*;
inline Queue storage;
inline QueueHandle_t xQueueCreate(unsigned count,unsigned item) { assert(item==1); storage={0,count}; return &storage; }
inline int xQueueSend(QueueHandle_t q,const void*,unsigned timeout) { assert(timeout==0); if(q->size==q->capacity)return 0; ++q->size;return pdTRUE; }
inline int xQueueSendFromISR(QueueHandle_t q,const void *p,void*) { return xQueueSend(q,p,0); }
inline int xQueueReceive(QueueHandle_t,const void*,unsigned timeout) { assert(timeout==0); return 0; }
inline void vQueueDelete(QueueHandle_t) {}
''')
(build / 'rom/ets_sys.h').write_text('''#pragma once
inline void(*hook)(char)=nullptr;
inline void ets_install_putc2(void(*p)(char)) { hook=p; }
''')
(build / 'main.cpp').write_text('''#include "Diagnostics.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <rom/ets_sys.h>
#include <cstdio>
int main() {
 knobDiagnosticBegin(); assert(tasks==1 && hook && timeoutSet==1);
 for(int i=0;i<1000;++i) knobDiagnosticPrintf("USB host not reading: %d\\n",i);
 assert(storage.size==storage.capacity && knobDiagnosticDropped()>0);
 mockIsISR=true; for(int i=0;i<1000;++i) hook('x');
 assert(usbWrites==0);
 printf("PASS: saturated queue and stalled host never cause producer USB access or blocking waits; dropped=%u\\n",knobDiagnosticDropped());
}
''')
source = root / 'src/display/drivers/GuitionKnob'
subprocess.run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-DGAGGIMATE_KNOB','-I'+str(build),'-I'+str(source),str(source/'Diagnostics.cpp'),str(build/'main.cpp'),'-o',str(build/'check')],check=True)
subprocess.run([str(build/'check')],check=True)
