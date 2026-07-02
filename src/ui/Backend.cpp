#include "Backend.h"

#include "../core/autostart.h"
#include "../core/covers.h"
#include "../core/store_covers.h"
#include "../core/settings.h"
#include "../core/steam_accounts.h"
#include "../core/steam_launcher.h"   // ss::steam::play (online + offline flag method)
#include "../core/steam_switcher.h"

#include <QBuffer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QMetaObject>
#include <QStringList>
#include <QThread>
#include <QVariant>
#include <QVariantMap>

#include <map>
#include <set>

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
// MUST stay below 2^53: the id round-trips through QML, whose numbers are doubles
// — a 2^62-range id gets rounded to the nearest representable double and no longer
// matches the gameIndex_ key (this broke non-Steam covers/launch on real HW).
constexpr qint64 kNonSteamIdBase = 1LL << 40;   // ~1.1e12, far above any Steam appid

// Per-store brand identity for the multi-store UI (chip/badge color, short label,
// and readable foreground). Mirrors the ORBIT design's store palette.
// Colors mirror the CINEMA palette (design/m1base.css --steam/--epic/--gog/--xbox
// and Theme.qml storeMeta — keep the three in sync).
struct StoreBrand { const char* key; const char* name; const char* shortName; QString color; QString fg; };
StoreBrand storeBrand(Store s) {
    switch (s) {
        case Store::Steam: return {"steam", "Steam",     "STEAM",     "#66c0f4", "#062032"};
        case Store::Epic:  return {"epic",  "Epic Games","EPIC",      "#a78bfa", "#160e2e"};
        case Store::Gog:   return {"gog",   "GOG",       "GOG",       "#c084fc", "#22093a"};
        case Store::Xbox:  return {"xbox",  "Game Pass", "GAME PASS", "#4ade80", "#0c2913"};
    }
    return {"?", "Unknown", "?", "#9aa0a6", "#ffffff"};
}
QString storeColor(Store s) { return storeBrand(s).color; }

// Store-scoped key for ORBIT's own launch history (settings.json).
std::string launchKey(Store store, const std::string& launchId) {
    return std::string(storeName(store)) + ":" + launchId;
}

}  // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    proxy_.setSourceModel(&model_);
    storeImpls_.push_back(makeSteamStore());
    storeImpls_.push_back(makeEpicStore());
    storeImpls_.push_back(makeGogStore());
    storeImpls_.push_back(makeXboxStore());
    pool_.setMaxThreadCount(6);
    language_ = QString::fromStdString(settings::language());
    onboarded_ = settings::onboarded();
    heroMode_ = QString::fromStdString(settings::heroMode());
    offlineDefault_ = settings::offlineDefault();
    runAtStartup_ = autostart::enabled();
}

void Backend::setSearch(const QString& text) { proxy_.setSearchText(text); }
void Backend::setAccountFilter(const QString& filter) { proxy_.setAccountFilter(filter); }
void Backend::setStoreFilter(const QString& store) { proxy_.setStoreFilter(store); }
void Backend::setSortOrder(const QString& order) { proxy_.setSortMode(order); }

void Backend::setHeroMode(const QString& mode) {
    if (heroMode_ == mode) return;
    heroMode_ = mode;
    settings::setHeroMode(mode.toStdString());
    emit heroModeChanged();
}

void Backend::setOfflineDefault(bool value) {
    if (offlineDefault_ == value) return;
    offlineDefault_ = value;
    settings::setOfflineDefault(value);
    emit offlineDefaultChanged();
}

bool Backend::autostartSupported() const { return autostart::supported(); }

void Backend::setRunAtStartup(bool value) {
    const std::string exe = QDir::toNativeSeparators(
        QCoreApplication::applicationFilePath()).toStdString();
    if (!autostart::setEnabled(value, exe)) {
        emit status(tr("Couldn't update the Windows startup entry."));
        return;
    }
    runAtStartup_ = autostart::enabled();
    emit runAtStartupChanged();
}

qint64 Backend::coverCacheSize() const { return covers::cacheSizeBytes(); }

