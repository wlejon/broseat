#include "broseat/autostart.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
    using namespace broseat;

    fs::path temp_dir = fs::temp_directory_path() / "broseat_test_autostart";
    fs::create_directories(temp_dir);

    // 1. Create a valid desktop file
    fs::path p1 = temp_dir / "valid_app.desktop";
    {
        std::ofstream f(p1);
        f << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=Test Valid App\n"
          << "Exec=echo \"hello world\" %f\n"
          << "TryExec=ls\n"
          << "OnlyShowIn=BRO;GNOME;\n"
          << "Hidden=false\n";
    }

    AutostartEntry e1;
    std::string err;
    assert(AutostartManager::parse_desktop_file(p1, &e1, &err));
    assert(e1.name == "Test Valid App");
    assert(e1.type == "Application");
    assert(e1.try_exec == "ls");
    assert(!e1.hidden);
    assert(e1.only_show_in.size() == 2);

    AutostartFilter filter_bro{ .current_desktop = "BRO" };
    std::string reason;
    assert(AutostartManager::should_autostart(e1, filter_bro, &reason));

    AutostartFilter filter_kde{ .current_desktop = "KDE" };
    assert(!AutostartManager::should_autostart(e1, filter_kde, &reason));
    assert(!reason.empty());

    // 2. Hidden app
    fs::path p2 = temp_dir / "hidden_app.desktop";
    {
        std::ofstream f(p2);
        f << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=Hidden App\n"
          << "Exec=echo hi\n"
          << "Hidden=true\n";
    }
    AutostartEntry e2;
    assert(AutostartManager::parse_desktop_file(p2, &e2, &err));
    assert(e2.hidden);
    assert(!AutostartManager::should_autostart(e2, filter_bro, &reason));

    // 3. NotShowIn
    fs::path p3 = temp_dir / "not_show.desktop";
    {
        std::ofstream f(p3);
        f << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=NotShow App\n"
          << "Exec=echo hi\n"
          << "NotShowIn=BRO;XFCE;\n";
    }
    AutostartEntry e3;
    assert(AutostartManager::parse_desktop_file(p3, &e3, &err));
    assert(!AutostartManager::should_autostart(e3, filter_bro, &reason));
    assert(AutostartManager::should_autostart(e3, filter_kde, &reason));

    // 4. TryExec fail
    fs::path p4 = temp_dir / "try_exec_fail.desktop";
    {
        std::ofstream f(p4);
        f << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=Fail App\n"
          << "Exec=echo hi\n"
          << "TryExec=nonexistent_binary_xyz_12345\n";
    }
    AutostartEntry e4;
    assert(AutostartManager::parse_desktop_file(p4, &e4, &err));
    assert(!AutostartManager::should_autostart(e4, filter_bro, &reason));

    // 5. AutostartCondition
    fs::path p5 = temp_dir / "condition.desktop";
    {
        std::ofstream f(p5);
        f << "[Desktop Entry]\n"
          << "Type=Application\n"
          << "Name=Cond App\n"
          << "Exec=echo hi\n"
          << "AutostartCondition=BRO if-session BRO\n";
    }
    AutostartEntry e5;
    assert(AutostartManager::parse_desktop_file(p5, &e5, &err));
    assert(AutostartManager::should_autostart(e5, filter_bro, &reason));
    assert(!AutostartManager::should_autostart(e5, filter_kde, &reason));

    // 6. Test exec expansion
    auto argv = AutostartManager::expand_exec_arguments("app --flag \"quoted argument\" 'single quote' %f %u %%");
    assert(argv.size() == 5);
    assert(argv[0] == "app");
    assert(argv[1] == "--flag");
    assert(argv[2] == "quoted argument");
    assert(argv[3] == "single quote");
    assert(argv[4] == "%");

    // 7. Test discovery and priority overriding
    fs::path dir_user = temp_dir / "user_autostart";
    fs::path dir_sys = temp_dir / "sys_autostart";
    fs::create_directories(dir_user);
    fs::create_directories(dir_sys);

    {
        std::ofstream f(dir_sys / "item.desktop");
        f << "[Desktop Entry]\nType=Application\nName=Sys Item\nExec=true\n";
    }
    {
        std::ofstream f(dir_user / "item.desktop");
        f << "[Desktop Entry]\nType=Application\nName=User Override Item\nExec=true\n";
    }

    auto discovered = AutostartManager::discover({dir_user, dir_sys}, filter_bro);
    assert(discovered.size() == 1);
    assert(discovered[0].name == "User Override Item");

    // 8. Test launch via ForkExec
    AutostartEntry launch_target;
    launch_target.id = "test-true";
    launch_target.exec = "true";
    auto launch_res = AutostartManager::launch(launch_target, LaunchMode::ForkExec);
    assert(launch_res.success);
    assert(launch_res.pid > 0);

    int status = 0;
    waitpid(launch_res.pid, &status, 0);
    assert(WIFEXITED(status));
    assert(WEXITSTATUS(status) == 0);

    // Clean up
    fs::remove_all(temp_dir);

    std::cout << "test_autostart PASSED\n";
    return 0;
}
