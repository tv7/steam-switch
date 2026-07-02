// "Run ORBIT on Windows startup" — a value named ORBIT under
// HKCU\Software\Microsoft\Windows\CurrentVersion\Run holding the quoted exe
// path. Windows-only (supported() is false elsewhere and the setters no-op).

#pragma once

#include <string>

namespace ss::autostart {

// True where the OS has a startup mechanism we implement (Windows registry).
bool supported();

// Whether the Run entry currently exists.
bool enabled();

// Create/remove the Run entry. `exePath` is the absolute path of the app exe
// (quoted for paths with spaces). Returns success; removing is idempotent.
bool setEnabled(bool on, const std::string& exePath);

// The exact value data written for `exePath` (exposed for tests).
std::string runValueData(const std::string& exePath);

}  // namespace ss::autostart
