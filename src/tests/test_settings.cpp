// Settings persistence — mirrors server.py's set_language/_load_settings round-trip.

#include "test_util.h"

#include "../core/appdata.h"
#include "../core/settings.h"

#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;
using namespace ss;

TEST_CASE(settings_language_defaults_to_en) {
    fs::path data = fs::temp_directory_path() / ("ss_set_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    CHECK_EQ(settings::language(), std::string("en"));   // no file yet
}

TEST_CASE(settings_language_round_trips) {
    fs::path data = fs::temp_directory_path() / ("ss_set2_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    settings::setLanguage("ar");
    CHECK_EQ(settings::language(), std::string("ar"));
    settings::setLanguage("en");                          // overwrite persists
    CHECK_EQ(settings::language(), std::string("en"));
    CHECK(fs::exists(data / "settings.json"));
}

TEST_CASE(launch_history_records_and_reads_back) {
    fs::path data = fs::temp_directory_path() / ("ss_set3_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    CHECK_EQ(settings::lastLaunched("Epic:Fortnite"), 0LL);   // never launched
    settings::recordLaunch("Epic:Fortnite", 1700000000LL);
    settings::recordLaunch("Steam:730", 1700000100LL);
    CHECK_EQ(settings::lastLaunched("Epic:Fortnite"), 1700000000LL);
    settings::recordLaunch("Epic:Fortnite", 1700000200LL);    // newer launch overwrites
    CHECK_EQ(settings::lastLaunched("Epic:Fortnite"), 1700000200LL);
    auto all = settings::launchHistory();
    CHECK_EQ(all.size(), (size_t)2);
    CHECK_EQ(all["Steam:730"], 1700000100LL);
    // Coexists with the other settings keys.
    settings::setLanguage("ar");
    CHECK_EQ(settings::lastLaunched("Steam:730"), 1700000100LL);
    CHECK_EQ(settings::language(), std::string("ar"));
    // Garbage input is ignored, not persisted.
    settings::recordLaunch("", 123);
    settings::recordLaunch("X:y", 0);
    CHECK_EQ(settings::launchHistory().size(), (size_t)2);
    fs::remove_all(data);
}

TEST_CASE(settings_hero_mode_and_offline_default) {
    fs::path data = fs::temp_directory_path() / ("ss_set4_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    appdata::setDir(data);
    CHECK_EQ(settings::heroMode(), std::string("last"));      // default
    CHECK_EQ(settings::offlineDefault(), false);              // default
    settings::setHeroMode("random");
    CHECK_EQ(settings::heroMode(), std::string("random"));
    settings::setHeroMode("garbage");                          // sanitised
    CHECK_EQ(settings::heroMode(), std::string("last"));
    settings::setOfflineDefault(true);
    CHECK_EQ(settings::offlineDefault(), true);
    CHECK_EQ(settings::language(), std::string("en"));         // others untouched
    fs::remove_all(data);
}
