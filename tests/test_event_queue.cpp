#include "broseat/event_queue.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>
#include <vector>

int main() {
    using namespace broseat;

    EventQueue queue;
    assert(queue.empty());
    assert(queue.size() == 0);

    // Push events
    queue.push(SeatActiveChanged{ .active = true });
    queue.push(VtSwitched{ .vt_number = 2 });
    queue.push(SessionLockedChanged{ .locked = true });

    assert(!queue.empty());
    assert(queue.size() == 3);

    // Drain events
    auto events = queue.drain();
    assert(events.size() == 3);
    assert(queue.empty());
    assert(queue.size() == 0);

    assert(std::holds_alternative<SeatActiveChanged>(events[0]));
    assert(std::get<SeatActiveChanged>(events[0]).active == true);

    assert(std::holds_alternative<VtSwitched>(events[1]));
    assert(std::get<VtSwitched>(events[1]).vt_number == 2);

    assert(std::holds_alternative<SessionLockedChanged>(events[2]));
    assert(std::get<SessionLockedChanged>(events[2]).locked == true);

    // Test wait_for timeout
    auto start = std::chrono::steady_clock::now();
    bool has_items = queue.wait_for(std::chrono::milliseconds(50));
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    assert(!has_items);
    assert(duration.count() >= 40);

    // Test set_wake hook
    std::atomic<int> wake_count{0};
    queue.set_wake([&wake_count]() {
        wake_count.fetch_add(1);
    });

    queue.push(SeatActiveChanged{ .active = false });
    assert(wake_count.load() == 1);
    queue.push(SeatActiveChanged{ .active = true });
    assert(wake_count.load() == 2);

    assert(queue.wait_for(std::chrono::milliseconds(50)));
    events = queue.drain();
    assert(events.size() == 2);

    // Multi-threaded producer-consumer test
    const int num_producers = 4;
    const int items_per_producer = 250;
    std::vector<std::thread> producers;

    for (int p = 0; p < num_producers; ++p) {
        producers.emplace_back([&queue, p, items_per_producer]() {
            for (int i = 0; i < items_per_producer; ++i) {
                queue.push(VtSwitched{ .vt_number = p * 1000 + i });
            }
        });
    }

    size_t total_received = 0;
    while (total_received < num_producers * items_per_producer) {
        if (queue.wait_for(std::chrono::milliseconds(200))) {
            auto chunk = queue.drain();
            total_received += chunk.size();
        }
    }

    for (auto& t : producers) {
        t.join();
    }

    assert(total_received == num_producers * items_per_producer);
    assert(queue.empty());

    std::cout << "test_event_queue PASSED\n";
    return 0;
}
