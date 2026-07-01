// Per-store cover resolution: base64 + Epic catcache URL selection (pure), the
// Xbox local-logo read, and the GOG API fetch + disk-cache via a stub fetcher.

#include "test_util.h"

#include "../core/appdata.h"
#include "../core/http.h"
#include "../core/store_covers.h"

#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using namespace ss;

namespace {
fs::path freshDir(const char* tag) {
    fs::path d = fs::temp_directory_path()
        / (std::string("ss_scov_") + tag + "_" + std::to_string(ss::test::procId()));
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}
}  // namespace

TEST_CASE(base64_decode_roundtrip) {
    CHECK_EQ(store_covers::base64Decode("aGVsbG8=").value_or(""), std::string("hello"));
    CHECK_EQ(store_covers::base64Decode("aGVsbG8h").value_or(""), std::string("hello!"));
    CHECK_EQ(store_covers::base64Decode("bad~chars").has_value(), false);
}

TEST_CASE(epic_catalog_prefers_tall_box) {
    std::string catalog = R"([
      {"id":"other","keyImages":[{"type":"DieselGameBoxTall","url":"https://x/no.jpg"}]},
      {"id":"cat123","keyImages":[
          {"type":"Thumbnail","url":"https://x/thumb.jpg"},
          {"type":"DieselGameBox","url":"https://x/wide.jpg"},
          {"type":"DieselGameBoxTall","url":"https://x/tall.jpg"}]}
    ])";
    CHECK_EQ(store_covers::epicCoverUrlFromCatalog(catalog, "cat123"),
             std::string("https://x/tall.jpg"));
    CHECK_EQ(store_covers::epicCoverUrlFromCatalog(catalog, "missing"), std::string(""));
}

TEST_CASE(xbox_cover_reads_local_logo) {
    fs::path dir = freshDir("xbox");
    std::ofstream(dir / "logo.png", std::ios::binary) << "\x89PNG-bytes";
    auto got = store_covers::coverBytes(Store::Xbox, "PFN!Game",
                                        (dir / "logo.png").string(), 42, false);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("\x89PNG-bytes"));
    // No hint -> no art (and never a network call).
    CHECK_EQ(store_covers::coverBytes(Store::Xbox, "PFN!Game", "", 42, false).has_value(),
             false);
}

TEST_CASE(gog_cover_fetches_boxart_and_disk_caches) {
    fs::path data = freshDir("gog");
    appdata::setDir(data);
    int fetches = 0;
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        ++fetches;
        if (url == "https://api.gog.com/v2/games/1207664663")
            return std::string(R"({"_links":{"boxArtImage":{"href":"//images.gog.com/box.jpg"}}})");
        if (url == "https://images.gog.com/box.jpg") return std::string("GOGIMG");
        return std::nullopt;
    });
    auto got = store_covers::coverBytes(Store::Gog, "1207664663", "", 99);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("GOGIMG"));
    int after = fetches;
    // Second call must come from the disk cache — no new network round-trips.
    auto again = store_covers::coverBytes(Store::Gog, "1207664663", "", 99);
    CHECK_EQ(again.value_or(""), std::string("GOGIMG"));
    CHECK_EQ(fetches, after);
    http::setFetcher({});
}
