// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "snapshot.h"

#include <QAbstractTableModel>
#include <QHash>
#include <QSortFilterProxyModel>
#include <QtQml/qqmlregistration.h>

class ProcessModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { Name, Pid, User, Cpu, Memory, DiskRead, DiskWrite, Gpu, Threads, State, Command, ColumnCount };
    enum Roles { SortRole = Qt::UserRole + 1, PidRole, KernelRole, HeatRole, NumericRole };

    explicit ProcessModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    int columnCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void update(const QList<ProcSample> &procs);
    const ProcSample &at(int row) const { return m_rows.at(row); }
    const ProcSample *byPid(int pid) const;

    static QString columnTitle(int column);
    static bool isNumeric(int column);

private:
    QList<ProcSample> m_rows;
    QHash<int, int> m_index;
};

class ProcessProxy : public QSortFilterProxyModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Use Sys.processes")
    Q_PROPERTY(QString filterText READ filterText WRITE setFilterText NOTIFY filterChanged)
    Q_PROPERTY(bool showKernel READ showKernel WRITE setShowKernel NOTIFY filterChanged)
    Q_PROPERTY(int sortColumnIndex READ sortColumnIndex NOTIFY sortChanged)
    Q_PROPERTY(bool descending READ descending NOTIFY sortChanged)
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int columns READ columns CONSTANT)
public:
    explicit ProcessProxy(QObject *parent = nullptr);

    QString filterText() const { return m_filter; }
    void setFilterText(const QString &t);
    bool showKernel() const { return m_showKernel; }
    void setShowKernel(bool on);
    int sortColumnIndex() const { return sortColumn(); }
    bool descending() const { return sortOrder() == Qt::DescendingOrder; }
    int count() const { return rowCount(); }
    int columns() const { return ProcessModel::ColumnCount; }

    Q_INVOKABLE void sortBy(int column);
    Q_INVOKABLE QString columnTitle(int column) const { return ProcessModel::columnTitle(column); }
    Q_INVOKABLE bool columnNumeric(int column) const { return ProcessModel::isNumeric(column); }
    Q_INVOKABLE QString nameOfPid(int pid) const;
    Q_INVOKABLE QString userOfPid(int pid) const;
    Q_INVOKABLE QString commandOfPid(int pid) const;

Q_SIGNALS:
    void filterChanged();
    void sortChanged();
    void countChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;
    bool lessThan(const QModelIndex &left, const QModelIndex &right) const override;

private:
    template<typename F>
    void refilter(F change)
    {
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
        beginFilterChange();
        change();
        endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
        change();
        invalidateFilter();
#endif
    }

    QString m_filter;
    bool m_showKernel = false;
};
