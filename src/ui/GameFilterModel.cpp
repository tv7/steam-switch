#include "GameFilterModel.h"

#include "GameModel.h"

namespace ss::ui {

GameFilterModel::GameFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);   // default A->Z
    // Live row count for shelves (hide a shelf when its filter matches nothing).
    for (auto sig : {&QAbstractItemModel::rowsInserted, &QAbstractItemModel::rowsRemoved}) {
        connect(this, sig, this, [this] { emit countChanged(); });
    }
    connect(this, &QAbstractItemModel::modelReset, this, [this] { emit countChanged(); });
    connect(this, &QAbstractItemModel::layoutChanged, this, [this] { emit countChanged(); });
}

void GameFilterModel::setSearchText(const QString& text) {
    if (search_ == text) return;
    search_ = text;
    invalidateFilter();
    emit filterChanged();
}

void GameFilterModel::setAccountFilter(const QString& filter) {
    if (accountFilter_ == filter) return;
    accountFilter_ = filter;
    invalidateFilter();
    emit filterChanged();
}

void GameFilterModel::setStoreFilter(const QString& store) {
    if (storeFilter_ == store) return;
    storeFilter_ = store;
    invalidateFilter();
    emit filterChanged();
}

void GameFilterModel::setSortMode(const QString& mode) {
    if (sortMode_ == mode) return;
    sortMode_ = mode;
    // lessThan encodes the comparison; the Qt order only flips A->Z / Z->A.
    sort(0, mode == "za" ? Qt::DescendingOrder : Qt::AscendingOrder);
    invalidate();
    emit filterChanged();
}

void GameFilterModel::setPlayedOnly(bool v) {
    if (playedOnly_ == v) return;
    playedOnly_ = v;
    invalidateFilter();
    emit filterChanged();
}

QVariantMap GameFilterModel::gameAt(int row) const {
    QVariantMap out;
    QModelIndex idx = index(row, 0);
    if (!idx.isValid()) return out;
    const auto roles = roleNames();
    for (auto it = roles.constBegin(); it != roles.constEnd(); ++it)
        out[QString::fromUtf8(it.value())] = idx.data(it.key());
    return out;
}

bool GameFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    QModelIndex idx = sourceModel()->index(sourceRow, 0, sourceParent);
    if (!idx.isValid()) return false;

    if (!search_.isEmpty()) {
        QString name = idx.data(GameModel::NameRole).toString();
        if (!name.contains(search_, Qt::CaseInsensitive)) return false;
    }
    if (storeFilter_ != "all" &&
        idx.data(GameModel::StoreRole).toString() != storeFilter_) return false;
    if (playedOnly_ && idx.data(GameModel::LastPlayedRole).toLongLong() <= 0) return false;

    if (accountFilter_ == "all") return true;
    if (accountFilter_ == "unmapped") return !idx.data(GameModel::MappedRole).toBool();
    return idx.data(GameModel::AccountIdRole).toString() == accountFilter_;
}

bool GameFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    if (sortMode_ == "recent") {
        qint64 a = left.data(GameModel::LastPlayedRole).toLongLong();
        qint64 b = right.data(GameModel::LastPlayedRole).toLongLong();
        if (a != b) return a > b;   // most recent first (ascending Qt order)
    }
    QString a = left.data(GameModel::NameRole).toString();
    QString b = right.data(GameModel::NameRole).toString();
    return a.compare(b, Qt::CaseInsensitive) < 0;
}

}  // namespace ss::ui
