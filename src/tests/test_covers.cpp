// Cover resolver with an injected HTTP fetcher (no real network). Verifies the
// fallback order + that only network hits are disk-cached.

#include "../core/appdata.h"
#include "../core/covers.h"
#include "../core/http.h"
#include "test_util.h"

#include <cstdlib>
#include <filesystem>

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

TEST_CASE(cover_offline_returns_nullopt_when_nothing_local) {
    fs::path data = fs::temp_directory_path() / ("ss_cov2_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    ss::test::setEnv("STEAM_ROOT", "/nonexistent_steam_root");
    http::setFetcher({});                 // no network
    auto bytes = covers::coverBytes(999, /*allowNetwork=*/false);
    CHECK(!bytes.has_value());
    fs::remove_all(data);
}
