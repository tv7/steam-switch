// POSIX (Linux/macOS) implementation of the platform layer. Mirrors the Python
// stubs: there's no Windows registry, so the registry calls are no-ops; process
// control uses pgrep/pkill; URIs open via xdg-open/open. The Linux account-switch
// itself is still a follow-up (Next steps #1) — this only provides the primitives.

#if !defined(_WIN32)

#include "platform.h"

#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

namespace ss::platform {

std::optional<std::string> regReadString(Hive, const std::string&, const std::string&) { return std::nullopt; }
std::optional<uint32_t>    regReadDword (Hive, const std::string&, const std::string&) { return std::nullopt; }
bool regWriteString(Hive, const std::string&, const std::string&, const std::string&) { return false; }
bool regWriteDword (Hive, const std::string&, const std::string&, uint32_t) { return false; }
bool regDeleteValue(Hive, const std::string&, const std::string&) { return false; }
std::vector<std::string> regSubKeys(Hive, const std::string&) { return {}; }

static int runv(const std::string& cmd) { return std::system(cmd.c_str()); }

bool processRunning(const std::string& imageName) {
    // pgrep -x <name>  (strip a trailing .exe if a Windows-style name leaks in)
    std::string name = imageName;
    auto dot = name.rfind(".exe");
    if (dot != std::string::npos && dot == name.size() - 4) name = name.substr(0, dot);
    return runv("pgrep -x '" + name + "' >/dev/null 2>&1") == 0;
}

void forceKill(const std::string& imageName) {
    std::string name = imageName;
    auto dot = name.rfind(".exe");
    if (dot != std::string::npos && dot == name.size() - 4) name = name.substr(0, dot);
    runv("pkill -9 -x '" + name + "' >/dev/null 2>&1");
}

static std::string join(const std::vector<std::string>& argv) {
    std::string s;
    for (size_t i = 0; i < argv.size(); ++i) {
        if (i) s += ' ';
        s += "'" + argv[i] + "'";
    }
    return s;
}

void runWait(const std::vector<std::string>& argv) {
    if (argv.empty()) return;
    runv(join(argv) + " >/dev/null 2>&1");
}

void spawnDetached(const std::vector<std::string>& argv) {
    if (argv.empty()) return;
    runv(join(argv) + " >/dev/null 2>&1 &");
}

void spawnDetached(const std::vector<std::string>& argv, const std::string& workingDir) {
    if (argv.empty()) return;
    std::string cmd = workingDir.empty() ? join(argv)
                                         : "cd '" + workingDir + "' && " + join(argv);
    runv("( " + cmd + " ) >/dev/null 2>&1 &");
}

void openUri(const std::string& uri) {
#if defined(__APPLE__)
    runv("open '" + uri + "' >/dev/null 2>&1 &");
#else
    runv("xdg-open '" + uri + "' >/dev/null 2>&1 &");
#endif
}

bool steamWindowPresent() { return false; }  // Windows-only signal

std::optional<std::string> getEnv(const std::string& name) {
    const char* v = std::getenv(name.c_str());
    if (v && *v) return std::string(v);
    return std::nullopt;
}

std::string homeDir() {
    if (const char* h = std::getenv("HOME")) return h;
    if (struct passwd* pw = getpwuid(getuid())) return pw->pw_dir;
    return "";
}

}  // namespace ss::platform

#endif  // !_WIN32
