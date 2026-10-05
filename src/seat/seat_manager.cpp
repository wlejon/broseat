#include "broseat/seat.h"
#include "common/utils.h"
#include "seat/libseat_backend.h"
#include "seat/logind_backend.h"
#include "seat/mock_backend.h"

namespace broseat {

std::unique_ptr<Seat> Seat::create(const SeatConfig& config, std::string* error) {
    SeatBackendType backend_type = config.backend;

    if (backend_type == SeatBackendType::Auto) {
        std::string env_backend = utils::get_env("BROSEAT_BACKEND");
        if (!env_backend.empty()) {
            backend_type = parse_seat_backend(env_backend);
        }
    }

    if (backend_type == SeatBackendType::Mock) {
        std::string name = config.seat_name.empty() ? "seat0" : config.seat_name;
        return std::make_unique<MockSeatBackend>(name);
    }

    if (backend_type == SeatBackendType::Libseat) {
        return LibseatBackend::open(config, error);
    }

    if (backend_type == SeatBackendType::Logind) {
        return LogindBackend::open(config, error);
    }

    // Auto mode: try Libseat first, then Logind
    std::string libseat_err;
    std::unique_ptr<Seat> seat = LibseatBackend::open(config, &libseat_err);
    if (seat) {
        return seat;
    }

    std::string logind_err;
    seat = LogindBackend::open(config, &logind_err);
    if (seat) {
        return seat;
    }

    if (error) {
        *error = "Failed to open seat (libseat: " + libseat_err + "; logind: " + logind_err + ")";
    }
    return nullptr;
}

}  // namespace broseat
