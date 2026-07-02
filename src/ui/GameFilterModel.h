// Search + account/store filter + sort over the GameModel, done in C++ with a
// proper QSortFilterProxyModel (the QML DelegateModel group-filter approach
// silently hides rows that arrive after a background scan — this doesn't).
//
// QML-instantiable (QML_ELEMENT): each CINEMA shelf creates its own instance
// over Backend.allGames, so every shelf is an independent live filter of the
// single scanned model. The Backend's main proxy (Backend.games) is one of these
// too, driven by the setSearch/setAccountFilter/… invokables.

#pragma once

#include <QSortFilterProxyModel>
#include <QString>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>

namespace ss::ui {

class GameFilterModel : public QSortFilterProxyModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(GameFilter)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY filterChanged)
    Q_PROPERTY(QString accountFilter READ accountFilter WRITE setAccountFilter NOTIFY filterChanged)
    Q_PROPERTY(QString storeFilter READ storeFilter WRITE setStoreFilter NOTIFY filterChanged)
    Q_PROPERTY(QString sortMode READ sortMode WRITE setSortMode NOTIFY filterChanged)
    Q_PROPERTY(bool playedOnly READ playedOnly WRITE setPlayedOnly NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    explicit GameFilterModel(QObject* parent = nullptr);

    QString searchText() const { return search_; }
    QString accountFilter() const { return accountFilter_; }
    QString storeFilter() const { return storeFilter_; }
    QString sortMode() const { return sortMode_; }
    bool playedOnly() const { return playedOnly_; }
    int count() const { return rowCount(); }

    void setSearchText(const QString& text);
    void setAccountFilter(const QString& filter);   // "all" | steamid64 | "unmapped"
    void setStoreFilter(const QString& store);      // "all" | store name ("Steam"/…)
    void setSortMode(const QString& mode);          // "az" | "za" | "recent"
    void setPlayedOnly(bool v);                     // only rows with lastPlayed > 0

    // All roles of one (proxied) row as a JS object — lets QML read a row
    // without a delegate (hero pick, palette Enter-to-play).
    Q_INVOKABLE QVariantMap gameAt(int row) const;

signals:
    void filterChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;
    bool lessThan(const QModelIndex& left, const QModelIndex& right) const override;

private:
    QString search_;
    QString accountFilter_ = "all";
    QString storeFilter_ = "all";
    QString sortMode_ = "az";
    bool playedOnly_ = false;
};

}  // namespace ss::ui
