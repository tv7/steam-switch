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
fs::path heroDiskPath(int64_t cacheId) { return coverDir() / (std::to_string(cacheId) + "_hero.jpg"); }

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
void saveDisk(int64_t cacheId, const std::string& data) { saveFile(diskPath(cacheId), data); }

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

// `v2Link` picks the _links entry from the v2 games API (boxArtImage = vertical
// box art, backgroundImage = the wide hero); `v1Fields` are the v1 products
// images fallbacks in preference order.
std::optional<std::string> gogArt(const std::string& productId, const char* v2Link,
                                  std::initializer_list<const char*> v1Fields) {
    if (productId.empty()) return std::nullopt;
    if (auto raw = http::get("https://api.gog.com/v2/games/" + productId)) {
        bool ok = false;
        json::Value v = json::parse(*raw, &ok);
        if (ok) {
            if (const json::Value* links = v.get("_links"))
                if (const json::Value* box = links->get(v2Link)) {
                    std::string url = getStr(*box, "href");
                    if (!url.empty())
                        if (auto img = http::get(absolutise(url))) return img;
                }
        }
    }
    if (auto raw = http::get("https://api.gog.com/products/" + productId + "?expand=images")) {
        bool ok = false;
        json::Value v = json::parse(*raw, &ok);
        if (ok) {
            if (const json::Value* images = v.get("images"))
                for (const char* field : v1Fields) {
                    std::string url = getStr(*images, field);
                    if (!url.empty())
                        if (auto img = http::get(absolutise(url))) return img;
                }
        }
    }
    return std::nullopt;
}
std::optional<std::string> gogCover(const std::string& productId) {
    // logo2x is landscape but real art — an acceptable grid fallback.
    return gogArt(productId, "boxArtImage", {"logo2x", "logo"});
}
std::optional<std::string> gogHero(const std::string& productId) {
    return gogArt(productId, "backgroundImage", {"background", "logo2x"});
}

// ---- Xbox: MS Store display catalog -------------------------------------------

// The public display catalog serves each product's real store art by StoreId.
std::optional<std::string> xboxCatalogArt(const std::string& storeId,
                                          std::initializer_list<const char*> purposes,
                                          const char* widthCap) {
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
    for (const char* want : purposes) {
        for (const auto& img : images->arr)
            if (getStr(img, "ImagePurpose") == want) {
                std::string url = getStr(img, "Uri");
                if (url.empty()) continue;
                // The store CDN honours ?w= — cap the download (art is huge).
                if (auto data = http::get(absolutise(url) + widthCap)) return data;
                if (auto data = http::get(absolutise(url))) return data;
            }
    }
    return std::nullopt;
}
// Grid: prefer the portrait Poster (1440x2160), then BoxArt (square).
std::optional<std::string> xboxCatalogCover(const std::string& storeId) {
    return xboxCatalogArt(storeId, {"Poster", "BoxArt", "FeaturePromotionalSquareArt", "Logo"},
                          "?w=720");
}
// Hero: the wide store banners.
std::optional<std::string> xboxCatalogHero(const std::string& storeId) {
    return xboxCatalogArt(storeId, {"TitledHeroArt", "SuperHeroArt", "BrandedKeyArt"},
                          "?w=1280");
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

namespace {
std::string epicUrlFromCatalog(const std::string& catalogJson, const std::string& catalogItemId,
                               std::initializer_list<const char*> wants) {
    bool ok = false;
    json::Value v = json::parse(catalogJson, &ok);
    if (!ok || !v.isArray()) return "";
    for (const auto& item : v.arr) {
        if (getStr(item, "id") != catalogItemId) continue;
        const json::Value* imgs = item.get("keyImages");
        if (!imgs || !imgs->isArray()) return "";
        for (const char* want : wants) {
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
}  // namespace

std::string epicCoverUrlFromCatalog(const std::string& catalogJson,
                                    const std::string& catalogItemId) {
    // Portrait box first (matches the grid), then the landscape box.
    return epicUrlFromCatalog(catalogJson, catalogItemId,
                              {"DieselGameBoxTall", "DieselGameBox", "Thumbnail"});
}

std::string epicHeroUrlFromCatalog(const std::string& catalogJson,
                                   const std::string& catalogItemId) {
    // The landscape box is the hero; the tall box still beats nothing.
    return epicUrlFromCatalog(catalogJson, catalogItemId,
                              {"DieselGameBox", "DieselGameBoxWide", "DieselGameBoxTall"});
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

std::optional<std::string> heroBytes(Store store, const std::string& launchId,
                                     const std::string& coverHint, int64_t cacheId,
                                     bool allowNetwork) {
    if (auto cached = readFile(heroDiskPath(cacheId))) return cached;
    if (!allowNetwork) return std::nullopt;

    std::optional<std::string> data;
    if (store == Store::Xbox) {
        // coverHint = "<StoreId>|<local logo path>"; the hero uses only the StoreId
        // half (the shipped logo PNG is square/generic — wrong shape for a banner).
        auto sep = coverHint.find('|');
        std::string storeId = sep == std::string::npos ? "" : coverHint.substr(0, sep);
        data = xboxCatalogHero(storeId);
    } else if (store == Store::Epic) {
        auto raw = readFile(catCachePath());
        if (raw) {
            auto jsonText = base64Decode(*raw);
            std::string url = epicHeroUrlFromCatalog(jsonText ? *jsonText : *raw, coverHint);
            if (!url.empty()) data = http::get(url);
        }
    } else if (store == Store::Gog) {
        data = gogHero(launchId);
    }
    if (data) saveFile(heroDiskPath(cacheId), *data);
    return data;
}

}  // namespace ss::store_covers
