// QAbstractListModel feeding the QML game grid. Holds the aggregated, decorated
// game list (store + owning-account color/name) the backend builds. Replaces the
// JS-side game array that web/app.js kept.

#pragma once

#include "../core/model.h"

#include <QAbstractListModel>
#include <QColor>
#include <QString>
#include <vector>

namespace ss::ui {

// One row as the UI needs it (game + resolved account decoration).
struct GameRow {
    qint64 appid = 0;
    Store store = Store::Steam;
    QString launchId;
    QString name;
    bool fullyInstalled = false;
    QString accountId;          // SteamID64, empty if unmapped
    QString accountName;        // persona, "Unmapped" if none
    QColor accountColor;        // per-account palette color (or unmapped red)
    bool mapped = false;
};

class GameModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles {
        AppidRole = Qt::UserRole + 1,
        StoreRole, NameRole, FullyInstalledRole,
        AccountIdRole, AccountNameRole, AccountColorRole, MappedRole,
    };

    explicit GameModel(QObject* parent = nullptr) : QAbstractListModel(parent) {}

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setRows(std::vector<GameRow> rows);

private:
    std::vector<GameRow> rows_;
};

}  // namespace ss::ui
