#include "GameFilterModel.h"

#include "GameModel.h"

namespace ss::ui {

GameFilterModel::GameFilterModel(QObject* parent) : QSortFilterProxyModel(parent) {
    setDynamicSortFilter(true);
    sort(0, Qt::AscendingOrder);   // default A->Z
}

void GameFilterModel::setSearchText(const QString& text) {
    if (search_ == text) return;
    search_ = text;
    invalidateFilter();
}

void GameFilterModel::setAccountFilter(const QString& filter) {
    if (accountFilter_ == filter) return;
    accountFilter_ = filter;
    invalidateFilter();
}

void GameFilterModel::setStoreFilter(const QString& store) {
    if (storeFilter_ == store) return;
    storeFilter_ = store;
    invalidateFilter();
}

void GameFilterModel::setSortAscending(bool asc) {
    sort(0, asc ? Qt::AscendingOrder : Qt::DescendingOrder);
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

    if (accountFilter_ == "all") return true;
    if (accountFilter_ == "unmapped") return !idx.data(GameModel::MappedRole).toBool();
    return idx.data(GameModel::AccountIdRole).toString() == accountFilter_;
}

bool GameFilterModel::lessThan(const QModelIndex& left, const QModelIndex& right) const {
    QString a = left.data(GameModel::NameRole).toString();
    QString b = right.data(GameModel::NameRole).toString();
    return a.compare(b, Qt::CaseInsensitive) < 0;
}

}  // namespace ss::ui