void Backend::clearCoverCache() {
    int n = covers::clearCache();
    emit status(tr("Cleared %1 cached covers.").arg(n));
}

void Backend::setLanguage(const QString& lang) {
    if (lang == language_) return;
    language_ = lang;
    settings::setLanguage(lang.toStdString());   // persist (parity with set_language)
    emit languageChanged();
    // qsTr bindings retranslate live (main.cpp), but model rows carry tr()'d data
    // baked in at scan time ("Unmapped") — rebuild them in the new language.
    refresh();
}

void Backend::completeOnboarding() {
    if (onboarded_) return;
    onboarded_ = true;
    settings::setOnboarded(true);                // persist so first-run runs once
    emit onboardedChanged();
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
    // A rescan must re-read reality: drop the core's owner/usage caches so newly
    // installed games, fresh playtime and account changes show up.
    steam::clearCaches();

    // Accounts + a stable color per account by position.
    std::vector<Account> accts;
    for (auto& s : storeImpls_) {
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

    // ORBIT's own launch history — the truthful lastPlayed for non-Steam stores.
    const auto history = settings::launchHistory();
    auto historyFor = [&](Store store, const QString& launchId) -> qint64 {
        auto it = history.find(launchKey(store, launchId.toStdString()));
        return it == history.end() ? 0 : (qint64)it->second;
    };

    std::vector<GameRow> rows;
    std::map<std::string, int> gameCounts;   // steamid64 -> #games (port of counts)
    std::map<Store, int> storeCounts;        // store -> #games (sidebar STORES panel)
    std::map<qint64, GameRef> index;         // routing id -> game, for play/cover
    for (auto& s : storeImpls_) {
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
                // 32 hash bits keeps the max id ≈ 2^40+2^32, comfortably double-exact.
                qint64 id = kNonSteamIdBase | static_cast<qint64>(h & 0xFFFFFFFFULL);
                while (index.count(id)) ++id;
                r.appid = id;
            }
            index[r.appid] = GameRef{g.store, g.appid, r.launchId, r.name,
                                     QString::fromStdString(g.coverHint)};

            std::optional<std::string> sid;
            if (g.store == Store::Steam) sid = steam::accountForGame(g.appid, &accts);
            if (sid && personaById.count(*sid)) {
                r.mapped = true;
                r.accountId = QString::fromStdString(*sid);
                r.accountName = personaById[*sid];
                r.accountColor = QColor(color.count(*sid) ? color[*sid] : kUnmappedColor);
                gameCounts[*sid]++;
                // Truthful usage from the owning account's localconfig.vdf (bulk
                // map: one parse per account per scan).
                const auto& usage = steam::accountUsageMap(*sid);
                auto uit = usage.find(g.appid);
                if (uit != usage.end()) {
                    r.playtime = uit->second.playtime;
                    r.lastPlayed = uit->second.lastPlayed;
                }
            } else if (g.store != Store::Steam) {
                // Non-Steam game: no account to switch — label it by its store.
                r.mapped = true;   // not "unmapped" (that's a Steam-only state)
                r.accountName = QString::fromUtf8(storeName(g.store));
                r.accountColor = QColor(storeColor(g.store));
                // No store-side usage data — ORBIT's own launch history only
                // (playtime stays 0: unknown, never faked).
                r.lastPlayed = historyFor(g.store, r.launchId);
            } else {
                r.mapped = false;
                r.accountName = tr("Unmapped");
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
        lastScanTime_ = QDateTime::currentSecsSinceEpoch();
        scanning_ = false;
        emit scanningChanged();
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void Backend::requestCover(qint64 appid) {
    // Steam ids resolve through covers::coverBytes (librarycache/CDN/appdetails);
    // non-Steam synthetic ids through store_covers::coverBytes (Xbox local logo,
    // Epic catalog cache, GOG products API).
    Store store = Store::Steam;
    QString launchId, coverHint;
    {
        std::lock_guard<std::mutex> lk(indexMutex_);
        auto it = gameIndex_.find(appid);
        if (it != gameIndex_.end()) {
            store = it->second.store;
            launchId = it->second.launchId;
            coverHint = it->second.coverHint;
        }
    }
    pool_.start([this, appid, store, launchId, coverHint] {
        auto bytes = store == Store::Steam
                         ? covers::coverBytes(appid)
                         : store_covers::coverBytes(store, launchId.toStdString(),
                                                    coverHint.toStdString(), appid);
        qInfo().nospace() << "[cover] " << storeName(store) << " id=" << appid
                          << " hint=" << (coverHint.isEmpty() ? "(none)" : coverHint)
                          << " -> " << (bytes ? qint64(bytes->size()) : -1) << " bytes";
        QString dataUrl;
        if (bytes) {
            // Sniff the mime — Xbox logos are PNGs, Steam/network art is JPEG.
            const bool png = bytes->size() > 4 && bytes->compare(1, 3, "PNG") == 0;
            QByteArray b64 = QByteArray::fromStdString(*bytes).toBase64();
            dataUrl = QString(png ? "data:image/png;base64," : "data:image/jpeg;base64,") +
                      QString::fromLatin1(b64);
        }
        QMetaObject::invokeMethod(this, [this, appid, dataUrl] {
            emit coverReady(appid, dataUrl);
        }, Qt::QueuedConnection);
    });
}

void Backend::requestHero(qint64 appid) {
    Store store = Store::Steam;
    QString launchId, coverHint;
    {
        std::lock_guard<std::mutex> lk(indexMutex_);
        auto it = gameIndex_.find(appid);
        if (it != gameIndex_.end()) {
            store = it->second.store;
            launchId = it->second.launchId;
            coverHint = it->second.coverHint;
        }
    }
    pool_.start([this, appid, store, launchId, coverHint] {
        auto bytes = store == Store::Steam
                         ? covers::heroBytes(appid)
                         : store_covers::heroBytes(store, launchId.toStdString(),
                                                   coverHint.toStdString(), appid);
        qInfo().nospace() << "[hero] " << storeName(store) << " id=" << appid
                          << " -> " << (bytes ? qint64(bytes->size()) : -1) << " bytes";
        QString dataUrl;
        if (bytes) {
            const bool png = bytes->size() > 4 && bytes->compare(1, 3, "PNG") == 0;
            QByteArray b64 = QByteArray::fromStdString(*bytes).toBase64();
            dataUrl = QString(png ? "data:image/png;base64," : "data:image/jpeg;base64,") +
                      QString::fromLatin1(b64);
        }
        QMetaObject::invokeMethod(this, [this, appid, dataUrl] {
            emit heroReady(appid, dataUrl);
        }, Qt::QueuedConnection);
    });
}

void Backend::switchTo(const QString& steamid64) {
    bool expected = false;
    if (!launchGuard_.compare_exchange_strong(expected, true)) {
        emit status(tr("A launch is already in progress."));
        return;  // switching tears down Steam — same one-at-a-time guard as play()
    }
    cancel_ = false;
    setLaunching(true);
    emit launchStarted();
    pool_.start([this, steamid64] {
        auto notify = [this](const std::string& m) {
            QString msg = QString::fromStdString(m);
            QMetaObject::invokeMethod(this, [this, msg] { emit status(msg); }, Qt::QueuedConnection);
        };
        auto shouldCancel = [this] { return cancel_.load(); };
        PlayResult res = steam::switchTo(steamid64.toStdString(), 120.0, notify, shouldCancel);
        launchGuard_ = false;
        QMetaObject::invokeMethod(this, [this, res] {
            launching_ = false;
            emit launchingChanged();
            emit launchDone(res.ok, QString::fromStdString(res.message));
        }, Qt::QueuedConnection);
        refresh();   // logged-in badges changed
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
            for (auto& s : storeImpls_) if (s->kind() == ref.store) { store = s.get(); break; }
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
        // A real launch went out: record it in ORBIT's history (drives the
        // truthful lastPlayed/shelf ordering for non-Steam stores).
        if (res.ok && !ref.launchId.isEmpty())
            settings::recordLaunch(launchKey(ref.store, ref.launchId.toStdString()),
                                   QDateTime::currentSecsSinceEpoch());
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
