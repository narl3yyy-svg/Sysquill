// SPDX-License-Identifier: GPL-3.0-or-later
#include "processmodel.h"
#include "format.h"

#include <QSet>

namespace {
QString stateName(char s)
{
    switch (s) {
    case 'R': return QStringLiteral("Running");
    case 'S': return QStringLiteral("Sleeping");
    case 'D': return QStringLiteral("Disk wait");
    case 'Z': return QStringLiteral("Zombie");
    case 'T': return QStringLiteral("Stopped");
    case 't': return QStringLiteral("Traced");
    case 'I': return QStringLiteral("Idle");
    case 'X': return QStringLiteral("Dead");
    default: return QString(QLatin1Char(s));
    }
}
} // namespace

ProcessModel::ProcessModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

int ProcessModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : int(m_rows.size());
}

int ProcessModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QString ProcessModel::columnTitle(int c)
{
    switch (c) {
    case Name: return tr("Name");
    case Pid: return tr("PID");
    case User: return tr("User");
    case Cpu: return tr("CPU");
    case Memory: return tr("Memory");
    case DiskRead: return tr("Disk read");
    case DiskWrite: return tr("Disk write");
    case Gpu: return tr("GPU");
    case Threads: return tr("Threads");
    case State: return tr("State");
    case Command: return tr("Command line");
    default: return {};
    }
}

bool ProcessModel::isNumeric(int c)
{
    return c == Pid || c == Cpu || c == Memory || c == DiskRead || c == DiskWrite || c == Gpu || c == Threads;
}

QVariant ProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
        return columnTitle(section);
    return {};
}

QHash<int, QByteArray> ProcessModel::roleNames() const
{
    return {
        {Qt::DisplayRole, "display"},
        {SortRole, "sortValue"},
        {PidRole, "pid"},
        {KernelRole, "kernel"},
        {HeatRole, "heat"},
        {NumericRole, "numeric"},
    };
}

QVariant ProcessModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const ProcSample &p = m_rows.at(index.row());
    const int c = index.column();
    switch (role) {
    case Qt::DisplayRole:
        switch (c) {
        case Name: return p.kernel ? QStringLiteral("[%1]").arg(p.name) : p.name;
        case Pid: return QString::number(p.pid);
        case User: return p.user;
        case Cpu: return QString::number(p.cpu, 'f', 1) + QStringLiteral(" %");
        case Memory: return fmt::bytes(double(p.rss));
        case DiskRead: return p.readRate < 0 ? QStringLiteral("–") : fmt::rate(p.readRate);
        case DiskWrite: return p.writeRate < 0 ? QStringLiteral("–") : fmt::rate(p.writeRate);
        case Gpu: return p.gpu < 0 ? QString() : QString::number(p.gpu, 'f', 1) + QStringLiteral(" %");
        case Threads: return QString::number(p.threads);
        case State: return stateName(p.state);
        case Command: return p.cmdline.isEmpty() ? QStringLiteral("[%1]").arg(p.name) : p.cmdline;
        }
        break;
    case SortRole:
        switch (c) {
        case Name: return p.name.toLower();
        case Pid: return double(p.pid);
        case User: return p.user;
        case Cpu: return p.cpu;
        case Memory: return double(p.rss);
        case DiskRead: return p.readRate;
        case DiskWrite: return p.writeRate;
        case Gpu: return p.gpu;
        case Threads: return double(p.threads);
        case State: return stateName(p.state);
        case Command: return p.cmdline;
        }
        break;
    case PidRole: return p.pid;
    case KernelRole: return p.kernel;
    case NumericRole: return isNumeric(c);
    case HeatRole:
        switch (c) {
        case Cpu: return qBound(0.0, p.cpu / 50.0, 1.0);
        case Memory: return qBound(0.0, double(p.rss) / (2.0 * 1024 * 1024 * 1024), 1.0);
        case DiskRead: return qBound(0.0, p.readRate / (50.0 * 1024 * 1024), 1.0);
        case DiskWrite: return qBound(0.0, p.writeRate / (50.0 * 1024 * 1024), 1.0);
        case Gpu: return qBound(0.0, p.gpu / 50.0, 1.0);
        default: return 0.0;
        }
    }
    return {};
}

const ProcSample *ProcessModel::byPid(int pid) const
{
    auto it = m_index.constFind(pid);
    return it == m_index.constEnd() ? nullptr : &m_rows.at(it.value());
}

