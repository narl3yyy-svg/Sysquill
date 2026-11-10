// SPDX-License-Identifier: GPL-3.0-or-later
// Collects everything from /proc and /sys on a worker thread.
#pragma once

#include "procscan.h"
#include "snapshot.h"

#include <QElapsedTimer>
#include <QHash>
#include <QObject>
#include <QPointer>
#include <QSet>

class QProcess;
class QTimer;

class Sampler : public QObject
{
    Q_OBJECT
public:
    explicit Sampler(QObject *parent = nullptr);

public Q_SLOTS:
    void start();
    void setIntervalMs(int ms);
    void setPaused(bool paused);
    void startHelper();
    void stopHelper();
    void requestKill(int pid, int sig);
    void sampleNow();

Q_SIGNALS:
    void snapshotReady(const Snapshot &snapshot);
    void helperStateChanged(const QString &state, const QString &message);
    void killResult(int pid, bool ok, const QString &message);

private:
    struct CpuTimes { quint64 idle = 0, total = 0; };
    struct ProcPrev { quint64 start = 0, ticks = 0, rb = 0, wb = 0; bool ioOk = false; };
    struct DiskPrev { quint64 rs = 0, ws = 0, ticks = 0; };
    struct DrmPrev { QHash<QString, quint64> ns; QHash<QString, QPair<quint64, quint64>> cycles; };
    struct GpuStatic { bool probed = false, present = false; QString name, driver, card, slot, sysCard; };

    void readCpu(Snapshot &s);
    void readMem(Snapshot &s);
    void readProcs(Snapshot &s, double dt);
    void readDisks(Snapshot &s, double dt);
    void readNet(Snapshot &s, double dt);
    void readSensors(Snapshot &s);
    void readGpu(Snapshot &s);
    void readPower(Snapshot &s, double dt);
    QString userName(uint uid);

    void setHelperState(const QString &state, const QString &message);
    QByteArray helperCommand(const QByteArray &cmd, int timeoutMs);
    void helperKill(int pid, int sig);

    QTimer *m_timer = nullptr;
    int m_intervalMs = 1000;
    bool m_paused = false;
    QElapsedTimer m_clock;
    qint64 m_lastNs = 0;
    long m_hz = 100;
    long m_pageSize = 4096;
    int m_ncpu = 1;

    QVector<CpuTimes> m_prevCpu;
    QHash<int, ProcPrev> m_prevProc;
    QHash<QString, DiskPrev> m_prevDisk;
    QHash<QString, QPair<quint64, quint64>> m_prevNet;
    QHash<QString, DrmPrev> m_prevDrm;
    QHash<QString, QHash<QString, double>> m_engineUtil;  // pdev -> engine -> % this tick
    QHash<QString, QSet<QString>> m_knownEngines;         // pdev -> engines seen
    QHash<QString, quint64> m_prevRapl;
    QHash<QString, quint64> m_helperRapl;
    QHash<uint, QString> m_users;
    GpuStatic m_gpu;

    QPointer<QProcess> m_helper;
    QString m_helperState = QStringLiteral("off");
    QList<QPair<int, int>> m_pendingKills;
};
