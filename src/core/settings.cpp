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

std::string heroMode() {
    json::Value s = load();
    if (const json::Value* v = s.get("hero_mode"))
        if (v->isString() && v->str == "random") return "random";
    return "last";
}

void setHeroMode(const std::string& mode) {
    json::Value s = load();
    s.set("hero_mode", json::Value::makeString(mode == "random" ? "random" : "last"));
    save(s);
}

bool offlineDefault() {
    json::Value s = load();
    if (const json::Value* v = s.get("offline_default"))
        if (v->type == json::Value::Type::Bool) return v->b;
    return false;
}

void setOfflineDefault(bool value) {
    json::Value s = load();
    s.set("offline_default", json::Value::makeBool(value));
    save(s);
}

std::map<std::string, long long> launchHistory() {
    std::map<std::string, long long> out;
    json::Value s = load();
    if (const json::Value* h = s.get("launch_history"))
        if (h->isObject())
            for (const auto& kv : h->obj)
                if (kv.second.type == json::Value::Type::Number && kv.second.num > 0)
                    out[kv.first] = (long long)kv.second.num;
    return out;
}

long long lastLaunched(const std::string& gameKey) {
    auto h = launchHistory();
    auto it = h.find(gameKey);
    return it == h.end() ? 0 : it->second;
}

void recordLaunch(const std::string& gameKey, long long unixSeconds) {
    if (gameKey.empty() || unixSeconds <= 0) return;
    json::Value s = load();
    if (!s.get("launch_history") || !s.get("launch_history")->isObject())
        s.set("launch_history", json::Value::makeObject());
    s.get("launch_history")->set(gameKey, json::Value::makeNumber((double)unixSeconds));
    save(s);
}

}  // namespace ss::settings