void ProcessModel::update(const QList<ProcSample> &procs)
{
    QHash<int, int> incoming;
    incoming.reserve(procs.size());
    for (int i = 0; i < procs.size(); ++i)
        incoming.insert(procs[i].pid, i);

    // Remove processes that went away (contiguous ranges, from the end)
    int r = int(m_rows.size()) - 1;
    while (r >= 0) {
        if (incoming.contains(m_rows[r].pid)) {
            --r;
            continue;
        }
        int first = r;
        while (first > 0 && !incoming.contains(m_rows[first - 1].pid))
            --first;
        beginRemoveRows({}, first, r);
        m_rows.remove(first, r - first + 1);
        endRemoveRows();
        r = first - 1;
    }

    // Update the rest in place
    QSet<int> present;
    for (int i = 0; i < m_rows.size(); ++i) {
        m_rows[i] = procs[incoming.value(m_rows[i].pid)];
        present.insert(m_rows[i].pid);
    }
    if (!m_rows.isEmpty())
        Q_EMIT dataChanged(index(0, 0), index(int(m_rows.size()) - 1, ColumnCount - 1));

    // Append new ones
    QList<ProcSample> added;
    for (const ProcSample &p : procs)
        if (!present.contains(p.pid))
            added.append(p);
    if (!added.isEmpty()) {
        beginInsertRows({}, int(m_rows.size()), int(m_rows.size() + added.size()) - 1);
        m_rows.append(added);
        endInsertRows();
    }

    m_index.clear();
    for (int i = 0; i < m_rows.size(); ++i)
        m_index.insert(m_rows[i].pid, i);
}

// ---------------------------------------------------------------------------

ProcessProxy::ProcessProxy(QObject *parent)
    : QSortFilterProxyModel(parent)
{
    setSortRole(ProcessModel::SortRole);
    setDynamicSortFilter(true);
    connect(this, &QAbstractItemModel::rowsInserted, this, &ProcessProxy::countChanged);
    connect(this, &QAbstractItemModel::rowsRemoved, this, &ProcessProxy::countChanged);
    connect(this, &QAbstractItemModel::modelReset, this, &ProcessProxy::countChanged);
    connect(this, &QAbstractItemModel::layoutChanged, this, &ProcessProxy::countChanged);
    sort(ProcessModel::Cpu, Qt::DescendingOrder);
}

void ProcessProxy::setFilterText(const QString &t)
{
    if (t == m_filter)
        return;
    refilter([&] { m_filter = t; });
    Q_EMIT filterChanged();
    Q_EMIT countChanged();
}

void ProcessProxy::setShowKernel(bool on)
{
    if (on == m_showKernel)
        return;
    refilter([&] { m_showKernel = on; });
    Q_EMIT filterChanged();
    Q_EMIT countChanged();
}

void ProcessProxy::sortBy(int column)
{
    Qt::SortOrder order;
    if (column == sortColumn())
        order = sortOrder() == Qt::AscendingOrder ? Qt::DescendingOrder : Qt::AscendingOrder;
    else
        order = (ProcessModel::isNumeric(column) && column != ProcessModel::Pid) ? Qt::DescendingOrder : Qt::AscendingOrder;
    sort(column, order);
    Q_EMIT sortChanged();
}

static const ProcSample *lookup(const QAbstractItemModel *m, int pid)
{
    auto *pm = qobject_cast<const ProcessModel *>(m);
    return pm ? pm->byPid(pid) : nullptr;
}

QString ProcessProxy::nameOfPid(int pid) const
{
    const ProcSample *p = lookup(sourceModel(), pid);
    return p ? p->name : QString();
}

QString ProcessProxy::userOfPid(int pid) const
{
    const ProcSample *p = lookup(sourceModel(), pid);
    return p ? p->user : QString();
}

QString ProcessProxy::commandOfPid(int pid) const
{
    const ProcSample *p = lookup(sourceModel(), pid);
    return p ? p->cmdline : QString();
}

bool ProcessProxy::filterAcceptsRow(int sourceRow, const QModelIndex &) const
{
    auto *pm = qobject_cast<const ProcessModel *>(sourceModel());
    if (!pm)
        return true;
    const ProcSample &p = pm->at(sourceRow);
    if (!m_showKernel && p.kernel)
        return false;
    if (m_filter.isEmpty())
        return true;
    return p.name.contains(m_filter, Qt::CaseInsensitive) || p.cmdline.contains(m_filter, Qt::CaseInsensitive)
           || p.user.contains(m_filter, Qt::CaseInsensitive) || QString::number(p.pid).startsWith(m_filter);
}

bool ProcessProxy::lessThan(const QModelIndex &left, const QModelIndex &right) const
{
    const QVariant l = left.data(ProcessModel::SortRole);
    const QVariant r = right.data(ProcessModel::SortRole);
    if (l.typeId() == QMetaType::QString) {
        const int c = QString::localeAwareCompare(l.toString(), r.toString());
        if (c != 0)
            return c < 0;
    } else {
        const double a = l.toDouble(), b = r.toDouble();
        if (a != b)
            return a < b;
    }
    // Stable tie-breaker so rows don't shuffle
    return left.data(ProcessModel::PidRole).toInt() < right.data(ProcessModel::PidRole).toInt();
}
