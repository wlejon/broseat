// XDG autostart: .desktop parsing, the show/hide conditions, Exec= expansion
// and directory-priority discovery run on every platform (they are file and
// string handling). On Linux the launch paths run for real: fork/exec (the
// child's exit status), a systemd-run scope and a transient service on the
// user manager, each checked with systemctl --user and a marker file the
// launched command writes.
#include "check.h"
#include "broseat/autostart.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#if defined(__linux__)
#include "run.h"
#include <csignal>
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;
using namespace broseat;

namespace {

fs::path write_file(const fs::path& p, const std::string& text) {
    std::ofstream f(p, std::ios::binary);
    f << text;
    return p;
}

void test_parse_and_conditions(const fs::path& dir) {
    std::string err;
#if defined(_WIN32)
    const std::string try_exec = "cmd.exe";
#else
    const std::string try_exec = "sh";
#endif
    const std::string valid_text =
        "[Desktop Entry]\nType=Application\nName=Test Valid App\nName[de]=Ignored\n"
        "Exec=echo \"hello world\" %f\nTryExec=" + try_exec +
        "\nOnlyShowIn=BRO;GNOME;\nHidden=false\n# comment\n[Other Group]\nName=Not this\n";
    AutostartEntry e1;
    REQUIRE(AutostartManager::parse_desktop_file(write_file(dir / "valid_app.desktop", valid_text), &e1, &err));
    CHECK_EQ(e1.name, std::string("Test Valid App"));
    CHECK_EQ(e1.type, std::string("Application"));
    CHECK(!e1.hidden);
    CHECK_EQ(e1.only_show_in.size(), size_t(2));

    AutostartFilter bro{.current_desktop = "BRO"};
    AutostartFilter kde{.current_desktop = "KDE"};
    std::string reason;
    CHECK(AutostartManager::should_autostart(e1, bro, &reason));
    reason.clear();
    CHECK(!AutostartManager::should_autostart(e1, kde, &reason));
    CHECK(!reason.empty());

    AutostartEntry e2;
    REQUIRE(AutostartManager::parse_desktop_file(
        write_file(dir / "hidden.desktop", "[Desktop Entry]\nType=Application\nName=H\nExec=echo hi\nHidden=true\n"),
        &e2, &err));
    CHECK(e2.hidden);
    CHECK(!AutostartManager::should_autostart(e2, bro, &reason));
    AutostartFilter with_hidden = bro;
    with_hidden.include_hidden = true;
    CHECK(AutostartManager::should_autostart(e2, with_hidden, &reason));

    AutostartEntry e3;
    REQUIRE(AutostartManager::parse_desktop_file(
        write_file(dir / "not_show.desktop",
                   "[Desktop Entry]\nType=Application\nName=N\nExec=echo hi\nNotShowIn=BRO;XFCE;\n"),
        &e3, &err));
    CHECK(!AutostartManager::should_autostart(e3, bro, &reason));
    CHECK(AutostartManager::should_autostart(e3, kde, &reason));

    AutostartEntry e4;
    REQUIRE(AutostartManager::parse_desktop_file(
        write_file(dir / "try_exec_fail.desktop",
                   "[Desktop Entry]\nType=Application\nName=F\nExec=echo hi\nTryExec=nonexistent_binary_xyz_12345\n"),
        &e4, &err));
    CHECK(!AutostartManager::should_autostart(e4, bro, &reason));

    AutostartEntry e5;
    REQUIRE(AutostartManager::parse_desktop_file(
        write_file(dir / "condition.desktop",
                   "[Desktop Entry]\nType=Application\nName=C\nExec=echo hi\nAutostartCondition=BRO if-session BRO\n"),
        &e5, &err));
    CHECK(AutostartManager::should_autostart(e5, bro, &reason));
    CHECK(!AutostartManager::should_autostart(e5, kde, &reason));

    AutostartEntry e6;
    REQUIRE(AutostartManager::parse_desktop_file(
        write_file(dir / "link.desktop", "[Desktop Entry]\nType=Link\nName=L\nURL=https://example.org\n"), &e6,
        &err));
    CHECK(!AutostartManager::should_autostart(e6, bro, &reason));

    AutostartEntry missing;
    CHECK(!AutostartManager::parse_desktop_file(dir / "does_not_exist.desktop", &missing, &err));
    CHECK(!err.empty());
}

void test_exec_expansion() {
    auto argv = AutostartManager::expand_exec_arguments(
        "app --flag \"quoted argument\" 'single quote' %f %u %%");
    REQUIRE(argv.size() == 5);
    CHECK_EQ(argv[0], std::string("app"));
    CHECK_EQ(argv[1], std::string("--flag"));
    CHECK_EQ(argv[2], std::string("quoted argument"));
    CHECK_EQ(argv[3], std::string("single quote"));
    CHECK_EQ(argv[4], std::string("%"));
    auto esc = AutostartManager::expand_exec_arguments("a\\ b \"c\\\"d\"");
    REQUIRE(esc.size() == 2);
    CHECK_EQ(esc[0], std::string("a b"));
    CHECK_EQ(esc[1], std::string("c\"d"));
    CHECK(AutostartManager::expand_exec_arguments("   ").empty());
}

void test_discovery(const fs::path& dir) {
    fs::path user = dir / "user_autostart";
    fs::path sys = dir / "sys_autostart";
    fs::create_directories(user);
    fs::create_directories(sys);
    write_file(sys / "item.desktop", "[Desktop Entry]\nType=Application\nName=Sys Item\nExec=true\n");
    write_file(user / "item.desktop", "[Desktop Entry]\nType=Application\nName=User Override Item\nExec=true\n");
    write_file(sys / "other.desktop", "[Desktop Entry]\nType=Application\nName=Other\nExec=true\n");
    write_file(sys / "readme.txt", "not a desktop file\n");

    AutostartFilter bro{.current_desktop = "BRO"};
    auto found = AutostartManager::discover({user, sys, dir / "missing_dir"}, bro);
    REQUIRE(found.size() == 2);
    bool saw_override = false, saw_other = false;
    for (auto& e : found) {
        if (e.name == "User Override Item") saw_override = true;
        if (e.name == "Other") saw_other = true;
        CHECK(e.name != "Sys Item");
    }
    CHECK(saw_override);
    CHECK(saw_other);
    CHECK(!AutostartManager::default_search_paths().empty());
}

#if defined(__linux__)

bool wait_for_file(const fs::path& p) {
    return bstest::wait_until([&] { return fs::exists(p); }, std::chrono::seconds(10));
}

void test_launch_fork_exec(const fs::path& dir) {
    AutostartEntry e;
    e.id = "broseat-test-fork";
    e.exec = "sh -c \"exit 3\"";
    auto r = AutostartManager::launch(e, LaunchMode::ForkExec);
    REQUIRE(r.success);
    REQUIRE(r.pid > 0);
    int status = 0;
    REQUIRE(waitpid(static_cast<pid_t>(r.pid), &status, 0) == static_cast<pid_t>(r.pid));
    CHECK(WIFEXITED(status));
    CHECK_EQ(WEXITSTATUS(status), 3);

    // Working directory is honoured.
    AutostartEntry wd;
    wd.id = "broseat-test-wd";
    wd.working_dir = dir.string();
    wd.exec = "sh -c \"pwd > pwd.txt\"";
    auto r2 = AutostartManager::launch(wd, LaunchMode::ForkExec);
    REQUIRE(r2.success);
    waitpid(static_cast<pid_t>(r2.pid), &status, 0);
    std::ifstream in(dir / "pwd.txt");
    std::string line;
    std::getline(in, line);
    CHECK_EQ(fs::canonical(line), fs::canonical(dir));
}

void test_launch_systemd(const fs::path& dir) {
    if (!bstest::run("systemctl --user show-environment").ok()) {
        std::printf("Note: no systemd user manager; the SystemdRun and SystemdTransient launches were not checked\n");
        return;
    }
    const std::string tag = std::to_string(getpid());

    // Transient service: the user manager runs it; the marker proves it ran,
    // systemctl proves it was a unit of that name while it ran.
    AutostartEntry t;
    t.id = "broseat-test-transient-" + tag;
    t.name = "broseat transient test";
    t.exec = "sh -c \"echo ran > '" + (dir / "transient.txt").string() + "'; sleep 30\"";
    auto rt = AutostartManager::launch(t, LaunchMode::SystemdTransient);
    REQUIRE(rt.success);
    CHECK(!rt.unit_name.empty());
    CHECK(wait_for_file(dir / "transient.txt"));
    auto active = bstest::run("systemctl --user is-active '" + rt.unit_name + "'");
    CHECK_EQ(bstest::trimmed(active.out), std::string("active"));
    auto desc = bstest::run("systemctl --user show -p Description --value '" + rt.unit_name + "'");
    CHECK_EQ(bstest::trimmed(desc.out), t.name);
    bstest::run("systemctl --user stop '" + rt.unit_name + "'");

    // systemd-run --scope: the scope is a unit while the process runs.
    AutostartEntry s;
    s.id = "broseat-test-scope-" + tag;
    s.exec = "sh -c \"echo ran > '" + (dir / "scope.txt").string() + "'; sleep 30\"";
    auto rs = AutostartManager::launch(s, LaunchMode::SystemdRun);
    REQUIRE(rs.success);
    REQUIRE(rs.pid > 0);
    CHECK(wait_for_file(dir / "scope.txt"));
    bool scope_active = bstest::wait_until(
        [&] {
            return bstest::trimmed(bstest::run("systemctl --user is-active '" + rs.unit_name + "'").out) ==
                   "active";
        },
        std::chrono::seconds(10));
    CHECK(scope_active);
    bstest::run("systemctl --user stop '" + rs.unit_name + "'");
    int status = 0;
    waitpid(static_cast<pid_t>(rs.pid), &status, 0);
}

#endif

}  // namespace

int main() {
    fs::path dir = fs::temp_directory_path() / ("broseat_test_autostart_" +
                                                std::to_string(std::chrono::steady_clock::now()
                                                                   .time_since_epoch()
                                                                   .count()));
    fs::create_directories(dir);

    test_parse_and_conditions(dir);
    test_exec_expansion();
    test_discovery(dir);
#if defined(__linux__)
    test_launch_fork_exec(dir);
    test_launch_systemd(dir);
#endif

    std::error_code ec;
    fs::remove_all(dir, ec);
    return bstest::finish("test_autostart");
}
