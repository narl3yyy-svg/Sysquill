// SPDX-License-Identifier: GPL-3.0-or-later
// Plain data collected by the Sampler thread on each tick.
#pragma once

#include <QList>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

struct CoreSample {
    double usage = 0;
    double mhz = -1;
};

struct ProcSample {
    int pid = 0;
    int ppid = 0;
    QString name;
    QString user;
    QString cmdline;
    char state = '?';
    double cpu = 0;          // percent of the whole machine
    quint64 rss = 0;         // bytes
    double readRate = -1;    // bytes/s, -1 = not visible
    double writeRate = -1;
    double gpu = -1;         // percent, -1 = no GPU use
    bool usesGpu = false;
    int threads = 0;
    int nice = 0;
    bool kernel = false;
    quint64 startTicks = 0;
    QVector<quint64> devs;   // filesystems the process has files open on
};

struct DiskSample {
    QString source;          // /dev/sda1
    QString kname;           // sda1 / dm-0 / nvme0n1p2
    QString parent;          // sda / nvme0n1
    QString label;
    QString uuid;
    QString fstype;
    QString model;
    QStringList mountPoints;
    quint64 devId = 0;
    quint64 total = 0;
    quint64 used = 0;
    quint64 avail = 0;
    double readRate = 0;
    double writeRate = 0;
    double busy = 0;
    bool rotational = false;
    bool removable = false;
    QVector<int> pids;
};

struct NetSample {
    QString name;
    QString mac;
    QString state;
    QStringList addresses;
    double rxRate = 0;
    double txRate = 0;
    quint64 rxTotal = 0;
    quint64 txTotal = 0;
    int speedMbps = -1;
    bool physical = false;
    bool wireless = false;
};

struct SensorSample {
    QString chip;
    QString device;
    QString label;
    double celsius = 0;
    double crit = 0;
};

struct EngineSample {
    QString name;
    double util = 0;
};

struct GpuSample {
    bool present = false;
    QString name;
    QString driver;
    QString card;
    QString slot;
    double curMhz = -1;
    double maxMhz = -1;
    double busy = 0;
    QList<EngineSample> engines;
    double powerW = -1;
};

struct Snapshot {
    double dt = 0;
    double cpuUsage = 0;
    QList<CoreSample> cores;
    double cpuTemp = -1;
    double cpuPowerW = -1;
    double load1 = 0, load5 = 0, load15 = 0;
    int procCount = 0;
    int threadCount = 0;
    double uptime = 0;

    quint64 memTotal = 0, memAvailable = 0, memFree = 0, memCached = 0, memBuffers = 0;
    quint64 memShmem = 0, memDirty = 0, swapTotal = 0, swapFree = 0, swapCached = 0;

    QList<ProcSample> procs;
    QList<DiskSample> disks;
    double diskRead = 0;
    double diskWrite = 0;
    QList<NetSample> nets;
    QList<SensorSample> sensors;
    GpuSample gpu;
    bool privileged = false;
};

Q_DECLARE_METATYPE(Snapshot)
