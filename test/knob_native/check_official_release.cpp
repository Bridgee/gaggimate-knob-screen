#include <display/plugins/OfficialReleasePolicy.h>
#include <cassert>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>
using namespace official_release;
int main() {
    Mailbox mailbox;
    assert(mailbox.snapshot().status == "not-checked");
    auto first = mailbox.enqueue("latest", "v1.9.0");
    Request old; assert(mailbox.take(old)); assert(old.generation == first);
    std::promise<void> started, release;
    auto gate = release.get_future(); bool stalePublished = true;
    std::thread network([&] {
        started.set_value(); gate.wait();
        Snapshot result; result.status = "ready"; result.version = "1.10.0"; result.displayUpdate = true;
        stalePublished = mailbox.publish(old.generation, result);
    });
    started.get_future().wait();
    auto poll = std::async(std::launch::async, [&] { return mailbox.snapshot(); });
    bool responsive = poll.wait_for(std::chrono::seconds(2)) == std::future_status::ready;
    auto second = mailbox.enqueue("nightly", "v1.9.0");
    release.set_value(); network.join();
    assert(responsive); assert(poll.get().status == "checking"); assert(!stalePublished);
    Request current; assert(mailbox.take(current)); assert(current.generation == second && current.channel == "nightly");
    Snapshot ready; ready.status = "ready"; ready.version = "1.10.0-beta.1"; ready.displayUpdate = true;
    assert(mailbox.publish(second, ready)); assert(mailbox.snapshot().displayUpdate);
    auto third = mailbox.enqueue("invalid", "");
    assert(mailbox.snapshot().version.empty() && !mailbox.snapshot().displayUpdate);
    assert(mailbox.take(current) && current.channel == "latest" && current.controllerVersion.empty());
    Snapshot failed; failed.status = "error"; failed.message = "timeout";
    assert(mailbox.publish(third, failed)); assert(mailbox.snapshot().version.empty());
    assert(!mailbox.snapshot().displayUpdate && !mailbox.snapshot().controllerUpdate);
    assert(!mailbox.take(current)); assert(mailbox.takeChanged()); assert(!mailbox.takeChanged());
    std::cout << "PASS: blocked network leaves status responsive; stale channel/error results cannot leak update state\n";
}
