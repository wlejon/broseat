#include "autostart/autostart_launcher.h"
#include "common/utils.h"
#include "dbus/bus.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace broseat {

namespace {

LaunchResult make_error_result(std::string error) {
    LaunchResult res;
    res.success = false;
    res.pid = 0;
    res.unit_name = "";
    res.error = std::move(error);
    return res;
}

LaunchResult launch_fork_exec(const AutostartEntry& entry, const std::vector<std::string>& argv) {
    if (argv.empty()) {
        return make_error_result("Empty argument list");
    }

    pid_t pid = fork();
    if (pid < 0) {
        return make_error_result(strerror(errno));
    }

    if (pid == 0) {
        // Child process
        setsid();

        // Reset signal mask
        sigset_t mask;
        sigemptyset(&mask);
        sigprocmask(SIG_SETMASK, &mask, nullptr);

        // Reset signals to default
        for (int sig = 1; sig < 32; ++sig) {
            if (sig != SIGKILL && sig != SIGSTOP) {
                signal(sig, SIG_DFL);
            }
        }

        // Redirect stdin to /dev/null
        int null_in = open("/dev/null", O_RDONLY);
        if (null_in >= 0) {
            dup2(null_in, STDIN_FILENO);
            close(null_in);
        }

        // Change working directory if specified
        if (!entry.working_dir.empty()) {
            [[maybe_unused]] int r = chdir(entry.working_dir.c_str());
        }

        std::vector<char*> c_argv;
        c_argv.reserve(argv.size() + 1);
        for (const auto& arg : argv) {
            c_argv.push_back(const_cast<char*>(arg.c_str()));
        }
        c_argv.push_back(nullptr);

        execvp(c_argv[0], c_argv.data());
        _exit(127);
    }

    LaunchResult res;
    res.success = true;
    res.pid = static_cast<uint32_t>(pid);
    res.unit_name = "";
    res.error = "";
    return res;
}

LaunchResult launch_systemd_run(const AutostartEntry& entry, const std::vector<std::string>& argv) {
    if (argv.empty()) {
        return make_error_result("Empty argument list");
    }

    std::string unit_name = "app-broseat-autostart-" + utils::sanitize_unit_name(entry.id) + ".scope";

    std::vector<std::string> run_argv;
    run_argv.push_back("systemd-run");
    run_argv.push_back("--user");
    run_argv.push_back("--scope");
    run_argv.push_back("--unit=" + unit_name);
    if (!entry.name.empty()) {
        run_argv.push_back("--description=" + entry.name);
    }
    for (const auto& arg : argv) {
        run_argv.push_back(arg);
    }

    auto res = launch_fork_exec(entry, run_argv);
    if (res.success) {
        res.unit_name = unit_name;
    }
    return res;
}

LaunchResult launch_systemd_transient(const AutostartEntry& entry, const std::vector<std::string>& argv) {
    if (argv.empty()) {
        return make_error_result("Empty argument list");
    }

    std::string dbus_err;
    auto user_bus = dbus::Bus::open_user(&dbus_err);
    if (!user_bus) {
        return make_error_result("User bus not available: " + dbus_err);
    }

    std::string unit_name = "app-broseat-autostart-" + utils::sanitize_unit_name(entry.id) + "@autostart.service";

    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(
        user_bus->raw(), &m,
        "org.freedesktop.systemd1",
        "/org/freedesktop/systemd1",
        "org.freedesktop.systemd1.Manager",
        "StartTransientUnit");
    if (r < 0) {
        return make_error_result(strerror(-r));
    }

    // signature: ssa(sv)a(sa(sv))
    sd_bus_message_append(m, "ss", unit_name.c_str(), "fail");

    // properties: a(sv)
    sd_bus_message_open_container(m, 'a', "(sv)");

    // 1. Description
    std::string desc = entry.name.empty() ? entry.id : entry.name;
    sd_bus_message_open_container(m, 'r', "sv");
    sd_bus_message_append(m, "s", "Description");
    sd_bus_message_open_container(m, 'v', "s");
    sd_bus_message_append(m, "s", desc.c_str());
    sd_bus_message_close_container(m); // v
    sd_bus_message_close_container(m); // r

    // 2. ExecStart: a(sasb)
    sd_bus_message_open_container(m, 'r', "sv");
    sd_bus_message_append(m, "s", "ExecStart");
    sd_bus_message_open_container(m, 'v', "a(sasb)");
    sd_bus_message_open_container(m, 'a', "(sasb)");
    sd_bus_message_open_container(m, 'r', "sasb");

    // binary path
    auto bin_path = utils::find_in_path(argv[0]);
    std::string exec_path = bin_path ? *bin_path : argv[0];
    sd_bus_message_append(m, "s", exec_path.c_str());

    // argv array
    sd_bus_message_open_container(m, 'a', "s");
    for (const auto& arg : argv) {
        sd_bus_message_append(m, "s", arg.c_str());
    }
    sd_bus_message_close_container(m); // as
    sd_bus_message_append(m, "b", 0);  // clean_exec_flag = false
    sd_bus_message_close_container(m); // r (sasb)
    sd_bus_message_close_container(m); // a(sasb)
    sd_bus_message_close_container(m); // v
    sd_bus_message_close_container(m); // r (sv)

    // 3. WorkingDirectory if set
    if (!entry.working_dir.empty()) {
        sd_bus_message_open_container(m, 'r', "sv");
        sd_bus_message_append(m, "s", "WorkingDirectory");
        sd_bus_message_open_container(m, 'v', "s");
        sd_bus_message_append(m, "s", entry.working_dir.c_str());
        sd_bus_message_close_container(m);
        sd_bus_message_close_container(m);
    }

    sd_bus_message_close_container(m); // a(sv)

    // aux units: a(sa(sv)) -> empty
    sd_bus_message_open_container(m, 'a', "(sa(sv))");
    sd_bus_message_close_container(m);

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    r = sd_bus_call(user_bus->raw(), m, 0, &err, &reply);
    sd_bus_message_unref(m);

    if (r < 0) {
        std::string err_msg = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return make_error_result(err_msg);
    }

    if (reply) sd_bus_message_unref(reply);
    sd_bus_error_free(&err);

    LaunchResult res;
    res.success = true;
    res.pid = 0;
    res.unit_name = unit_name;
    res.error = "";
    return res;
}

}  // namespace

LaunchResult launch_entry(const AutostartEntry& entry, LaunchMode mode) {
    auto argv = expand_exec_line(entry.exec);
    if (argv.empty()) {
        return make_error_result("Failed to parse exec line");
    }

    if (mode == LaunchMode::SystemdTransient) {
        return launch_systemd_transient(entry, argv);
    }

    if (mode == LaunchMode::SystemdRun) {
        return launch_systemd_run(entry, argv);
    }

    if (mode == LaunchMode::ForkExec) {
        return launch_fork_exec(entry, argv);
    }

    // Auto mode: try SystemdTransient -> SystemdRun -> ForkExec
    auto res = launch_systemd_transient(entry, argv);
    if (res.success) return res;

    res = launch_systemd_run(entry, argv);
    if (res.success) return res;

    return launch_fork_exec(entry, argv);
}

}  // namespace broseat
