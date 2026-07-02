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
fs::path heroDiskPath(int64_t appid) { return coverDir() / (std::to_string(appid) + "_hero.jpg"); }

std::optional<std::string> readFile(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) return std::nullopt;
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.empty()) return std::nullopt;
    return data;
}

void saveFile(const fs::path& p, const std::string& data) {
    std::error_code ec;
    fs::create_directories(coverDir(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (f) f.write(data.data(), (std::streamsize)data.size());
}
void saveDisk(int64_t appid, const std::string& data) { saveFile(diskPath(appid), data); }

// Steam's cached art for an installed app, under a KNOWN filename only.
// (Never glob hashed names — that grabs low-res logos; see the memory.)
std::optional<std::string> localSteamAsset(int64_t appid, const char* asset) {
    auto root = steam::steamRoot();
    if (!root) return std::nullopt;
    fs::path cache = *root / "appcache" / "librarycache";
    for (const fs::path& cand : {cache / std::to_string(appid) / asset,
                                 cache / (std::to_string(appid) + "_" + asset)}) {
        if (auto data = readFile(cand)) return data;
    }
    return std::nullopt;
}
std::optional<std::string> localSteam(int64_t appid) {
    return localSteamAsset(appid, "library_600x900.jpg");
}

// LAST-RESORT local source for apps with no store art anywhere (hidden demo/beta
// appids: appdetails success=false, CDN 404). Steam still ships their portrait
// into librarycache/<appid>/ under a HASHED filename we can't predict — so scan
// the folder but accept a file only if its decoded dimensions are an actual
// portrait capsule (2:3-ish). This is what makes the scan safe: the old blind
// glob regressed Apex by grabbing a low-res logo (see cover-art-resolution memory);
// logos/heroes/headers are wide or square and can't pass this check. Runs after
// every other layer has failed, so it can never override better art.
std::optional<std::string> localSteamHashedPortrait(int64_t appid) {
    auto root = steam::steamRoot();
    if (!root) return std::nullopt;
    fs::path dir = *root / "appcache" / "librarycache" / std::to_string(appid);
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return std::nullopt;
    std::optional<std::string> best;
    long long bestArea = 0;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.file_size(ec) > 8u * 1024 * 1024) continue;   // sanity cap
        auto data = readFile(entry.path());
        if (!data) continue;
        auto size = imageSize(*data);
        if (!size) continue;
        auto [w, h] = *size;
        double aspect = h > 0 ? double(w) / h : 0;
        if (h < 400 || aspect < 0.6 || aspect > 0.75) continue;  // portrait capsules only
        long long area = 1LL * w * h;
        if (area > bestArea) { bestArea = area; best = std::move(data); }
    }
    return best;
}

std::optional<std::string> legacyCdnAssets(int64_t appid, std::initializer_list<const char*> assets) {
    for (const char* asset : assets) {
        std::string url = "https://cdn.cloudflare.steamstatic.com/steam/apps/" +
                          std::to_string(appid) + "/" + asset;
        if (auto data = http::get(url)) return data;
    }
    return std::nullopt;
}
std::optional<std::string> legacyCdn(int64_t appid) {
    return legacyCdnAssets(appid, {"library_600x900.jpg", "header.jpg", "capsule_616x353.jpg"});
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

// Pixel dimensions (w,h) from raw PNG (IHDR) or JPEG (SOF marker) bytes.
std::optional<std::pair<int, int>> imageSize(const std::string& d) {
    auto u8 = [&](size_t i) { return static_cast<unsigned char>(d[i]); };
    if (d.size() >= 24 && d.compare(1, 3, "PNG") == 0) {
        int w = (u8(16) << 24) | (u8(17) << 16) | (u8(18) << 8) | u8(19);
        int h = (u8(20) << 24) | (u8(21) << 16) | (u8(22) << 8) | u8(23);
        return std::make_pair(w, h);
    }
    if (d.size() > 4 && u8(0) == 0xFF && u8(1) == 0xD8) {   // JPEG
        size_t i = 2;
        while (i + 9 < d.size()) {
            if (u8(i) != 0xFF) { ++i; continue; }
            int m = u8(i + 1);
            if (m == 0xFF) { ++i; continue; }
            if (m == 0x01 || (m >= 0xD0 && m <= 0xD9)) { i += 2; continue; }  // no payload
            size_t len = (static_cast<size_t>(u8(i + 2)) << 8) | u8(i + 3);
            bool sof = (m >= 0xC0 && m <= 0xCF) && m != 0xC4 && m != 0xC8 && m != 0xCC;
            if (sof) {
                int h = (u8(i + 5) << 8) | u8(i + 6);
                int w = (u8(i + 7) << 8) | u8(i + 8);
                return std::make_pair(w, h);
            }
            i += 2 + len;
        }
    }
    return std::nullopt;
}

std::optional<std::string> coverBytes(int64_t appid, bool allowNetwork) {
    if (auto local = localSteam(appid)) return local;      // Steam's current art first
    if (auto cached = readFile(diskPath(appid))) return cached;
    if (allowNetwork) {
        for (auto source : {legacyCdn, appDetails}) {
            if (auto data = source(appid)) { saveDisk(appid, *data); return data; }
        }
    }
    // Nothing anywhere else (hidden demos/betas) — dimension-checked local scan.
    return localSteamHashedPortrait(appid);
}

std::optional<std::string> heroBytes(int64_t appid, bool allowNetwork) {
    // Steam's current hero first (seasonal art), then our network cache.
    if (auto local = localSteamAsset(appid, "library_hero.jpg")) return local;
    if (auto cached = readFile(heroDiskPath(appid))) return cached;
    if (allowNetwork) {
        // Flat CDN still serves older apps' hero; header.jpg / appdetails
        // header_image are the wide fallbacks for the rest.
        if (auto data = legacyCdnAssets(appid, {"library_hero.jpg", "header.jpg"})) {
            saveFile(heroDiskPath(appid), *data);
            return data;
        }
        if (auto data = appDetails(appid)) {
            saveFile(heroDiskPath(appid), *data);
            return data;
        }
    }
    return std::nullopt;
}

long long cacheSizeBytes() {
    long long total = 0;
    std::error_code ec;
    if (!fs::is_directory(coverDir(), ec)) return 0;
    for (const auto& entry : fs::directory_iterator(coverDir(), ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec)) total += (long long)entry.file_size(ec);
    }
    return total;
}

int clearCache() {
    int removed = 0;
    std::error_code ec;
    if (!fs::is_directory(coverDir(), ec)) return 0;
    for (const auto& entry : fs::directory_iterator(coverDir(), ec)) {
        if (ec) break;
        if (entry.is_regular_file(ec) && fs::remove(entry.path(), ec)) ++removed;
    }
    return removed;
}

}  // namespace ss::covers
