#include "GameModel.h"

namespace ss::ui {

int GameModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : (int)rows_.size();
}

QVariant GameModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= (int)rows_.size())
        return {};
    const GameRow& r = rows_[index.row()];
    switch (role) {
        case AppidRole: return r.appid;
        case StoreRole: return QString::fromUtf8(storeName(r.store));
        case NameRole: return r.name;
        case FullyInstalledRole: return r.fullyInstalled;
        case AccountIdRole: return r.accountId;
        case AccountNameRole: return r.accountName;
        case AccountColorRole: return r.accountColor;
        case MappedRole: return r.mapped;
        case PlaytimeRole: return r.playtime;
        case LastPlayedRole: return r.lastPlayed;
        default: return {};
    }
}

QHash<int, QByteArray> GameModel::roleNames() const {
    return {
        {AppidRole, "appid"}, {StoreRole, "store"}, {NameRole, "name"},
        {FullyInstalledRole, "fullyInstalled"}, {AccountIdRole, "accountId"},
        {AccountNameRole, "accountName"}, {AccountColorRole, "accountColor"},
        {MappedRole, "mapped"},
        {PlaytimeRole, "playtime"}, {LastPlayedRole, "lastPlayed"},
    };
}

void GameModel::setRows(std::vector<GameRow> rows) {
    beginResetModel();
    rows_ = std::move(rows);
    endResetModel();
}

}  // namespace ss::ui
