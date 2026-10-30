#ifdef GAGGIMATE_KNOB
#include "ScreenOtaInstaller.h"

namespace screen_ota {
bool Installer::queue(const Preflight &preflight, const Request &request) {
    const bool normal = mayInstall(preflight, request) && !request.qualification && !request.verifyOnly;
    if (!normal && !mayQualify(preflight, request, qualificationPermission)) return false;
    std::lock_guard<std::mutex> lock(mutex);
    if (status.busy) return false;
    job = {preflight.plan, request.verifyOnly}; queued = true;
    status = {}; status.phase = "queued"; status.tag = job.plan.tag; status.busy = true;
    changed = true; return true;
}
bool Installer::takeQueued(InstallJob &value) {
    std::lock_guard<std::mutex> lock(mutex);
    if (!queued) return false;
    value = job; queued = false; return true;
}
void Installer::fail(const char *error, bool holdForRecovery) {
    std::lock_guard<std::mutex> lock(mutex);
    status.phase = holdForRecovery ? "recovery-required" : "error";
    status.error = error; status.busy = holdForRecovery; queued = false; changed = true;
}
} // namespace screen_ota
#endif
