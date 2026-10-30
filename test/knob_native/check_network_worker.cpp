#include <display/plugins/NetworkWorker.h>
#include <freertos/task.h>
#include <cassert>
#include <thread>
#include <vector>

namespace {
struct Idle {};
void (*entry)(void *) = nullptr;
void *argument = nullptr;
unsigned creates = 0, notifications = 0;
bool refuse = true;
}
int xTaskCreate(void (*fn)(void *), const char *, unsigned stack, void *arg, unsigned priority, TaskHandle_t *handle) {
    ++creates; assert(stack == 8192 && priority == 1);
    if (refuse) return 0;
    entry = fn; argument = arg; *handle = reinterpret_cast<void *>(1); return pdPASS;
}
void xTaskNotifyGive(TaskHandle_t handle) { assert(handle); ++notifications; }
unsigned ulTaskNotifyTake(int clear, unsigned timeout) {
    assert(clear == pdTRUE && timeout == portMAX_DELAY);
    if (!notifications) throw Idle{};
    return std::exchange(notifications, 0);
}
int main() {
    using namespace network_work;
    assert(!begin());
    assert(!post(Kind::Screen, [] { assert(false); }));
    refuse = false;
    assert(begin()); assert(creates == 3);
    std::vector<int> order;
    assert(post(Kind::Official, [&] { order.push_back(90); }));
    assert(post(Kind::Official, [&] { order.push_back(3); })); // latest request wins
    assert(post(Kind::Matched, [&] { order.push_back(2); }));
    assert(post(Kind::Screen, [&] {
        order.push_back(1);
        // A callback may post from another thread: no queue/startup lock spans work.
        std::thread other([&] { assert(post(Kind::Official, [&] { order.push_back(4); })); });
        other.join();
    }));
    assert(!post(Kind::Count, [] {})); assert(!post(Kind::Screen, {}));
    try { entry(argument); } catch (const Idle &) {}
    assert((order == std::vector<int>{1, 2, 4}));
    assert(creates == 3); // one successful task, no per-operation stack allocations
    assert(post(Kind::Screen, [&] { order.push_back(5); }));
    try { entry(argument); } catch (const Idle &) {}
    assert(order.back() == 5 && creates == 3);
}
