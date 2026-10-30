#pragma once
#ifdef GAGGIMATE_KNOB
#include "OfficialReleasePolicy.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
namespace official_release {
class Discovery {
  public:
    // Main-loop scheduling only; network work is isolated on a low-priority task.
    void request(const std::string &channel, const std::string &controllerVersion);
    Snapshot snapshot() const { return mailbox.snapshot(); }
    bool takeChanged() { return mailbox.takeChanged(); }
  private:
    void check();
    Mailbox mailbox;
};
}
#endif
