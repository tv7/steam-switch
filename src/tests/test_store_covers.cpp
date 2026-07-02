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

TEST_CASE(epic_hero_prefers_wide_box) {
    std::string catalog = R"([
      {"id":"cat123","keyImages":[
          {"type":"Thumbnail","url":"https://x/thumb.jpg"},
          {"type":"DieselGameBox","url":"https://x/wide.jpg"},
          {"type":"DieselGameBoxTall","url":"https://x/tall.jpg"}]},
      {"id":"tallonly","keyImages":[
          {"type":"DieselGameBoxTall","url":"https://x/tall2.jpg"}]}
    ])";
    CHECK_EQ(store_covers::epicHeroUrlFromCatalog(catalog, "cat123"),
             std::string("https://x/wide.jpg"));
    // Falls back to the tall box rather than nothing.
    CHECK_EQ(store_covers::epicHeroUrlFromCatalog(catalog, "tallonly"),
             std::string("https://x/tall2.jpg"));
    CHECK_EQ(store_covers::epicHeroUrlFromCatalog(catalog, "missing"), std::string(""));
}

TEST_CASE(xbox_hero_uses_titled_hero_art_and_caches) {
    fs::path data = freshDir("xheros");
    appdata::setDir(data);
    int fetches = 0;
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        ++fetches;
        if (url.find("displaycatalog.mp.microsoft.com") != std::string::npos)
            return std::string(R"({"Products":[{"LocalizedProperties":[{"Images":[
                {"ImagePurpose":"Poster","Uri":"//cdn/poster.jpg"},
                {"ImagePurpose":"TitledHeroArt","Uri":"//cdn/hero.jpg"}]}]}]})");
        if (url.rfind("https://cdn/hero.jpg", 0) == 0) return std::string("XHERO");
        return std::nullopt;
    });
    // coverHint = "<StoreId>|<logo path>"; the hero must pick TitledHeroArt, not Poster.
    auto got = store_covers::heroBytes(Store::Xbox, "PFN!Game", "9NABC|/x/logo.png", 4242);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("XHERO"));
    CHECK(fs::exists(data / "covers" / "4242_hero.jpg"));
    int after = fetches;
    auto again = store_covers::heroBytes(Store::Xbox, "PFN!Game", "9NABC|/x/logo.png", 4242);
    CHECK_EQ(again.value_or(""), std::string("XHERO"));
    CHECK_EQ(fetches, after);                       // disk cache, no re-fetch
    // Offline + uncached -> nothing (no local hero source for Xbox).
    CHECK_EQ(store_covers::heroBytes(Store::Xbox, "PFN!Game", "9NABC|/x/logo.png", 4243,
                                     false).has_value(), false);
    http::setFetcher({});
}

TEST_CASE(gog_hero_uses_background_image) {
    fs::path data = freshDir("gheros");
    appdata::setDir(data);
    http::setFetcher([&](const std::string& url) -> std::optional<std::string> {
        if (url == "https://api.gog.com/v2/games/1207664663")
            return std::string(R"({"_links":{
                "boxArtImage":{"href":"//images.gog.com/box.jpg"},
                "backgroundImage":{"href":"//images.gog.com/bg.jpg"}}})");
        if (url == "https://images.gog.com/bg.jpg") return std::string("GOGBG");
        return std::nullopt;
    });
    auto got = store_covers::heroBytes(Store::Gog, "1207664663", "", 77);
    CHECK(got.has_value());
    CHECK_EQ(*got, std::string("GOGBG"));           // background, not the box art
    http::setFetcher({});
}
