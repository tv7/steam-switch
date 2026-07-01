#include "store_covers.h"

#include "appdata.h"
#include "epic_games.h"
#include "http.h"
#include "json.h"

#include <filesystem>
#include <fstream>
#include <iterator>

namespace ss::store_covers {

namespace fs = std::filesystem;

namespace {

fs::path coverDir() { return appdata::dir() / "covers"; }
fs::path diskPath(int64_t cacheId) { return coverDir() / (std::to_string(cacheId) + ".jpg"); }

std::optional<std::string> readFile(const fs::path& p) {
    std::error_code ec;
    if (!fs::exists(p, ec)) return std::nullopt;
    std::ifstream f(p, std::ios::binary);
    if (!f) return std::nullopt;
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.empty()) return std::nullopt;
    return data;
}

void saveDisk(int64_t cacheId, const std::string& data) {
    std::error_code ec;
    fs::create_directories(coverDir(), ec);
    std::ofstream f(diskPath(cacheId), std::ios::binary | std::ios::trunc);
    if (f) f.write(data.data(), (std::streamsize)data.size());
}

std::string getStr(const json::Value& o, const std::string& key) {
    if (const json::Value* v = o.get(key)) return v->asString();
    return "";
}

// ---- Epic: the launcher's local catalog cache -------------------------------

fs::path catCachePath() {
    // Sibling of the Manifests dir ($SS_EPIC_MANIFESTS override applies to both).
    return epic::manifestsDir().parent_path() / "Catalog" / "catcache.bin";
}

// ---- GOG: public products API ------------------------------------------------

// Absolutise GOG's protocol-relative image URLs ("//images-N.gog.com/…").
std::string absolutise(std::string url) {
    if (url.rfind("//", 0) == 0) url = "https:" + url;
    return url;
}

std::optional<std::string> gogCover(const std::string& productId) {
    if (productId.empty()) return std::nullopt;
    // v2 games API: _links.boxArtImage.href is the vertical box art (grid-shaped).
    if (auto raw = http::get("https://api.gog.com/v2/games/" + productId)) {
        bool ok = false;
        json::Value v = json::parse(*raw, &ok);
        if (ok) {
            if (const json::Value* links = v.get("_links"))
                if (const json::Value* box = links->get("boxArtImage")) {
                    std::string url = getStr(*box, "href");
                    if (!url.empty())
                        if (auto img = http::get(absolutise(url))) return img;
                }
        }
    }
    // Fallback: the v1 products API's images block (logo2x is landscape but real).
    if (auto raw = http::get("https://api.gog.com/products/" + productId + "?expand=images")) {
        bool ok = false;
        json::Value v = json::parse(*raw, &ok);
        if (ok) {
            if (const json::Value* images = v.get("images"))
                for (const char* field : {"logo2x", "logo"}) {
                    std::string url = getStr(*images, field);
                    if (!url.empty())
                        if (auto img = http::get(absolutise(url))) return img;
                }
        }
    }
    return std::nullopt;
}

// ---- Xbox: MS Store display catalog -------------------------------------------

// The public display catalog serves each product's real store art by StoreId.
// Prefer the portrait Poster (grid-shaped, 1440x2160), then BoxArt (square).
std::optional<std::string> xboxCatalogCover(const std::string& storeId) {
    if (storeId.empty()) return std::nullopt;
    auto raw = http::get("https://displaycatalog.mp.microsoft.com/v7.0/products?bigIds=" +
                         storeId + "&market=US&languages=en-us");
    if (!raw) return std::nullopt;
    bool ok = false;
    json::Value v = json::parse(*raw, &ok);
    if (!ok) return std::nullopt;
    const json::Value* products = v.get("Products");
    if (!products || !products->isArray() || products->arr.empty()) return std::nullopt;
    const json::Value* props = products->arr[0].get("LocalizedProperties");
    if (!props || !props->isArray() || props->arr.empty()) return std::nullopt;
    const json::Value* images = props->arr[0].get("Images");
    if (!images || !images->isArray()) return std::nullopt;
    for (const char* want : {"Poster", "BoxArt", "FeaturePromotionalSquareArt", "Logo"}) {
        for (const auto& img : images->arr)
            if (getStr(img, "ImagePurpose") == want) {
                std::string url = getStr(img, "Uri");
                if (url.empty()) continue;
                // The store CDN honours ?w= — cap the download (Posters are 2160 tall).
                if (auto data = http::get(absolutise(url) + "?w=720")) return data;
                if (auto data = http::get(absolutise(url))) return data;
            }
    }
    return std::nullopt;
}

}  // namespace

std::optional<std::string> base64Decode(const std::string& in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    out.reserve(in.size() / 4 * 3);
    int buf = 0, bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        int v = val(c);
        if (v < 0) return std::nullopt;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::string epicCoverUrl(const std::string& catalogItemId) {
    if (catalogItemId.empty()) return "";
    auto raw = readFile(catCachePath());
    if (!raw) return "";
    // catcache.bin is base64-wrapped JSON; some launcher builds write it raw.
    auto jsonText = base64Decode(*raw);
    return epicCoverUrlFromCatalog(jsonText ? *jsonText : *raw, catalogItemId);
}

std::string epicCoverUrlFromCatalog(const std::string& catalogJson,
                                    const std::string& catalogItemId) {
    bool ok = false;
    json::Value v = json::parse(catalogJson, &ok);
    if (!ok || !v.isArray()) return "";
    for (const auto& item : v.arr) {
        if (getStr(item, "id") != catalogItemId) continue;
        const json::Value* imgs = item.get("keyImages");
        if (!imgs || !imgs->isArray()) return "";
        // Portrait box first (matches the grid), then the landscape box.
        for (const char* want : {"DieselGameBoxTall", "DieselGameBox", "Thumbnail"}) {
            for (const auto& img : imgs->arr)
                if (getStr(img, "type") == want) {
                    std::string url = getStr(img, "url");
                    if (!url.empty()) return url;
                }
        }
        return "";
    }
    return "";
}

std::optional<std::string> coverBytes(Store store, const std::string& launchId,
                                      const std::string& coverHint, int64_t cacheId,
                                      bool allowNetwork) {
    if (auto cached = readFile(diskPath(cacheId))) return cached;

    std::optional<std::string> data;
    if (store == Store::Xbox) {
        // coverHint = "<StoreId>|<local logo path>" (either half may be empty).
        auto sep = coverHint.find('|');
        std::string storeId = sep == std::string::npos ? "" : coverHint.substr(0, sep);
        std::string logo = sep == std::string::npos ? coverHint : coverHint.substr(sep + 1);
        if (allowNetwork) data = xboxCatalogCover(storeId);   // real store box art
        if (data) saveDisk(cacheId, *data);
        // Fallback: the logo PNG shipped with the game — often generic/low-res
        // (UE logo, 300px StoreLogo), so only when the catalog gives nothing.
        // Local read: not disk-cached.
        if (!data && !logo.empty()) data = readFile(logo);
        return data;
    }

    if (!allowNetwork) return std::nullopt;
    if (store == Store::Epic) {
        std::string url = epicCoverUrl(coverHint);
        if (!url.empty()) data = http::get(url);
    } else if (store == Store::Gog) {
        data = gogCover(launchId);
    }
    if (data) saveDisk(cacheId, *data);
    return data;
}

}  // namespace ss::store_covers
