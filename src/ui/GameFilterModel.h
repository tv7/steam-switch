// Search + account-filter + sort over the GameModel, done in C++ with a proper
// QSortFilterProxyModel (the QML DelegateModel group-filter approach silently
// hides rows that arrive after a background scan — this doesn't). Mirrors
// web/app.js visibleGames(): search (name substring) -> account filter -> sort.

#pragma once

#include <QSortFilterProxyModel>
#include <QString>

namespace ss::ui {

class GameFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
public:
    explicit GameFilterModel(QObject* parent = nullptr);

    void setSearchText(const QString& text);
    void setAccountFilter(const QString& filter);   // "all" | steamid64 | "unmapped"
    void setStoreFilter(const QString& store);      // "all" | store name ("Steam"/…)
    void setSortAscending(bool asc);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString search_;
    QString accountFilter_ = "all";
    QString storeFilter_ = "all";
};

}  // namespace ss::ui
