#include "appdata.h"

#include "platform.h"

namespace ss::appdata {

namespace {
fs::path g_override;
}

void setDir(const fs::path& d) { g_override = d; }

fs::path dir() {
    fs::path d;
    if (!g_override.empty()) d = g_override;
    else if (auto env = platform::getEnv("SS_DATA_DIR")) d = *env;
    else d = fs::current_path() / "data";
    std::error_code ec;
    fs::create_directories(d, ec);
    return d;
}

}  // namespace ss::appdata
