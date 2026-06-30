// Switcher writes that are platform-independent: loginusers.vdf MostRecent +
// stale-offline-flag clearing, config.vdf picker toggle, offline flag set/read,
// backup creation, accountid math. (The registry/process bits are Windows-only and
// exercised on the user's real PC.)

#include "../core/model.h"
#include "../core/steam_switcher.h"
#include "../core/vdf.h"
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

fs::path makeTree() {
    fs::path root = fs::temp_directory_path() / ("ss_sw_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    std::string alice = std::to_string(kSteamId64Base + 1);
    std::string bob = std::to_string(kSteamId64Base + 2);
    writeFile(root / "config" / "loginusers.vdf",
              "\"users\"{"
              "\"" + alice + "\"{\"AccountName\" \"alice\" \"PersonaName\" \"Alice\" \"MostRecent\" \"0\" "
              "\"RememberPassword\" \"1\" \"WantsOfflineMode\" \"1\"}"
              "\"" + bob + "\"{\"AccountName\" \"bob\" \"PersonaName\" \"Bob\" \"MostRecent\" \"1\" "
              "\"RememberPassword\" \"0\"}"
              "}");
    writeFile(root / "config" / "config.vdf",
              "\"InstallConfigStore\"{\"Software\"{\"Valve\"{\"Steam\"{\"AlwaysShowUserChooser\" \"0\"}}}}");
    ss::test::setEnv("STEAM_ROOT", root.string());
    return root;
}

const vdf::Value* userInfo(const vdf::Value& data, const std::string& sid) {
    const vdf::Value* users = data.get("users");
    return users ? users->get(sid) : nullptr;
}

}  // namespace

TEST_CASE(accountid_math) {
    CHECK_EQ(accountIdFromSteamId64(kSteamId64Base + 7), (int64_t)7);
}

TEST_CASE(switch_sets_most_recent_and_clears_stale_offline) {
    auto root = makeTree();
    steam::switcher::switchAccount("alice");
    auto data = vdf::load((root / "config" / "loginusers.vdf").string());
    std::string alice = std::to_string(kSteamId64Base + 1);
    std::string bob = std::to_string(kSteamId64Base + 2);
    CHECK_EQ(userInfo(data, alice)->getStr("MostRecent"), std::string("1"));
    CHECK_EQ(userInfo(data, bob)->getStr("MostRecent"), std::string("0"));
    // Stale WantsOfflineMode on the target is cleared (would hang online cold start).
    CHECK_EQ(userInfo(data, alice)->getStr("WantsOfflineMode"), std::string("0"));
    CHECK_EQ(userInfo(data, alice)->getStr("AllowAutoLogin"), std::string("1"));
    CHECK_EQ(userInfo(data, bob)->getStr("AllowAutoLogin"), std::string("0"));
    fs::remove_all(root);
}

TEST_CASE(switch_makes_backup_once) {
    auto root = makeTree();
    fs::path bak = root / "config" / "loginusers.vdf.bak";
    CHECK(!fs::exists(bak));
    steam::switcher::switchAccount("alice");
    CHECK(fs::exists(bak));
    fs::remove_all(root);
}

TEST_CASE(offline_flag_set_and_read) {
    auto root = makeTree();
    std::string bob = std::to_string(kSteamId64Base + 2);
    CHECK(!steam::switcher::wantsOfflineMode(bob));            // bob starts online
    CHECK(steam::switcher::setOfflineMode("bob", true));
    CHECK(steam::switcher::wantsOfflineMode(bob));
    auto data = vdf::load((root / "config" / "loginusers.vdf").string());
    CHECK_EQ(userInfo(data, bob)->getStr("SkipOfflineModeWarning"), std::string("1"));
    CHECK(steam::switcher::setOfflineMode("bob", false));
    CHECK(!steam::switcher::wantsOfflineMode(bob));
    fs::remove_all(root);
}

TEST_CASE(picker_toggle_writes_config) {
    auto root = makeTree();
    CHECK(steam::switcher::setAccountPicker(root, true));
    auto data = vdf::load((root / "config" / "config.vdf").string());
    const vdf::Value* node = &data;
    for (const char* p : {"InstallConfigStore", "Software", "Valve", "Steam"}) node = node->getCI(p);
    CHECK_EQ(node->getStr("AlwaysShowUserChooser"), std::string("1"));
    fs::remove_all(root);
}

TEST_CASE(can_autologin_preflight) {
    auto root = makeTree();
    auto [okA, whyA] = steam::switcher::canAutologin("alice");   // RememberPassword=1
    CHECK(okA);
    auto [okB, whyB] = steam::switcher::canAutologin("bob");     // RememberPassword=0
    CHECK(!okB);
    auto [okC, whyC] = steam::switcher::canAutologin("nobody");  // not present
    CHECK(!okC);
    fs::remove_all(root);
}
