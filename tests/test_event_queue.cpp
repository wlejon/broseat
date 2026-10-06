// EventQueue: ordering, drain, wait_for timeouts, the wake hook, and many
// producers against one consumer. Portable: runs on every platform.
#include "check.h"
#include "broseat/event_queue.h"

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace broseat;

static void test_order_and_drain() {
    EventQueue queue;
    CHECK(queue.empty());
    CHECK_EQ(queue.size(), size_t(0));

    queue.push(SeatActiveChanged{.active = true});
    queue.push(VtSwitched{.vt_number = 2});
    queue.push(SessionLockedChanged{.locked = true});
    CHECK(!queue.empty());
    CHECK_EQ(queue.size(), size_t(3));

    auto events = queue.drain();
    REQUIRE(events.size() == 3);
    CHECK(queue.empty());
    REQUIRE(std::holds_alternative<SeatActiveChanged>(events[0]));
    CHECK(std::get<SeatActiveChanged>(events[0]).active);
    REQUIRE(std::holds_alternative<VtSwitched>(events[1]));
    CHECK_EQ(std::get<VtSwitched>(events[1]).vt_number, 2);
    REQUIRE(std::holds_alternative<SessionLockedChanged>(events[2]));
    CHECK(std::get<SessionLockedChanged>(events[2]).locked);
}

static void test_wait_and_wake() {
    EventQueue queue;
    auto start = std::chrono::steady_clock::now();
    CHECK(!queue.wait_for(std::chrono::milliseconds(50)));
    auto waited = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    CHECK(waited.count() >= 40);

    std::atomic<int> wakes{0};
    queue.set_wake([&wakes] { wakes.fetch_add(1); });
    queue.push(SeatActiveChanged{.active = false});
    CHECK_EQ(wakes.load(), 1);
    queue.push(SeatActiveChanged{.active = true});
    CHECK_EQ(wakes.load(), 2);
    CHECK(queue.wait_for(std::chrono::milliseconds(50)));
    CHECK_EQ(queue.drain().size(), size_t(2));
}

static void test_many_producers() {
    EventQueue queue;
    constexpr int kProducers = 4;
    constexpr int kPerProducer = 250;
    std::vector<std::thread> producers;
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&queue, p] {
            for (int i = 0; i < kPerProducer; ++i) queue.push(VtSwitched{.vt_number = p * 1000 + i});
        });
    }

    std::vector<int> seen;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (seen.size() < size_t(kProducers * kPerProducer) && std::chrono::steady_clock::now() < deadline) {
        if (queue.wait_for(std::chrono::milliseconds(200))) {
            for (auto& e : queue.drain()) seen.push_back(std::get<VtSwitched>(e).vt_number);
        }
    }
    for (auto& t : producers) t.join();

    CHECK_EQ(seen.size(), size_t(kProducers * kPerProducer));
    CHECK(queue.empty());
    // Each producer's items arrive in the order it pushed them.
    std::vector<int> last(kProducers, -1);
    for (int v : seen) {
        int p = v / 1000, i = v % 1000;
        CHECK(i > last[p]);
        last[p] = i;
    }
}

int main() {
    test_order_and_drain();
    test_wait_and_wake();
    test_many_producers();
    return bstest::finish("test_event_queue");
}
