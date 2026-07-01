#include "xbox_games.h"

#include "platform.h"
#include "sha256.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace ss::xbox {

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return "";
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

// Value of `attr="..."` within a single XML tag's text (naive; the config is
// machine-generated, so attributes are plain double-quoted and unescaped).
std::string attr(const std::string& tag, const std::string& name) {
    std::string needle = name + "=\"";
    auto pos = tag.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    auto end = tag.find('"', pos);
    if (end == std::string::npos) return "";
    return tag.substr(pos, end - pos);
}

// The opening tag text (`<Name ... >` contents, without the brackets) of the first
// `<element` in `xml`, or "" if absent.
std::string openTag(const std::string& xml, const std::string& element) {
    auto pos = xml.find("<" + element);
    if (pos == std::string::npos) return "";
    auto end = xml.find('>', pos);
    if (end == std::string::npos) return "";
    return xml.substr(pos, end - pos);
}

// UTF-16LE bytes of an ASCII string (publisher DNs are ASCII).
std::string toUtf16le(const std::string& s) {
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) { out.push_back(static_cast<char>(c)); out.push_back('\0'); }
    return out;
}

}  // namespace

std::string publisherHash(const std::string& publisher) {
    auto digest = crypto::sha256(toUtf16le(publisher));   // 32 bytes
    // Take the first 8 bytes = 64 bits, append a 0 bit (→ 65 bits), read as 13
    // groups of 5 bits MSB-first through Crockford-style base32.
    static const char* kAlphabet = "0123456789abcdefghjkmnpqrstvwxyz";
    auto bit = [&](int i) -> int {
        if (i >= 64) return 0;                 // the padding bit
        return (digest[i / 8] >> (7 - (i % 8))) & 1;
    };
    std::string hash;
    for (int g = 0; g < 13; ++g) {
        int v = 0;
        for (int b = 0; b < 5; ++b) v = (v << 1) | bit(g * 5 + b);
        hash.push_back(kAlphabet[v]);
    }
    return hash;
}

std::string packageFamilyName(const std::string& identityName, const std::string& publisher) {
    return identityName + "_" + publisherHash(publisher);
}

std::string aumid(const Config& c) {
    return packageFamilyName(c.identityName, c.publisher) + "!" + c.appId;
}

std::optional<Config> parseConfig(const std::string& xml) {
    std::string identity = openTag(xml, "Identity");
    if (identity.empty()) return std::nullopt;
    Config c;
    c.identityName = attr(identity, "Name");
    c.publisher = attr(identity, "Publisher");
    if (c.identityName.empty() || c.publisher.empty()) return std::nullopt;

    std::string shell = openTag(xml, "ShellVisuals");
    std::string display = attr(shell, "DefaultDisplayName");
    // Some configs use an ms-resource: indirection we can't resolve — fall back.
    c.displayName = (display.empty() || display.rfind("ms-resource:", 0) == 0)
                        ? c.identityName : display;

    std::string exe = openTag(xml, "Executable");
    c.appId = attr(exe, "Id");
    if (c.appId.empty()) c.appId = "Game";   // the near-universal default app id
    return c;
}

std::vector<std::string> installRoots() {
    std::vector<std::string> roots;
    if (auto env = platform::getEnv("SS_XBOX_ROOTS")) {
        std::string s = *env, cur;
        for (char ch : s) {
            if (ch == ';') { if (!cur.empty()) roots.push_back(cur); cur.clear(); }
            else cur.push_back(ch);
        }
        if (!cur.empty()) roots.push_back(cur);
        return roots;
    }
    // Default: <drive>:\XboxGames for each fixed drive letter (Game Pass makes this
    // folder on whichever drive the user installs to).
    std::error_code ec;
    for (char d = 'C'; d <= 'Z'; ++d) {
        std::string root = std::string(1, d) + ":\\XboxGames";
        if (fs::is_directory(root, ec)) roots.push_back(root);
    }
    return roots;
}

std::vector<Game> installedGames() {
    std::vector<Game> out;
    std::error_code ec;
    for (const std::string& root : installRoots()) {
        if (!fs::is_directory(root, ec)) continue;
        for (const auto& gameDir : fs::directory_iterator(root, ec)) {
            if (ec) break;
            if (!gameDir.is_directory()) continue;
            fs::path cfg = gameDir.path() / "Content" / "MicrosoftGame.config";
            if (!fs::exists(cfg, ec)) continue;
            auto parsed = parseConfig(readFile(cfg));
            if (!parsed) continue;

            Game g;
            g.store = Store::Xbox;
            g.appid = 0;                      // UWP has no numeric appid; identity is the AUMID
            g.launchId = aumid(*parsed);
            g.name = parsed->displayName;
            g.installdir = (gameDir.path() / "Content").string();
            g.fullyInstalled = true;
            out.push_back(std::move(g));
        }
    }
    std::sort(out.begin(), out.end(),
              [](const Game& a, const Game& b) { return a.name < b.name; });
    return out;
}

PlayResult launch(const std::string& aumidStr, const std::string& name, const Notify& notify) {
    if (aumidStr.empty() || aumidStr.front() == '!' || aumidStr.back() == '!')
        return PlayResult::fail("Couldn't resolve the launch id for \"" + name + "\".");
    if (notify) notify("Launching \"" + name + "\" via the Xbox app…");
    // UWP/MSIX apps launch by AUMID through the shell's AppsFolder.
    platform::spawnDetached({"explorer.exe", "shell:AppsFolder\\" + aumidStr});
    return PlayResult::success("Handed \"" + name + "\" to the Xbox app.");
}

}  // namespace ss::xbox
