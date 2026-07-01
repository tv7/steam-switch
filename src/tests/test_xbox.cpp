// Xbox store — SHA-256 vectors, the PackageFamilyName publisher hash (validated
// against Microsoft's well-known "8wekyb3d8bbwe"), MicrosoftGame.config parsing +
// AUMID, and folder enumeration over a synthetic $SS_XBOX_ROOTS tree.

#include "test_util.h"

#include "../core/model.h"
#include "../core/sha256.h"
#include "../core/stores/store.h"
#include "../core/xbox_games.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;
using namespace ss;

namespace {
std::string hex(const std::array<uint8_t, 32>& d) {
    static const char* h = "0123456789abcdef";
    std::string s;
    for (auto b : d) { s.push_back(h[b >> 4]); s.push_back(h[b & 0xf]); }
    return s;
}
// Microsoft's canonical publisher DN — its hash is the famous "8wekyb3d8bbwe".
const char* kMsPublisher =
    "CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US";
}  // namespace

TEST_CASE(sha256_known_vectors) {
    // FIPS-180 test vectors.
    CHECK_EQ(hex(crypto::sha256("")),
             std::string("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    CHECK_EQ(hex(crypto::sha256("abc")),
             std::string("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"));
}

TEST_CASE(xbox_publisher_hash_matches_microsoft) {
    // The end-to-end check of the SHA-256 + base32 pipeline.
    CHECK_EQ(xbox::publisherHash(kMsPublisher), std::string("8wekyb3d8bbwe"));
    CHECK_EQ(xbox::packageFamilyName("Microsoft.WindowsCalculator", kMsPublisher),
             std::string("Microsoft.WindowsCalculator_8wekyb3d8bbwe"));
}

TEST_CASE(xbox_parse_config_and_aumid) {
    std::string xml = R"(<?xml version="1.0" encoding="utf-8"?>
<Game configVersion="1">
  <Identity Name="Microsoft.PaintShop" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" Version="1.2.3.0" />
  <ExecutableList>
    <Executable Name="Game.exe" Id="Game" TargetDeviceFamily="PC" />
  </ExecutableList>
  <ShellVisuals DefaultDisplayName="Paint Shop" PublisherDisplayName="MS" />
</Game>)";
    auto c = xbox::parseConfig(xml);
    CHECK(c.has_value());
    CHECK_EQ(c->identityName, std::string("Microsoft.PaintShop"));
    CHECK_EQ(c->displayName, std::string("Paint Shop"));
    CHECK_EQ(c->appId, std::string("Game"));
    CHECK_EQ(xbox::aumid(*c), std::string("Microsoft.PaintShop_8wekyb3d8bbwe!Game"));
}

TEST_CASE(xbox_parse_config_display_falls_back_on_ms_resource) {
    std::string xml =
        R"(<Game><Identity Name="Pub.Game" Publisher="CN=Pub" />)"
        R"(<ShellVisuals DefaultDisplayName="ms-resource:AppName" /><Executable Id="App" /></Game>)";
    auto c = xbox::parseConfig(xml);
    CHECK(c.has_value());
    CHECK_EQ(c->displayName, std::string("Pub.Game"));   // fell back to identity name
    CHECK_EQ(c->appId, std::string("App"));
}

TEST_CASE(xbox_real_config_reads_executable_id_not_executablelist) {
    // Real ARC Raiders config (abridged): the launchable <Executable> lives inside
    // <ExecutableList>. The AppId must come from it — NOT default to "Game" because
    // the parser stopped at the <ExecutableList> prefix.
    std::string xml = R"(<Game configVersion="1">
  <Identity Name="Embark.arc-raiders" Publisher="CN=C8213C0F-4430-4083-8FB4-16A68674F2B4" Version="1.1233.4650.0" />
  <ShellVisuals DefaultDisplayName="ARC Raiders" PublisherDisplayName="Embark" />
  <ExecutableList>
    <Executable Name="PioneerGame.exe" Id="AppARCRaidersShipping" TargetDeviceFamily="PC" />
  </ExecutableList>
</Game>)";
    auto c = xbox::parseConfig(xml);
    CHECK(c.has_value());
    CHECK_EQ(c->displayName, std::string("ARC Raiders"));
    CHECK_EQ(c->appId, std::string("AppARCRaidersShipping"));
}

TEST_CASE(xbox_dlc_stub_is_skipped) {
    // Real BO6 DLC stub (abridged): no <Executable> — a content package, not a game.
    std::string xml = R"(<Game configVersion="1">
  <Identity Name="38985CA0.BO6DLC06BetaPack01" Publisher="CN=07A9AC0F-5502-4D92-BA69-01D5D39D1E92" Version="0.0.10.0" />
  <ShellVisuals DefaultDisplayName="BO6 DLC06 Beta Pack 01" PublisherDisplayName="Activision Publishing Inc." />
  <TargetDeviceFamilyForDLC>PC</TargetDeviceFamilyForDLC>
  <DesktopRegistration><MainPackageDependency Name="38985CA0.COREBase" /></DesktopRegistration>
</Game>)";
    CHECK_EQ(xbox::parseConfig(xml).has_value(), false);
}

TEST_CASE(xbox_scan_finds_games_under_roots) {
    fs::path root = fs::temp_directory_path()
        / ("ss_xbox_" + std::to_string(ss::test::procId()) + "_" + std::to_string(rand()));
    fs::path content = root / "Forsaken" / "Content";
    fs::create_directories(content);
    std::ofstream(content / "MicrosoftGame.config", std::ios::binary) << R"(<Game>
        <Identity Name="Studio.Forsaken" Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
        <ShellVisuals DefaultDisplayName="Forsaken" />
        <Executable Id="Game" /></Game>)";
    // A dir without a config is ignored.
    fs::create_directories(root / "NotAGame");

    ss::test::setEnv("SS_XBOX_ROOTS", root.string());
    auto store = makeXboxStore();
    CHECK_EQ(store->kind() == Store::Xbox, true);
    CHECK_EQ(store->accounts().empty(), true);
    auto games = store->scan();
    CHECK_EQ(games.size(), static_cast<size_t>(1));
    CHECK_EQ(games[0].name, std::string("Forsaken"));
    CHECK_EQ(games[0].store == Store::Xbox, true);
    CHECK_EQ(games[0].launchId, std::string("Studio.Forsaken_8wekyb3d8bbwe!Game"));
    CHECK_EQ(games[0].appid, static_cast<int64_t>(0));
}
