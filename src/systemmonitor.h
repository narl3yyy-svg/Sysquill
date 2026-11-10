// SPDX-License-Identifier: GPL-3.0-or-later
// The "Sys" singleton that QML binds to.
#pragma once

#include "processmodel.h"
#include "snapshot.h"

#include <QHash>
#include <QObject>
#include <QVariant>
#include <QVector>
#include <QtQml/qqmlregistration.h>

class QQmlEngine;
class QJSEngine;
class QThread;
class Sampler;

class SystemMonitor : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Sys)
    QML_SINGLETON

    // Static information
    Q_PROPERTY(QString hostname READ hostname CONSTANT)
    Q_PROPERTY(QString osName READ osName CONSTANT)
    Q_PROPERTY(QString kernel READ kernel CONSTANT)
    Q_PROPERTY(QString cpuModel READ cpuModel CONSTANT)
    Q_PROPERTY(int logicalCpus READ logicalCpus CONSTANT)
    Q_PROPERTY(int physicalCores READ physicalCores CONSTANT)
    Q_PROPERTY(double cpuMaxMhz READ cpuMaxMhz CONSTANT)
    Q_PROPERTY(QString version READ version CONSTANT)
    Q_PROPERTY(ProcessProxy *processes READ processes CONSTANT)

    // Live values
    Q_PROPERTY(double cpuUsage MEMBER m_cpuUsage NOTIFY updated)
    Q_PROPERTY(QVariantList cpuHistory MEMBER m_cpuHistory NOTIFY updated)
    Q_PROPERTY(QVariantList cores MEMBER m_cores NOTIFY updated)
    Q_PROPERTY(double cpuMhz MEMBER m_cpuMhz NOTIFY updated)
    Q_PROPERTY(double cpuTemp MEMBER m_cpuTemp NOTIFY updated)
    Q_PROPERTY(double cpuPower MEMBER m_cpuPower NOTIFY updated)
    Q_PROPERTY(QVariantList loadAvg MEMBER m_loadAvg NOTIFY updated)
    Q_PROPERTY(int procCount MEMBER m_procCount NOTIFY updated)
    Q_PROPERTY(int threadCount MEMBER m_threadCount NOTIFY updated)
    Q_PROPERTY(double uptime MEMBER m_uptime NOTIFY updated)
    Q_PROPERTY(QVariantMap memory MEMBER m_memory NOTIFY updated)
    Q_PROPERTY(QVariantList memHistory MEMBER m_memHistory NOTIFY updated)
    Q_PROPERTY(QVariantList swapHistory MEMBER m_swapHistory NOTIFY updated)
    Q_PROPERTY(QVariantList disks MEMBER m_disks NOTIFY updated)
    Q_PROPERTY(double diskRead MEMBER m_diskRead NOTIFY updated)
    Q_PROPERTY(double diskWrite MEMBER m_diskWrite NOTIFY updated)
    Q_PROPERTY(QVariantList diskReadHistory MEMBER m_diskReadHistory NOTIFY updated)
    Q_PROPERTY(QVariantList diskWriteHistory MEMBER m_diskWriteHistory NOTIFY updated)
    Q_PROPERTY(QVariantList nets MEMBER m_nets NOTIFY updated)
    Q_PROPERTY(double netRx MEMBER m_netRx NOTIFY updated)
    Q_PROPERTY(double netTx MEMBER m_netTx NOTIFY updated)
    Q_PROPERTY(QVariantList netRxHistory MEMBER m_netRxHistory NOTIFY updated)
    Q_PROPERTY(QVariantList netTxHistory MEMBER m_netTxHistory NOTIFY updated)
    Q_PROPERTY(QVariantList sensorGroups MEMBER m_sensorGroups NOTIFY updated)
    Q_PROPERTY(int sensorCount MEMBER m_sensorCount NOTIFY updated)
    Q_PROPERTY(QVariantMap gpu MEMBER m_gpu NOTIFY updated)
    Q_PROPERTY(QVariantList topProcesses MEMBER m_topProcesses NOTIFY updated)
    Q_PROPERTY(bool privileged MEMBER m_privileged NOTIFY updated)

    Q_PROPERTY(QString helperState READ helperState NOTIFY helperChanged)
    Q_PROPERTY(QString helperMessage READ helperMessage NOTIFY helperChanged)
    Q_PROPERTY(int interval READ interval WRITE setInterval NOTIFY intervalChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)

public:
    explicit SystemMonitor(QObject *parent = nullptr);
    ~SystemMonitor() override;

    static SystemMonitor *create(QQmlEngine *, QJSEngine *);

    QString hostname() const { return m_hostname; }
    QString osName() const { return m_osName; }
    QString kernel() const { return m_kernel; }
    QString cpuModel() const { return m_cpuModel; }
    int logicalCpus() const { return m_logicalCpus; }
    int physicalCores() const { return m_physicalCores; }
    double cpuMaxMhz() const { return m_cpuMaxMhz; }
    QString version() const;
    ProcessProxy *processes() const { return m_proxy; }
    QString helperState() const { return m_helperState; }
    QString helperMessage() const { return m_helperMessage; }
    int interval() const { return m_interval; }
    void setInterval(int ms);
    bool paused() const { return m_paused; }
    void setPaused(bool p);

    Q_INVOKABLE void unlock();
    Q_INVOKABLE void endProcess(int pid);
    Q_INVOKABLE void killProcess(int pid);
    Q_INVOKABLE QString formatBytes(double v) const;
    Q_INVOKABLE QString formatRate(double v) const;
    Q_INVOKABLE QString formatDuration(double secs) const;
    Q_INVOKABLE QString formatMhz(double mhz) const;
    Q_INVOKABLE QString debugDump() const;

Q_SIGNALS:
    void updated();
    void helperChanged();
    void intervalChanged();
    void pausedChanged();
    void killFinished(int pid, bool ok, const QString &message);

private:
    void onSnapshot(const Snapshot &s);
    void readStaticInfo();

    static SystemMonitor *s_instance;

    QThread *m_thread = nullptr;
    Sampler *m_sampler = nullptr;
    ProcessModel *m_model = nullptr;
    ProcessProxy *m_proxy = nullptr;

    QString m_hostname, m_osName, m_kernel, m_cpuModel;
    int m_logicalCpus = 1, m_physicalCores = 1;
    double m_cpuMaxMhz = -1;

    double m_cpuUsage = 0, m_cpuMhz = -1, m_cpuTemp = -1, m_cpuPower = -1, m_uptime = 0;
    QVariantList m_cpuHistory, m_cores, m_loadAvg;
    int m_procCount = 0, m_threadCount = 0;
    QVariantMap m_memory;
    QVariantList m_memHistory, m_swapHistory;
    QVariantList m_disks;
    double m_diskRead = 0, m_diskWrite = 0;
    QVariantList m_diskReadHistory, m_diskWriteHistory;
    QVariantList m_nets;
    double m_netRx = 0, m_netTx = 0;
    QVariantList m_netRxHistory, m_netTxHistory;
    QVariantList m_sensorGroups;
    int m_sensorCount = 0;
    QVariantMap m_gpu;
    QVariantList m_topProcesses;
    bool m_privileged = false;
    QString m_helperState = QStringLiteral("off");
    QString m_helperMessage;
    int m_interval = 1000;
    bool m_paused = false;

    // History buffers
    QVector<double> m_hCpu, m_hMem, m_hSwap, m_hDiskR, m_hDiskW, m_hNetRx, m_hNetTx, m_hGpu;
    QHash<int, QVector<double>> m_hCores;
    QHash<QString, QVector<double>> m_hDisk, m_hNet, m_hEngine;
};
