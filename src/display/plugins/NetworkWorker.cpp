#ifdef GAGGIMATE_KNOB
#include "NetworkWorker.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace network_work {
namespace {
Pending pending;
std::mutex startupMutex;
TaskHandle_t task = nullptr;
void worker(void *) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        while (auto work = pending.take()) work();
    }
}
}
bool begin() {
    std::lock_guard<std::mutex> lock(startupMutex);
    if (!task && xTaskCreate(worker, "K718 network", 8192, nullptr, 1, &task) != pdPASS) {
        task = nullptr; return false;
    }
    return true;
}
bool post(Kind kind, std::function<void()> work) {
    if (!begin() || !pending.put(kind, std::move(work))) return false;
    xTaskNotifyGive(task); return true;
}
} // namespace network_work
#endif
