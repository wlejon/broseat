#include "broseat/session.h"
#include "common/utils.h"

#include <cassert>
#include <iostream>
#include <utility>
#include <vector>

int main() {
    using namespace broseat;

    // Test environment export
    std::string err;
    std::vector<std::pair<std::string, std::string>> env_vars = {
        {"BROSEAT_TEST_FOO", "bar123"}
    };
    bool ok = export_environment(env_vars, &err);
    assert(ok);
    assert(utils::get_env("BROSEAT_TEST_FOO") == "bar123");

    ok = export_environment(std::vector<std::string>{"BROSEAT_TEST_FOO"}, &err);
    assert(ok);

    unset_environment({"BROSEAT_TEST_FOO"}, &err);

    // Test unit active check
    bool active = is_unit_active("init.scope");
    std::cout << "init.scope is_active: " << (active ? "true" : "false") << "\n";

    // Test SessionManager creation and session info
    auto session_mgr = SessionManager::create(&err);
    if (!session_mgr) {
        std::cout << "SessionManager not created: " << err << "\n";
    } else {
        auto info = session_mgr->current_session_info();
        std::cout << "Session info: id=" << info.id
                  << ", user=" << info.user
                  << ", seat=" << info.seat
                  << ", vt=" << info.vt
                  << ", active=" << (info.active ? "yes" : "no")
                  << ", locked=" << (info.locked ? "yes" : "no")
                  << ", state=" << session_state_name(info.state)
                  << "\n";
        assert(!info.id.empty() || !info.user.empty() || info.active);
    }

    std::cout << "test_session PASSED\n";
    return 0;
}
