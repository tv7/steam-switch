// Cover resolver with an injected HTTP fetcher (no real network). Verifies the
// fallback order + that only network hits are disk-cached.

#include "../core/appdata.h"
#include "../core/covers.h"
#include "../core/http.h"
#include "test_util.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace ss;
namespace fs = std::filesystem;

TEST_CASE(cover_uses_legacy_cdn_then_caches) {
    fs::path data = fs::temp_directory_path() / ("ss_cov_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    ss::test::setEnv("STEAM_ROOT", "/nonexistent_steam_root");   // force no local art

    int calls = 0;
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        ++calls;
        if (url.find("cdn.cloudflare") != std::string::npos &&
            url.find("library_600x900.jpg") != std::string::npos)
            return std::string("JPEGDATA");
        return std::nullopt;
    });

    auto bytes = covers::coverBytes(12345);
    CHECK(bytes.has_value());
    CHECK_EQ(*bytes, std::string("JPEGDATA"));

    // Second call should hit the disk cache, not the network again.
    int callsAfterFirst = calls;
    auto bytes2 = covers::coverBytes(12345);
    CHECK(bytes2.has_value());
    CHECK_EQ(calls, callsAfterFirst);   // no further fetches

    http::setFetcher({});
    fs::remove_all(data);
}

namespace {
// Minimal PNG header (signature + IHDR) with the given dimensions — enough for
// covers::imageSize; the pixel data doesn't matter.
std::string fakePng(int w, int h) {
    std::string d("\x89PNG\r\n\x1a\n", 8);
    d += std::string("\0\0\0\rIHDR", 8);
    for (int v : {w, h})
        for (int s : {24, 16, 8, 0}) d.push_back(char((v >> s) & 0xFF));
    d += std::string(8, '\0');
    return d;
}
}  // namespace

TEST_CASE(image_size_parses_png_and_jpeg) {
    auto png = covers::imageSize(fakePng(600, 900));
    CHECK(png.has_value());
    CHECK_EQ(png->first, 600);
    CHECK_EQ(png->second, 900);
    // Tiny JPEG: SOI + SOF0 (len 11, 8-bit, 900x600, 1 component).
    std::string jpg("\xFF\xD8\xFF\xC0\x00\x0B\x08\x03\x84\x02\x58\x01\x00\x00", 14);
    auto j = covers::imageSize(jpg);
    CHECK(j.has_value());
    CHECK_EQ(j->first, 600);
    CHECK_EQ(j->second, 900);
    CHECK_EQ(covers::imageSize("not an image").has_value(), false);
}

TEST_CASE(cover_hashed_librarycache_fallback_picks_portrait_only) {
    // A hidden demo: no known-name art, no disk cache, no network — but Steam left
    // hashed files in librarycache/<appid>/: a big LOGO (wide) and the portrait.
    fs::path root = fs::temp_directory_path()
        / ("ss_cov3_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    fs::path data = root / "appdata";
    appdata::setDir(data);
    fs::path lc = root / "steam" / "appcache" / "librarycache" / "777";
    fs::create_directories(lc);
    fs::create_directories(root / "steam" / "steamapps");   // make steamRoot valid
    std::ofstream(root / "steam" / "steamapps" / "libraryfolders.vdf") << "\"libraryfolders\"{}";
    std::ofstream(lc / "a1b2c3.png", std::ios::binary) << fakePng(1280, 720);   // logo/hero: wide
    std::ofstream(lc / "d4e5f6.png", std::ios::binary) << fakePng(600, 900);    // the capsule
    ss::test::setEnv("STEAM_ROOT", (root / "steam").string());
    http::setFetcher({});

    auto bytes = covers::coverBytes(777, /*allowNetwork=*/false);
    CHECK(bytes.has_value());
    auto size = covers::imageSize(*bytes);
    CHECK(size.has_value());
    CHECK_EQ(size->second, 900);   // the portrait won, not the wide logo
    fs::remove_all(root);
}

TEST_CASE(cover_offline_returns_nullopt_when_nothing_local) {
    fs::path data = fs::temp_directory_path() / ("ss_cov2_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    ss::test::setEnv("STEAM_ROOT", "/nonexistent_steam_root");
    http::setFetcher({});                 // no network
    auto bytes = covers::coverBytes(999, /*allowNetwork=*/false);
    CHECK(!bytes.has_value());
    fs::remove_all(data);
}
