#include "Backend.h"

#include "../core/covers.h"
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

}  // namespace

Backend::Backend(QObject* parent) : QObject(parent) {
    proxy_.setSourceModel(&model_);
    stores_.push_back(makeSteamStore());
    // Epic/GOG/Xbox stores get pushed here in later phases.
    pool_.setMaxThreadCount(6);
}

void Backend::setSearch(const QString& text) { proxy_.setSearchText(text); }
void Backend::setAccountFilter(const QString& filter) { proxy_.setAccountFilter(filter); }
void Backend::setSortOrder(const QString& order) { proxy_.setSortAscending(order != "za"); }

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
    for (auto& s : stores_) {
        for (const auto& g : s->scan()) {
            GameRow r;
            r.appid = g.appid;
            r.store = g.store;
            r.launchId = QString::fromStdString(g.launchId);
            r.name = QString::fromStdString(g.name);
            r.fullyInstalled = g.fullyInstalled;
            std::optional<std::string> sid;
            if (g.store == Store::Steam) sid = steam::accountForGame(g.appid, &accts);
            if (sid && personaById.count(*sid)) {
                r.mapped = true;
                r.accountId = QString::fromStdString(*sid);
                r.accountName = personaById[*sid];
                r.accountColor = QColor(color.count(*sid) ? color[*sid] : kUnmappedColor);
            } else {
                r.mapped = false;
                r.accountName = "Unmapped";
                r.accountColor = QColor(kUnmappedColor);
            }
            rows.push_back(std::move(r));
        }
    }

    // Publish on the GUI thread.
    QMetaObject::invokeMethod(this, [this, rows = std::move(rows), accountCards, current]() mutable {
        accounts_ = accountCards;
        currentAccount_ = current;
        model_.setRows(std::move(rows));
        scanning_ = false;
        emit scanningChanged();
        emit stateChanged();
    }, Qt::QueuedConnection);
}

void Backend::requestCover(qint64 appid) {
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
    cancel_ = false;
    setLaunching(true);
    emit launchStarted();
    pool_.start([this, appid, offline] {
        auto notify = [this](const std::string& m) {
            QString msg = QString::fromStdString(m);
            QMetaObject::invokeMethod(this, [this, msg] { emit status(msg); }, Qt::QueuedConnection);
        };
        auto shouldCancel = [this] { return cancel_.load(); };
        // Only the Steam store launches via the account-switch flow today.
        PlayResult res = steam::play(appid, offline, 120.0, notify, shouldCancel);
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
