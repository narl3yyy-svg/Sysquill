// SPDX-License-Identifier: GPL-3.0-or-later
// Per-process details that need access to /proc/<pid>/{io,fd,fdinfo}.
// Shared by the app (for the user's own processes) and by the privileged
// helper (for everyone's processes).
#pragma once

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QPair>
#include <QString>
#include <QVector>

struct DrmClient {
    QString key;                                     // pdev + "#" + client id
    QString pdev;                                    // PCI slot, e.g. 0000:00:02.0
    QHash<QString, quint64> engineNs;                // i915/amdgpu: busy time per engine
    QHash<QString, int> capacity;                    // engine instances
    QHash<QString, QPair<quint64, quint64>> cycles;  // xe: (cycles, total cycles)
};

struct PidExtras {
    bool ioOk = false;
    quint64 readBytes = 0;
    quint64 writeBytes = 0;
    bool fdOk = false;
    QVector<quint64> devs;  // st_dev of every file / cwd the process has open
    QList<DrmClient> drm;
};

namespace procscan {

QByteArray slurp(const char *path, int maxBytes = 1 << 20);
QByteArray readText(const QString &path);
QList<int> listPids();
PidExtras scanPid(int pid);
QHash<QString, quint64> readRapl();

QJsonObject encode(const QHash<int, PidExtras> &procs, const QHash<QString, quint64> &rapl);
void decode(const QJsonObject &obj, QHash<int, PidExtras> *procs, QHash<QString, quint64> *rapl);

} // namespace procscan
