#include "autostart.h"

#include "platform.h"

namespace ss::autostart {

namespace {
const char* kRunKey = "Software\\Microsoft\\Windows\\CurrentVersion\\Run";
const char* kValueName = "ORBIT";
}  // namespace

bool supported() {
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
}

bool enabled() {
    return platform::regReadString(platform::Hive::CurrentUser, kRunKey, kValueName)
        .has_value();
}

std::string runValueData(const std::string& exePath) {
    return "\"" + exePath + "\"";
}

bool setEnabled(bool on, const std::string& exePath) {
    if (!supported()) return false;
    if (!on)
        return platform::regDeleteValue(platform::Hive::CurrentUser, kRunKey, kValueName);
    if (exePath.empty()) return false;
    return platform::regWriteString(platform::Hive::CurrentUser, kRunKey, kValueName,
                                    runValueData(exePath));
}

}  // namespace ss::autostart
