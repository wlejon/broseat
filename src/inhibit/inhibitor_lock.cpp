// InhibitorLock: owns the fd logind hands back for an inhibitor. Built on every
// platform (off Linux nothing creates one with a valid fd).
#include "broseat/inhibit.h"
#include "common/utils.h"

#include <utility>

namespace broseat {

InhibitorLock::InhibitorLock(int fd, std::string what, std::string who, std::string why, InhibitMode mode)
    : fd_(fd), what_(std::move(what)), who_(std::move(who)), why_(std::move(why)), mode_(mode) {}

InhibitorLock::~InhibitorLock() {
    release();
}

InhibitorLock::InhibitorLock(InhibitorLock&& other) noexcept
    : fd_(other.fd_),
      what_(std::move(other.what_)),
      who_(std::move(other.who_)),
      why_(std::move(other.why_)),
      mode_(other.mode_) {
    other.fd_ = -1;
}

InhibitorLock& InhibitorLock::operator=(InhibitorLock&& other) noexcept {
    if (this != &other) {
        release();
        fd_ = other.fd_;
        what_ = std::move(other.what_);
        who_ = std::move(other.who_);
        why_ = std::move(other.why_);
        mode_ = other.mode_;
        other.fd_ = -1;
    }
    return *this;
}

void InhibitorLock::release() {
    if (fd_ >= 0) {
        utils::close_fd(fd_);
        fd_ = -1;
    }
}

}  // namespace broseat
