#include "Backend.h"

#include "../core/covers.h"
#include "../core/settings.h"
#include "../core/steam_accounts.h"
#include "../core/steam_launcher.h"   // ss::steam::play (online + offline flag method)
#include "../core/steam_switcher.h"

#include <QBuffer>
#include <QMetaObject>
#include <QStringList>
#include <QThread>
#include <QVariant>
#include <QVariantMap>

#include <map>

namespace ss::ui {

namespace {

// Distinct, vivid account colors, assigned by list position (port of server.py
// _ACCOUNT_PALETTE). Unmapped games use the red.
const QStringList kPalette = {
    "#58a6ff", "#57cc99", "#ff8a65", "#c792ea", "#ff6384",
    "#ffd166", "#26c6da", "#f071b2", "#7cb342", "#9575cd",
};
const QString kUnmappedColor = "#c94f4f";

// Non-Steam stores have no numeric appid; Backend hands their games a stable
// synthetic id in this high range so the appid-keyed UI can address them without
// colliding with real Steam appids (which are small positives).
constexpr qint64 kNonSteamIdBase = 0x4000000000000000LL;

// Per-store brand identity for the multi-store UI (chip/badge color, short label,
// and readable foreground). Mirrors the ORBIT design's store palette.
struct StoreBrand { const char* key; const char* name; const char* shortName; QString color; QString fg; };
StoreBrand storeBrand(Store s) {
    switch (s) {
        case Store::Steam: return {"steam", "Steam",     "STEAM",     "#2a7fd4", "#ffffff"};
        case Store::Epic:  return {"epic",  "Epic Games","EPIC",      "#d9d9e0", "#0a0a0c"};
        case Store::Gog:   return {"gog",   "GOG",       "GOG",       "#9b4dde", "#ffffff"};
        case Store::Xbox:  return {"xbox",  "Game Pass", "GAME PASS", "#3fae4f", "#ffffff"};
    }
    return {"?", "Unknown", "?", "#9aa0a6", "#ffffff"};
}
QString storeColor(Store s) { return storeBrand(s).color; }

}  // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    proxy_.setSourceModel(&model_);
    stores_.push_back(makeSteamStore());
    stores_.push_back(makeEpicStore());
    stores_.push_back(makeGogStore());
    stores_.push_back(makeXboxStore());
    pool_.setMaxThreadCount(6);
    language_ = QString::fromStdString(settings::language());
}

void Backend::setSearch(const QString& text) { proxy_.setSearchText(text); }
void Backend::setAccountFilter(const QString& filter) { proxy_.setAccountFilter(filter); }
void Backend::setStoreFilter(const QString& store) { proxy_.setStoreFilter(store); }
void Backend::setSortOrder(const QString& order) { proxy_.setSortAscending(order != "za"); }

void Backend::setLanguage(const QString& lang) {
    if (lang == language_) return;
    language_ = lang;
    settings::setLanguage(lang.toStdString());   // persist (parity with set_language)
    emit languageChanged();
}

void Backend::setScanning(bool v) {
    if (scanning_ == v) return;
    scanning_ = v;
    QMetaObject::invokeMethod(this, [this] { emit scanningChanged(); }, Qt::QueuedConnection);
}

void Backend::setLaunching(bool v) {
    if (launching_ == v) return;
    launching_ = v;
    QMetaObject::invokeMethod(this, [this] { emit launchingChanged(); }, Qt::QueuedConnection);
}

void Backend::refresh() {
    setScanning(true);
    pool_.start([this] { buildState(); });
}

