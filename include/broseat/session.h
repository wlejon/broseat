#pragma once

#include "broseat/event_queue.h"
#include "broseat/events.h"
#include "broseat/types.h"

#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace broseat {

inline constexpr std::string_view kGraphicalSessionTarget = "graphical-session.target";
inline constexpr std::string_view kGraphicalSessionPreTarget = "graphical-session-pre.target";

bool export_environment(const std::vector<std::pair<std::string, std::string>>& vars, std::string* error = nullptr);
bool export_environment(const std::vector<std::string>& var_names, std::string* error = nullptr);
bool unset_environment(const std::vector<std::string>& var_names, std::string* error = nullptr);

bool start_unit(const std::string& unit_name, const std::string& mode = "replace", std::string* error = nullptr);
bool stop_unit(const std::string& unit_name, const std::string& mode = "replace", std::string* error = nullptr);
bool restart_unit(const std::string& unit_name, const std::string& mode = "replace", std::string* error = nullptr);
bool reset_failed_unit(const std::string& unit_name, std::string* error = nullptr);
bool is_unit_active(const std::string& unit_name, std::string* error = nullptr);

class SessionManager {
public:
    static std::unique_ptr<SessionManager> create(std::string* error = nullptr);
    virtual ~SessionManager() = default;

    virtual SessionInfo current_session_info() = 0;
    virtual bool is_locked() const = 0;

    virtual bool lock_session() = 0;
    virtual bool unlock_session() = 0;

    virtual EventQueue& event_queue() noexcept = 0;
    virtual void set_event_callback(std::function<void(const Event&)> cb) = 0;

    virtual int poll_fd() const noexcept = 0;
    virtual int dispatch(int timeout_ms = 0) = 0;
};

}  // namespace broseat
