// OS abstraction for the bits the Steam logic needs that std::filesystem doesn't
// cover: the Windows registry, process control, window enumeration and launching
// URIs. Ports the platform-specific halves of switcher.py / steam_paths.py.
//
// Implemented in platform_win.cpp (real) and platform_posix.cpp (Linux/macOS;
// registry-less, mirrors the Python stubs). The header is OS-neutral so core/
// compiles everywhere; the .cpp picked by the build provides the bodies.

#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace ss::platform {

// --- Registry (Windows only; POSIX impls return nullopt / false) -------------
enum class Hive { CurrentUser, LocalMachine };

std::optional<std::string> regReadString(Hive hive, const std::string& subkey, const std::string& name);
std::optional<uint32_t>    regReadDword (Hive hive, const std::string& subkey, const std::string& name);
bool regWriteString(Hive hive, const std::string& subkey, const std::string& name, const std::string& value);
bool regWriteDword (Hive hive, const std::string& subkey, const std::string& name, uint32_t value);

// Delete a value from a key. True if it was deleted OR was already absent
// (idempotent — used to turn off run-at-startup); false on real errors/POSIX.
bool regDeleteValue(Hive hive, const std::string& subkey, const std::string& name);

// Immediate subkey names under a registry key (for GOG's HKLM\...\GOG.com\Games\<id>
// enumeration). Empty on POSIX / missing key.
std::vector<std::string> regSubKeys(Hive hive, const std::string& subkey);

// --- Process control ---------------------------------------------------------
// True if a process with this image name is running (tasklist on Windows,
// pgrep -x on POSIX). `imageName` is e.g. "steam.exe" / "steam".
bool processRunning(const std::string& imageName);

// Hard-kill the process tree (taskkill /F /T on Windows, pkill -9 -x on POSIX).
void forceKill(const std::string& imageName);

// Run a command and wait for it to exit; output discarded. On Windows every
// child gets CREATE_NO_WINDOW so no console flashes (switcher._no_window()).
void runWait(const std::vector<std::string>& argv);

// Start a process detached (do not wait) — used to (re)launch Steam.
void spawnDetached(const std::vector<std::string>& argv);

// Same, but in a given working directory (GOG DRM-free exes often need their
// install dir as the cwd). Empty `workingDir` == inherit the parent's.
void spawnDetached(const std::vector<std::string>& argv, const std::string& workingDir);

// Open a URI/file with the OS handler (ShellExecute / open / xdg-open) — used
// for steam://rungameid/<id> and the Epic/Xbox launch protocols.
void openUri(const std::string& uri);

// True once Steam's main window is present (EnumWindows: class SDL_app / title
// "Steam"). Windows only; false elsewhere. Our 'Steam really up' offline signal.
bool steamWindowPresent();

// --- Environment -------------------------------------------------------------
std::optional<std::string> getEnv(const std::string& name);

// User home directory (for POSIX Steam paths / registry.vdf).
std::string homeDir();

}  // namespace ss::platform