void Backend::buildState() {
    // Accounts + a stable color per account by position.
    std::vector<Account> accts;
    for (auto& s : stores_) {
        auto a = s->accounts();
        accts.insert(accts.end(), a.begin(), a.end());
    }
    std::map<std::string, QString> color;
    QVariantList accountCards;
    int i = 0;
    QString current;
    for (const auto& a : accts) {
        QString c = kPalette[i % kPalette.size()];
        color[a.steamid64] = c;
        auto [ready, why] = steam::switcher::canAutologin(a.accountName);
        QVariantMap card;
        card["steamid64"] = QString::fromStdString(a.steamid64);
        card["accountName"] = QString::fromStdString(a.accountName);
        card["personaName"] = QString::fromStdString(a.personaName);
        card["color"] = c;
        card["ready"] = ready;
        card["readyReason"] = QString::fromStdString(why);
        card["loggedIn"] = steam::switcher::isLoggedInAs(a.steamid64);
        accountCards.push_back(card);
        if (a.mostRecent) current = QString::fromStdString(a.personaName);
        ++i;
    }

    // Game rows across all stores, decorated with the owning account.
    std::map<std::string, QString> personaById;
    for (const auto& a : accts) personaById[a.steamid64] = QString::fromStdString(a.personaName);

    std::vector<GameRow> rows;
    std::map<std::string, int> gameCounts;   // steamid64 -> #games (port of counts)
    std::map<Store, int> storeCounts;        // store -> #games (sidebar STORES panel)
    std::map<qint64, GameRef> index;         // routing id -> game, for play/cover
    for (auto& s : stores_) {
        for (const auto& g : s->scan()) {
            storeCounts[g.store]++;
            GameRow r;
            r.store = g.store;
            r.launchId = QString::fromStdString(g.launchId);
            r.name = QString::fromStdString(g.name);
            r.fullyInstalled = g.fullyInstalled;

            // Assign the id the UI addresses this game by. Steam = real appid;
            // other stores get a stable synthetic id (probed for uniqueness).
            if (g.store == Store::Steam) {
                r.appid = g.appid;
            } else {
                std::size_t h = std::hash<std::string>{}(g.launchId);
                qint64 id = kNonSteamIdBase | static_cast<qint64>(h & 0x3FFFFFFFFFFFFFFFULL);
                while (index.count(id)) ++id;
                r.appid = id;
            }
            index[r.appid] = GameRef{g.store, g.appid, r.launchId, r.name};

            std::optional<std::string> sid;
            if (g.store == Store::Steam) sid = steam::accountForGame(g.appid, &accts);
            if (sid && personaById.count(*sid)) {
                r.mapped = true;
                r.accountId = QString::fromStdString(*sid);
                r.accountName = personaById[*sid];
                r.accountColor = QColor(color.count(*sid) ? color[*sid] : kUnmappedColor);
                gameCounts[*sid]++;
            } else if (g.store != Store::Steam) {
                // Non-Steam game: no account to switch — label it by its store.
                r.mapped = true;   // not "unmapped" (that's a Steam-only state)
                r.accountName = QString::fromUtf8(storeName(g.store));
                r.accountColor = QColor(storeColor(g.store));
            } else {
                r.mapped = false;
                r.accountName = "Unmapped";
                r.accountColor = QColor(kUnmappedColor);
            }
            rows.push_back(std::move(r));
        }
    }

    // Fold the per-account game counts into the account cards.
    for (QVariant& v : accountCards) {
        QVariantMap card = v.toMap();
        card["gameCount"] = gameCounts.count(card["steamid64"].toString().toStdString())
                                ? gameCounts[card["steamid64"].toString().toStdString()] : 0;
        v = card;
    }

    { std::lock_guard<std::mutex> lk(indexMutex_); gameIndex_ = std::move(index); }

    // Per-store cards for the sidebar STORES panel / Accounts view / Manage panel.
    QVariantList storeCards;
    for (Store s : {Store::Steam, Store::Epic, Store::Gog, Store::Xbox}) {
        StoreBrand b = storeBrand(s);
        int n = storeCounts.count(s) ? storeCounts[s] : 0;
        QVariantMap card;
        card["key"] = b.key;
        card["name"] = b.name;                    // brand label ("Epic Games")
        card["storeName"] = storeName(s);          // core token for filtering ("Epic")
        card["shortName"] = b.shortName;
        card["color"] = b.color;
        card["textColor"] = b.fg;
        card["count"] = n;
        card["connected"] = n > 0;   // a store is "connected" if it has installed games
        card["isSteam"] = (s == Store::Steam);
        storeCards.push_back(card);
    }

    // Publish on the GUI thread.
    QMetaObject::invokeMethod(this, [this, rows = std::move(rows), accountCards, storeCards, current]() mutable {
        accounts_ = accountCards;
        stores_ = storeCards;
        currentAccount_ = current;
        model_.setRows(std::move(rows));
        scanning_ = false;
        emit scanningChanged();
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void Backend::requestCover(qint64 appid) {
    // covers::coverBytes resolves Steam CDN/librarycache art by appid; a non-Steam
    // synthetic id has no such art, so don't waste a network round-trip on it.
    // (Per-store cover resolvers land with the multi-store UI.)
    {
        std::lock_guard<std::mutex> lk(indexMutex_);
        auto it = gameIndex_.find(appid);
        if (it != gameIndex_.end() && it->second.store != Store::Steam) {
            QMetaObject::invokeMethod(this, [this, appid] { emit coverReady(appid, QString()); },
                                      Qt::QueuedConnection);
            return;
        }
    }
    pool_.start([this, appid] {
        auto bytes = covers::coverBytes(appid);
        QString dataUrl;
        if (bytes) {
            QByteArray b64 = QByteArray::fromStdString(*bytes).toBase64();
            dataUrl = "data:image/jpeg;base64," + QString::fromLatin1(b64);
        }
        QMetaObject::invokeMethod(this, [this, appid, dataUrl] {
            emit coverReady(appid, dataUrl);
        }, Qt::QueuedConnection);
    });
}

void Backend::play(qint64 appid, bool offline) {
    bool expected = false;
    if (!launchGuard_.compare_exchange_strong(expected, true)) {
        emit status("A launch is already in progress.");
        return;  // one launch at a time
    }

    // Resolve which store/game the UI id refers to.
    GameRef ref;
    {
        std::lock_guard<std::mutex> lk(indexMutex_);
        auto it = gameIndex_.find(appid);
        if (it != gameIndex_.end()) ref = it->second;
        else ref = GameRef{Store::Steam, appid, {}, QString::number(appid)};
    }

    // Steam pre-flight: an unmapped game has no owner to switch to, so don't tear
    // down and restart Steam for nothing (parity with server.py's play() guard).
    if (ref.store == Store::Steam && !steam::accountForGame(ref.appid, nullptr)) {
        launchGuard_ = false;
        emit launchDone(false, QStringLiteral("\"%1\" isn't mapped to an account yet.").arg(ref.name));
        return;
    }

    cancel_ = false;
    setLaunching(true);
    emit launchStarted();
    pool_.start([this, ref, offline] {
        auto notify = [this](const std::string& m) {
            QString msg = QString::fromStdString(m);
            QMetaObject::invokeMethod(this, [this, msg] { emit status(msg); }, Qt::QueuedConnection);
        };
        auto shouldCancel = [this] { return cancel_.load(); };

        PlayResult res;
        if (ref.store == Store::Steam) {
            // Steam launches via the account-switch flow (online / offline flag method).
            res = steam::play(ref.appid, offline, 120.0, notify, shouldCancel);
        } else {
            // Other stores just enumerate + fire their launch URI (no switching).
            IStore* store = nullptr;
            for (auto& s : stores_) if (s->kind() == ref.store) { store = s.get(); break; }
            if (store) {
                Game g;
                g.store = ref.store;
                g.appid = ref.appid;
                g.launchId = ref.launchId.toStdString();
                g.name = ref.name.toStdString();
                res = store->launch(g, LaunchOptions{offline}, notify, shouldCancel);
            } else {
                res = PlayResult::fail("That store isn't available.");
            }
        }
        launchGuard_ = false;
        QMetaObject::invokeMethod(this, [this, res] {
            launching_ = false;
            emit launchingChanged();
            emit launchDone(res.ok, QString::fromStdString(res.message));
        }, Qt::QueuedConnection);
        // Logins/switches change account state — refresh so badges update.
        refresh();
    });
}

void Backend::cancel() { cancel_ = true; }

void Backend::addAccount() {
    pool_.start([this] {
        bool ok = steam::switcher::restartToAddAccount();
        QString msg = ok ? "Steam is restarting at its login screen — sign into the new account."
                         : "Couldn't restart Steam to add an account.";
        QMetaObject::invokeMethod(this, [this, msg] { emit status(msg); }, Qt::QueuedConnection);
        // Give Steam time to come up, then refresh so the new account appears (~45s).
        QThread::sleep(45);
        refresh();
    });
}

void Backend::pinToAccount(qint64 appid, const QString& steamid64) {
    steam::setOverride(appid, steamid64.toStdString());
    refresh();
}

}  // namespace ss::ui
