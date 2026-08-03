#include "argyll.h"

#include <cstdlib>
#include <cstring>
#include <array>
#include <sstream>

#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>
#include <errno.h>

namespace jcolour {
namespace argyll {

// ---- path detection --------------------------------------------------------

static const char* const SEARCH_DIRS[] = {
    "/opt/homebrew/bin",
    "/usr/local/bin",
    nullptr
};

std::string find_tool(const std::string& name) {
    // Fixed search directories
    for (int i = 0; SEARCH_DIRS[i]; ++i) {
        std::string path = std::string(SEARCH_DIRS[i]) + "/" + name;
        if (access(path.c_str(), X_OK) == 0)
            return path;
    }

    // ~/bin
    const char* home = getenv("HOME");
    if (home) {
        std::string path = std::string(home) + "/bin/" + name;
        if (access(path.c_str(), X_OK) == 0)
            return path;
    }

    // Walk PATH
    const char* path_env = getenv("PATH");
    if (path_env) {
        std::string dirs(path_env);
        std::istringstream ss(dirs);
        std::string dir;
        while (std::getline(ss, dir, ':')) {
            if (dir.empty()) continue;
            std::string full = dir + "/" + name;
            if (access(full.c_str(), X_OK) == 0)
                return full;
        }
    }

    return "";
}

// ---- subprocess ------------------------------------------------------------

// Read all available bytes from fd into buf without blocking.
static void drain(int fd, std::string& buf) {
    char tmp[4096];
    ssize_t n;
    while ((n = read(fd, tmp, sizeof(tmp))) > 0)
        buf.append(tmp, static_cast<size_t>(n));
}

RunResult run(const std::string& tool_path,
              const std::vector<std::string>& args,
              int timeout_sec) {
    RunResult result;

    // Build argv
    std::vector<const char*> argv;
    argv.push_back(tool_path.c_str());
    for (const auto& a : args)
        argv.push_back(a.c_str());
    argv.push_back(nullptr);

    // Create pipes for stdout and stderr
    int out_pipe[2], err_pipe[2];
    if (pipe(out_pipe) != 0 || pipe(err_pipe) != 0) {
        result.exit_code = -1;
        result.stderr_out = "pipe() failed";
        return result;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        result.exit_code = -1;
        result.stderr_out = "fork() failed";
        return result;
    }

    if (pid == 0) {
        // Child: redirect stdout/stderr to pipes
        dup2(out_pipe[1], STDOUT_FILENO);
        dup2(err_pipe[1], STDERR_FILENO);
        close(out_pipe[0]); close(out_pipe[1]);
        close(err_pipe[0]); close(err_pipe[1]);
        execv(tool_path.c_str(), const_cast<char* const*>(argv.data()));
        _exit(127);
    }

    // Parent: close write ends
    close(out_pipe[1]);
    close(err_pipe[1]);

    // Set read ends non-blocking
    fcntl(out_pipe[0], F_SETFL, O_NONBLOCK);
    fcntl(err_pipe[0], F_SETFL, O_NONBLOCK);

    // Read until both pipes close or timeout
    struct timeval deadline{};
    if (timeout_sec > 0) {
        deadline.tv_sec  = timeout_sec;
        deadline.tv_usec = 0;
    }

    bool out_open = true, err_open = true;
    while (out_open || err_open) {
        fd_set rfds;
        FD_ZERO(&rfds);
        int maxfd = -1;
        if (out_open) { FD_SET(out_pipe[0], &rfds); maxfd = std::max(maxfd, out_pipe[0]); }
        if (err_open) { FD_SET(err_pipe[0], &rfds); maxfd = std::max(maxfd, err_pipe[0]); }

        struct timeval tv = deadline;
        int sel = select(maxfd + 1, &rfds, nullptr, nullptr,
                         timeout_sec > 0 ? &tv : nullptr);

        if (sel == 0) {
            // Timeout
            kill(pid, SIGKILL);
            result.timed_out = true;
            break;
        }
        if (sel < 0) break;

        if (out_open && FD_ISSET(out_pipe[0], &rfds)) {
            char tmp[4096];
            ssize_t n = read(out_pipe[0], tmp, sizeof(tmp));
            if (n > 0) result.stdout_out.append(tmp, static_cast<size_t>(n));
            else       out_open = false;
        }
        if (err_open && FD_ISSET(err_pipe[0], &rfds)) {
            char tmp[4096];
            ssize_t n = read(err_pipe[0], tmp, sizeof(tmp));
            if (n > 0) result.stderr_out.append(tmp, static_cast<size_t>(n));
            else       err_open = false;
        }
    }

    // Drain any remaining bytes
    drain(out_pipe[0], result.stdout_out);
    drain(err_pipe[0], result.stderr_out);
    close(out_pipe[0]);
    close(err_pipe[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return result;
}

// ---- version ---------------------------------------------------------------

std::string version() {
    std::string path = find_tool("targen");
    if (path.empty()) return "";

    // targen prints version to stderr with no args; exit code is non-zero
    auto r = run(path, {}, 10);
    // Version line looks like: "Argyll_V2.3.1 ..."
    auto search = [](const std::string& s) -> std::string {
        auto pos = s.find("Argyll_V");
        if (pos == std::string::npos) return "";
        auto end = s.find_first_of(" \t\r\n", pos + 8);
        std::string token = s.substr(pos + 8, end == std::string::npos ? std::string::npos : end - pos - 8);
        return token;
    };
    std::string v = search(r.stdout_out);
    if (v.empty()) v = search(r.stderr_out);
    return v;
}

// ---- high-level wrappers ---------------------------------------------------

RunResult targen(const std::string& output_base,
                 const std::vector<std::string>& extra_args,
                 int timeout_sec) {
    std::string path = find_tool("targen");
    if (path.empty()) {
        RunResult r;
        r.exit_code  = -2;
        r.stderr_out = "targen not found";
        return r;
    }
    std::vector<std::string> args(extra_args);
    args.push_back(output_base);
    return run(path, args, timeout_sec);
}

RunResult colprof(const std::string& output_base,
                  const std::vector<std::string>& extra_args,
                  int timeout_sec) {
    std::string path = find_tool("colprof");
    if (path.empty()) {
        RunResult r;
        r.exit_code  = -2;
        r.stderr_out = "colprof not found";
        return r;
    }
    std::vector<std::string> args(extra_args);
    args.push_back(output_base);
    return run(path, args, timeout_sec);
}

RunResult profcheck(const std::string& profile,
                    const std::string& ti3,
                    const std::vector<std::string>& extra_args,
                    int timeout_sec) {
    std::string path = find_tool("profcheck");
    if (path.empty()) {
        RunResult r;
        r.exit_code  = -2;
        r.stderr_out = "profcheck not found";
        return r;
    }
    std::vector<std::string> args(extra_args);
    args.push_back(profile);
    args.push_back(ti3);
    return run(path, args, timeout_sec);
}

}  // namespace argyll
}  // namespace jcolour
