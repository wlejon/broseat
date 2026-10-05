#pragma once

#include "broseat/event_queue.h"
#include "broseat/events.h"
#include "broseat/types.h"

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace broseat {

class InhibitorLock {
public:
    InhibitorLock() = default;
    InhibitorLock(int fd, std::string what, std::string who, std::string why, InhibitMode mode);
    ~InhibitorLock();

    InhibitorLock(const InhibitorLock&) = delete;
    InhibitorLock& operator=(const InhibitorLock&) = delete;

    InhibitorLock(InhibitorLock&& other) noexcept;
    InhibitorLock& operator=(InhibitorLock&& other) noexcept;

    void release();
    int fd() const noexcept { return fd_; }
    bool is_held() const noexcept { return fd_ >= 0; }
    const std::string& what() const noexcept { return what_; }
    const std::string& who() const noexcept { return who_; }
    const std::string& why() const noexcept { return why_; }
    InhibitMode mode() const noexcept { return mode_; }

private:
    int fd_ = -1;
    std::string what_;
    std::string who_;
    std::string why_;
    InhibitMode mode_ = InhibitMode::Block;
};

class InhibitManager {
public:
    static std::unique_ptr<InhibitManager> create(std::string* error = nullptr);
    virtual ~InhibitManager() = default;

    virtual std::unique_ptr<InhibitorLock> inhibit(
        const std::string& what,
        const std::string& who,
        const std::string& why,
        InhibitMode mode = InhibitMode::Block,
        std::string* error = nullptr) = 0;

    virtual std::vector<InhibitorInfo> list_inhibitors(std::string* error = nullptr) = 0;
    virtual bool is_inhibited(const std::string& what) = 0;

    virtual EventQueue& event_queue() noexcept = 0;
    virtual void set_event_callback(std::function<void(const Event&)> cb) = 0;

    virtual int poll_fd() const noexcept = 0;
    virtual int dispatch(int timeout_ms = 0) = 0;
};

class IdleAggregator {
public:
    explicit IdleAggregator(std::chrono::milliseconds idle_timeout = std::chrono::milliseconds(300000));

    void report_activity(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());
    void set_idle_timeout(std::chrono::milliseconds timeout);
    std::chrono::milliseconds idle_timeout() const noexcept { return idle_timeout_; }

    bool is_idle() const noexcept { return is_idle_; }
    std::chrono::milliseconds current_idle_time(
        std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now()) const;

    void set_inhibited(bool inhibited);
    bool is_inhibited() const noexcept { return is_inhibited_; }

    bool check_idle(std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    EventQueue& event_queue() noexcept { return event_queue_; }
    void set_event_callback(std::function<void(const Event&)> cb) { callback_ = std::move(cb); }

private:
    std::chrono::milliseconds idle_timeout_;
    std::chrono::steady_clock::time_point last_activity_;
    bool is_idle_ = false;
    bool is_inhibited_ = false;
    EventQueue event_queue_;
    std::function<void(const Event&)> callback_;
};

}  // namespace broseat
