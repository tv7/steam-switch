#include "covers.h"

#include "appdata.h"
#include "http.h"
#include "json.h"
#include "steam_paths.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace ss::covers {

namespace fs = std::filesystem;

namespace {

fs::path coverDir() { return appdata::dir() / "covers"; }
fs::path diskPath(int64_t appid) { return coverDir() / (std::to_string(appid) + ".jpg"); }

std::optional<std::string> readFile(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) return std::nullopt;
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.empty()) return std::nullopt;
    return data;
}

void saveDisk(int64_t appid, const std::string& data) {
    std::error_code ec;
    fs::create_directories(coverDir(), ec);
    std::ofstream f(diskPath(appid), std::ios::binary | std::ios::trunc);
    if (f) f.write(data.data(), (std::streamsize)data.size());
}

// Steam's cached portrait for an installed app, under its KNOWN filename only.
// (Never glob hashed names — that grabs low-res logos; see the memory.)
std::optional<std::string> localSteam(int64_t appid) {
    auto root = steam::steamRoot();
    if (!root) return std::nullopt;
    fs::path cache = *root / "appcache" / "librarycache";
    for (const fs::path& cand : {cache / std::to_string(appid) / "library_600x900.jpg",
                                 cache / (std::to_string(appid) + "_library_600x900.jpg")}) {
        if (auto data = readFile(cand)) return data;
    }
    return std::nullopt;
}

std::optional<std::string> legacyCdn(int64_t appid) {
    const char* assets[] = {"library_600x900.jpg", "header.jpg", "capsule_616x353.jpg"};
    for (const char* asset : assets) {
        std::string url = "https://cdn.cloudflare.steamstatic.com/steam/apps/" +
                          std::to_string(appid) + "/" + asset;
        if (auto data = http::get(url)) return data;
    }
    return std::nullopt;
}

// Ask the store API for the live (hashed) art URL and fetch it.
std::optional<std::string> appDetails(int64_t appid) {
    auto raw = http::get("https://store.steampowered.com/api/appdetails?appids=" +
                         std::to_string(appid) + "&l=en");
    if (!raw) return std::nullopt;
    bool ok = false;
    json::Value v = json::parse(*raw, &ok);
    if (!ok) return std::nullopt;
    const json::Value* entry = v.get(std::to_string(appid));
    if (!entry) return std::nullopt;
    const json::Value* success = entry->get("success");
    if (!success || !success->asBool()) return std::nullopt;
    const json::Value* data = entry->get("data");
    if (!data) return std::nullopt;
    for (const char* field : {"header_image", "capsule_image", "capsule_imagev5"}) {
        if (const json::Value* u = data->get(field)) {
            if (u->isString() && !u->str.empty())
                if (auto img = http::get(u->str)) return img;
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> coverBytes(int64_t appid, bool allowNetwork) {
    if (auto local = localSteam(appid)) return local;      // Steam's current art first
    if (auto cached = readFile(diskPath(appid))) return cached;
    if (!allowNetwork) return std::nullopt;
    for (auto source : {legacyCdn, appDetails}) {
        if (auto data = source(appid)) { saveDisk(appid, *data); return data; }
    }
    return std::nullopt;
}

}  // namespace ss::covers
