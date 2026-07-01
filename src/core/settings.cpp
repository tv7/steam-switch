#include "settings.h"

#include "appdata.h"
#include "json.h"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace ss::settings {

namespace fs = std::filesystem;

namespace {

fs::path settingsFile() { return appdata::dir() / "settings.json"; }

// Load settings.json as an object, or an empty object on any error (parity with
// Python's try/except returning {}).
json::Value load() {
    std::error_code ec;
    if (fs::exists(settingsFile(), ec)) {
        std::ifstream f(settingsFile(), std::ios::binary);
        if (f) {
            std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
            bool ok = false;
            json::Value v = json::parse(text, &ok);
            if (ok && v.isObject()) return v;
        }
    }
    return json::Value::makeObject();
}

void save(const json::Value& v) {
    std::error_code ec;
    fs::create_directories(appdata::dir(), ec);
    std::ofstream f(settingsFile(), std::ios::binary | std::ios::trunc);
    if (f) f << json::dump(v, 2);  // best-effort; ignore I/O failure like Python
}

}  // namespace

std::string language() {
    json::Value s = load();
    if (const json::Value* v = s.get("language"))
        if (v->isString() && !v->str.empty()) return v->str;
    return "en";
}

void setLanguage(const std::string& lang) {
    json::Value s = load();
    s.set("language", json::Value::makeString(lang));
    save(s);
}

bool onboarded() {
    json::Value s = load();
    if (const json::Value* v = s.get("onboarded"))
        if (v->type == json::Value::Type::Bool) return v->b;
    return false;
}

void setOnboarded(bool value) {
    json::Value s = load();
    s.set("onboarded", json::Value::makeBool(value));
    save(s);
}

}  // namespace ss::settings
