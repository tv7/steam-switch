// Mapping resolution tests against a synthetic Steam tree, including the
// family-share playtime tiebreak (the family-share-mapping-tiebreaker memory).

#include "../core/appdata.h"
#include "../core/steam_accounts.h"
#include "../core/steam_paths.h"
#include "test_util.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace ss;
namespace fs = std::filesystem;

namespace {

void writeFile(const fs::path& p, const std::string& s) {
    fs::create_directories(p.parent_path());
    std::ofstream f(p, std::ios::binary); f << s;
}

// alice = 7656119...001 (accountid 1), bob = 7656119...002 (accountid 2).
// Game 100: normally owned by alice (LastOwner=alice, alice local).
// Game 200: family share — LastOwner is a non-local lender; both alice and bob
//           have userdata; bob has more playtime so should win.
fs::path makeTree() {
    fs::path root = fs::temp_directory_path() / ("ss_acc_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    std::string alice = std::to_string(kSteamId64Base + 1);
    std::string bob = std::to_string(kSteamId64Base + 2);
    std::string lender = "76561190000099999";

    writeFile(root / "steamapps" / "libraryfolders.vdf",
              "\"libraryfolders\"{\"0\"{\"path\" \"" + root.string() + "\"}}");
    writeFile(root / "config" / "loginusers.vdf",
              "\"users\"{"
              "\"" + alice + "\"{\"AccountName\" \"alice\" \"PersonaName\" \"Alice\" \"MostRecent\" \"1\" \"RememberPassword\" \"1\"}"
              "\"" + bob + "\"{\"AccountName\" \"bob\" \"PersonaName\" \"Bob\" \"MostRecent\" \"0\" \"RememberPassword\" \"1\"}"
              "}");
    writeFile(root / "steamapps" / "appmanifest_100.acf",
              "\"AppState\"{\"appid\" \"100\" \"name\" \"Owned Game\" \"StateFlags\" \"4\" \"LastOwner\" \"" + alice + "\"}");
    writeFile(root / "steamapps" / "appmanifest_200.acf",
              "\"AppState\"{\"appid\" \"200\" \"name\" \"Shared Game\" \"StateFlags\" \"4\" \"LastOwner\" \"" + lender + "\"}");

    // userdata folders: both have game 200; alice also has 100.
    fs::create_directories(root / "userdata" / "1" / "100");
    fs::create_directories(root / "userdata" / "1" / "200");
    fs::create_directories(root / "userdata" / "2" / "200");
    // localconfig playtime: bob plays 200 a lot, alice barely.
    writeFile(root / "userdata" / "1" / "config" / "localconfig.vdf",
              "\"UserLocalConfigStore\"{\"Software\"{\"Valve\"{\"Steam\"{\"apps\"{"
              "\"200\"{\"Playtime\" \"5\" \"LastPlayed\" \"1000\"}}}}}}");
    writeFile(root / "userdata" / "2" / "config" / "localconfig.vdf",
              "\"UserLocalConfigStore\"{\"Software\"{\"Valve\"{\"Steam\"{\"apps\"{"
              "\"200\"{\"Playtime\" \"600\" \"LastPlayed\" \"2000\"}}}}}}");

    ss::test::setEnv("STEAM_ROOT", root.string());
    appdata::setDir(root / "_appdata");   // isolate mapping.json
    steam::clearCaches();
    return root;
}

}  // namespace

TEST_CASE(accounts_listed_from_loginusers) {
    auto root = makeTree();
    auto accts = steam::listAccounts();
    CHECK_EQ(accts.size(), (size_t)2);
    fs::remove_all(root);
}

TEST_CASE(normal_game_maps_to_last_owner) {
    auto root = makeTree();
    auto sid = steam::accountForGame(100);
    CHECK(sid.has_value());
    CHECK_EQ(*sid, std::to_string(kSteamId64Base + 1));   // alice
    fs::remove_all(root);
}

TEST_CASE(family_share_tiebreaks_by_playtime) {
    auto root = makeTree();
    // LastOwner is a non-local lender; both alice & bob have userdata; bob plays more.
    auto sid = steam::accountForGame(200);
    CHECK(sid.has_value());
    CHECK_EQ(*sid, std::to_string(kSteamId64Base + 2));   // bob wins on playtime
    fs::remove_all(root);
}

TEST_CASE(override_wins) {
    auto root = makeTree();
    steam::setOverride(200, std::to_string(kSteamId64Base + 1));   // pin to alice
    auto sid = steam::accountForGame(200);
    CHECK(sid.has_value());
    CHECK_EQ(*sid, std::to_string(kSteamId64Base + 1));   // override beats playtime
    fs::remove_all(root);
}

TEST_CASE(unmapped_returns_nullopt) {
    auto root = makeTree();
    auto sid = steam::accountForGame(999);   // no manifest, no userdata
    CHECK(!sid.has_value());
    fs::remove_all(root);
}

TEST_CASE(account_usage_map_bulk_matches_app_usage) {
    auto root = makeTree();
    std::string bob = std::to_string(kSteamId64Base + 2);
    const auto& usage = steam::accountUsageMap(bob);
    CHECK_EQ(usage.size(), (size_t)1);
    CHECK(usage.count(200) == 1);
    CHECK_EQ(usage.at(200).playtime, 600LL);
    CHECK_EQ(usage.at(200).lastPlayed, 2000LL);
    // Agrees with the per-app reader used by the tiebreak.
    auto single = steam::appUsage(200, bob);
    CHECK_EQ(usage.at(200).playtime, single.playtime);
    CHECK_EQ(usage.at(200).lastPlayed, single.lastPlayed);
    fs::remove_all(root);
}

TEST_CASE(account_usage_map_missing_account_is_empty) {
    auto root = makeTree();
    const auto& usage = steam::accountUsageMap(std::to_string(kSteamId64Base + 42));
    CHECK(usage.empty());
    // Bad ids don't throw either.
    CHECK(steam::accountUsageMap("not-a-steamid").empty());
    fs::remove_all(root);
}
