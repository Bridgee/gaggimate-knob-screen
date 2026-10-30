#include <display/plugins/ScreenOtaInstaller.h>
#include <cassert>
#include <thread>
using namespace screen_ota;
int main() {
    Preflight ready; ready.state = "metadata-ready";
    ready.plan.tag = "v0.4.0-rc.4";
    ready.plan.manifest.sha = std::string(64, 'a'); ready.plan.image.sha = std::string(64, 'b');
    Request request{"display", ready.plan.tag, ready.plan.manifest.sha, ready.plan.image.sha, true, true};
    Installer locked(false); assert(!locked.queue(ready, request));
    for (const char *component : {"controller", "both", "DISPLAY", ""}) {
        Installer onlyScreen(true); auto bad = request; bad.component = component;
        assert(!onlyScreen.queue(ready, bad)); assert(!onlyScreen.snapshot().busy);
    }
    Installer installer(true);
    auto normal = request; normal.qualification = false;
    assert(!installer.queue(ready, normal));
    auto stale = request; stale.imageSha = std::string(64, 'c');
    assert(!installer.queue(ready, stale));
    assert(!installer.queue({}, request));
    assert(installer.queue(ready, request)); assert(installer.snapshot().busy);
    assert(!installer.queue(ready, request));
    ready.plan.image.sha = std::string(64, 'd'); // Frozen queued identity cannot change with discovery.
    InstallJob frozen; assert(installer.takeQueued(frozen));
    assert(frozen.plan.image.sha == request.imageSha && frozen.verifyOnly);
    assert(!installer.takeQueued(frozen)); assert(installer.snapshot().busy);
    installer.fail("readiness-changed"); assert(!installer.snapshot().busy);
    ready.plan.image.sha = request.imageSha;
    std::atomic<unsigned> accepted{0};
    std::thread a([&] { if (installer.queue(ready, request)) ++accepted; });
    std::thread b([&] { if (installer.queue(ready, request)) ++accepted; });
    a.join(); b.join(); assert(accepted == 1);
    installer.fail("unknown-boot-selection", true);
    assert(installer.snapshot().busy && installer.snapshot().phase == "recovery-required");
    assert(!installer.queue(ready, request));
}
