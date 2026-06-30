// Tests for steam_paths + steam_games against a synthetic Steam tree (no real
// Steam), driven via $STEAM_ROOT. Mirrors tests/smoke_test.py's fixture approach.

#include "../core/steam_games.h"
#include "../core/steam_paths.h"
#include "test_util.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>

using namespace ss;
namespace fs = std::filesystem;

namespace {

fs::path makeSyntheticSteam() {
    fs::path root = fs::temp_directory_path() / ("ss_steam_" + std::to_string(ss::test::procId()) +
                                                 "_" + std::to_string(rand()));
    fs::create_directories(root / "steamapps");
    fs::create_directories(root / "config");

    auto write = [](const fs::path& p, const std::string& s) {
        std::ofstream f(p, std::ios::binary); f << s;
    };

    // libraryfolders.vdf with only the root library.
    write(root / "steamapps" / "libraryfolders.vdf",
          "\"libraryfolders\"\n{\n\t\"0\"\n\t{\n\t\t\"path\"\t\t\"" +
          root.string() + "\"\n\t}\n}\n");

    // Two installed games: one fully installed, one partial.
    write(root / "steamapps" / "appmanifest_440.acf",
          "\"AppState\"\n{\n\t\"appid\"\t\"440\"\n\t\"name\"\t\"Team Fortress 2\"\n"
          "\t\"installdir\"\t\"Team Fortress 2\"\n\t\"StateFlags\"\t\"4\"\n"
          "\t\"LastOwner\"\t\"76561190000000001\"\n}\n");
    write(root / "steamapps" / "appmanifest_220.acf",
          "\"AppState\"\n{\n\t\"appid\"\t\"220\"\n\t\"name\"\t\"Half-Life 2\"\n"
          "\t\"installdir\"\t\"Half-Life 2\"\n\t\"StateFlags\"\t\"1026\"\n"
          "\t\"LastOwner\"\t\"76561190000000002\"\n}\n");

    ss::test::setEnv("STEAM_ROOT", root.string());
    return root;
}

}  // namespace

TEST_CASE(steam_root_honors_env) {
    auto root = makeSyntheticSteam();
    auto r = steam::steamRoot();
    CHECK(r.has_value());
    CHECK_EQ(*r, root);
    fs::remove_all(root);
}

TEST_CASE(library_folders_includes_root) {
    auto root = makeSyntheticSteam();
    auto libs = steam::libraryFolders();
    CHECK(!libs.empty());
    bool hasRoot = false;
    for (auto& l : libs) if (l == root) hasRoot = true;
    CHECK(hasRoot);
    fs::remove_all(root);
}

TEST_CASE(installed_games_enumerates_and_sorts) {
    auto root = makeSyntheticSteam();
    auto games = steam::installedGames();
    CHECK_EQ(games.size(), (size_t)2);
    // Sorted by name: "Half-Life 2" before "Team Fortress 2".
    CHECK_EQ(games[0].name, std::string("Half-Life 2"));
    CHECK_EQ(games[0].appid, (int64_t)220);
    CHECK(!games[0].fullyInstalled);              // StateFlags 1026 -> not bit 2
    CHECK_EQ(games[1].name, std::string("Team Fortress 2"));
    CHECK(games[1].fullyInstalled);               // StateFlags 4 -> installed
    CHECK_EQ(games[1].lastOwner, std::string("76561190000000001"));
    fs::remove_all(root);
}
