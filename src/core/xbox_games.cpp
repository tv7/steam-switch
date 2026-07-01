#include "xbox_games.h"

#include "platform.h"
#include "sha256.h"

#include <algorithm>
#include <cctype>
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
    std::string needle = "<" + element;
    std::size_t pos = 0;
    while ((pos = xml.find(needle, pos)) != std::string::npos) {
        // The char after the element name must be a tag delimiter, so `Executable`
        // doesn't spuriously match `<ExecutableList>` (a prefix). Without this the
        // real `<Executable Id="…">` inside `<ExecutableList>` is missed and the
        // AppId (→ AUMID) is wrong.
        char after = (pos + needle.size() < xml.size()) ? xml[pos + needle.size()] : '>';
        if (after == ' ' || after == '\t' || after == '\r' || after == '\n' ||
            after == '>' || after == '/') {
            auto end = xml.find('>', pos);
            if (end == std::string::npos) return "";
            return xml.substr(pos, end - pos);
        }
        pos += needle.size();
    }
    return "";
}

// Text content of the first <element>text</element> in `xml`, or "".
std::string elemText(const std::string& xml, const std::string& element) {
    std::string open = openTag(xml, element);
    if (open.empty()) return "";
    auto start = xml.find(open);
    if (start == std::string::npos) return "";
    start += open.size() + 1;                       // past the '>'
    auto end = xml.find("</" + element, start);
    if (end == std::string::npos) return "";
    return xml.substr(start, end - start);
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
    // Local cover art ships with the game: prefer the big square logo, then the
    // store logo. Paths are relative to the Content dir (where the config lives).
    for (const char* key : {"Square480x480Logo", "StoreLogo", "Square150x150Logo"}) {
        std::string p = attr(shell, key);
        if (!p.empty()) { c.logoPath = p; break; }
    }
    c.storeId = elemText(xml, "StoreId");
    // Some configs use an ms-resource: indirection we can't resolve — fall back.
    c.displayName = (display.empty() || display.rfind("ms-resource:", 0) == 0)
                        ? c.identityName : display;

    // A launchable game declares an <Executable> (inside <ExecutableList>). DLC and
    // pre-order/launch stubs (BO6/BO7 packs, "Game Stub", "Launch Tracker", …) have
    // no executable — they carry <TargetDeviceFamilyForDLC>/<MainPackageDependency>
    // instead — so skip them rather than surfacing fake tiles.
    std::string exe = openTag(xml, "Executable");
    if (exe.empty()) return std::nullopt;
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
    // Game Pass installs into a folder on whichever drive the user picks. The Xbox
    // app's default name is "XboxGames", but users who install to another drive
    // sometimes name it differently ("xbox", "Xbox Games", …). Rather than hardcode
    // the name, enumerate each drive's TOP-LEVEL folders (cheap — no deep scan) and
    // treat any whose name contains "xbox" (case-insensitive) as an install root.
    auto containsXbox = [](std::string name) {
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return name.find("xbox") != std::string::npos;
    };
    std::error_code ec;
    for (char d = 'C'; d <= 'Z'; ++d) {
        std::string drive = std::string(1, d) + ":\\";
        if (!fs::is_directory(drive, ec)) continue;
        for (const auto& e : fs::directory_iterator(drive, ec)) {
            if (ec) break;
            std::error_code ec2;
            if (e.is_directory(ec2) && containsXbox(e.path().filename().string()))
                roots.push_back(e.path().string());
        }
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
            // Cover hint = "<StoreId>|<absolute logo path>" ('|' is illegal in
            // Windows paths, so it's a safe separator). store_covers.cpp fetches
            // the real box art from the MS Store display catalog by StoreId; the
            // shipped logo PNG is the offline fallback (often a low-res or engine
            // logo — e.g. UE's — so it must not be the primary source).
            std::string logoAbs;
            if (!parsed->logoPath.empty()) {
                fs::path logo = fs::path(g.installdir) / parsed->logoPath;
                if (fs::exists(logo, ec)) logoAbs = logo.string();
            }
            if (!parsed->storeId.empty() || !logoAbs.empty())
                g.coverHint = parsed->storeId + "|" + logoAbs;
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
