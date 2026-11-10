// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemmonitor.h"
#include "format.h"
#include "procscan.h"
#include "sampler.h"

#include <QJsonDocument>
#include <QQmlEngine>
#include <QRegularExpression>
#include <QSet>
#include <QSysInfo>
#include <QThread>

#include <algorithm>
#include <csignal>
#include <unistd.h>

SystemMonitor *SystemMonitor::s_instance = nullptr;

namespace {
constexpr int kHistory = 120;

void push(QVector<double> &h, double v)
{
    h.append(v);
    if (h.size() > kHistory)
        h.remove(0, h.size() - kHistory);
}

QVariantList toList(const QVector<double> &h)
{
    QVariantList l;
    l.reserve(h.size());
    for (double v : h)
        l.append(v);
    return l;
}

QString chipTitle(const QString &chip)
{
    if (chip == QLatin1String("coretemp") || chip == QLatin1String("k10temp") || chip == QLatin1String("zenpower"))
        return QStringLiteral("CPU");
    if (chip == QLatin1String("nvme"))
        return QStringLiteral("NVMe drive");
    if (chip == QLatin1String("drivetemp"))
        return QStringLiteral("Drive");
    if (chip == QLatin1String("acpitz"))
        return QStringLiteral("ACPI thermal zone");
    if (chip.startsWith(QLatin1String("pch_")))
        return QStringLiteral("Chipset");
    if (chip.startsWith(QLatin1String("iwlwifi")))
        return QStringLiteral("Wi-Fi");
    if (chip == QLatin1String("amdgpu") || chip == QLatin1String("i915") || chip == QLatin1String("xe")
        || chip == QLatin1String("nouveau"))
        return QStringLiteral("GPU");
    static const QRegularExpression superIo(QStringLiteral("^(it8\\d|nct6|f71|w83|asus|dell_smm|thinkpad)"));
    if (superIo.match(chip).hasMatch())
        return QStringLiteral("Motherboard");
    if (chip.startsWith(QLatin1String("r8169")) || chip.startsWith(QLatin1String("igc")))
        return QStringLiteral("Network adapter");
    return chip;
}

QString engineLabel(const QString &e)
{
    static const QHash<QString, QString> names = {
        {QStringLiteral("render"), QStringLiteral("Render / 3D")},
        {QStringLiteral("rcs"), QStringLiteral("Render / 3D")},
        {QStringLiteral("gfx"), QStringLiteral("Graphics")},
        {QStringLiteral("video"), QStringLiteral("Video (decode / encode)")},
        {QStringLiteral("vcs"), QStringLiteral("Video (decode / encode)")},
        {QStringLiteral("video-enhance"), QStringLiteral("Video enhance")},
        {QStringLiteral("vecs"), QStringLiteral("Video enhance")},
        {QStringLiteral("copy"), QStringLiteral("Copy / blitter")},
        {QStringLiteral("bcs"), QStringLiteral("Copy / blitter")},
        {QStringLiteral("compute"), QStringLiteral("Compute")},
        {QStringLiteral("ccs"), QStringLiteral("Compute")},
        {QStringLiteral("dec"), QStringLiteral("Video decode")},
        {QStringLiteral("enc"), QStringLiteral("Video encode")},
    };
    return names.value(e, e);
}

QVariant stripHistory(const QVariant &v)
{
    if (v.typeId() == QMetaType::QVariantMap) {
        QVariantMap m = v.toMap();
        for (auto it = m.begin(); it != m.end();) {
            if (it.key().contains(QLatin1String("istory")))
                it = m.erase(it);
            else {
                it.value() = stripHistory(it.value());
                ++it;
            }
        }
        return m;
    }
    if (v.typeId() == QMetaType::QVariantList) {
        QVariantList l = v.toList();
        for (QVariant &x : l)
            x = stripHistory(x);
        return l;
    }
    return v;
}
} // namespace

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent)
{
    s_instance = this;
    qRegisterMetaType<Snapshot>();
    readStaticInfo();

    m_model = new ProcessModel(this);
    m_proxy = new ProcessProxy(this);
    m_proxy->setSourceModel(m_model);

    m_thread = new QThread(this);
    m_thread->setObjectName(QStringLiteral("sysquill-sampler"));
    m_sampler = new Sampler;
    m_sampler->moveToThread(m_thread);
    connect(m_thread, &QThread::started, m_sampler, &Sampler::start);
    connect(m_thread, &QThread::finished, m_sampler, &QObject::deleteLater);
    connect(m_sampler, &Sampler::snapshotReady, this, &SystemMonitor::onSnapshot);
    connect(m_sampler, &Sampler::helperStateChanged, this, [this](const QString &st, const QString &msg) {
        m_helperState = st;
        m_helperMessage = msg;
        Q_EMIT helperChanged();
    });
    connect(m_sampler, &Sampler::killResult, this, &SystemMonitor::killFinished);
    m_thread->start();
}

SystemMonitor::~SystemMonitor()
{
    if (m_thread->isRunning()) {
        QMetaObject::invokeMethod(m_sampler, &Sampler::stopHelper, Qt::BlockingQueuedConnection);
        m_thread->quit();
        m_thread->wait();
    }
    if (s_instance == this)
        s_instance = nullptr;
}

SystemMonitor *SystemMonitor::create(QQmlEngine *, QJSEngine *)
{
    if (!s_instance)
        s_instance = new SystemMonitor;
    QJSEngine::setObjectOwnership(s_instance, QJSEngine::CppOwnership);
    return s_instance;
}

QString SystemMonitor::version() const
{
    return QStringLiteral(SYSQUILL_VERSION);
}

void SystemMonitor::readStaticInfo()
{
    m_hostname = QSysInfo::machineHostName();
    m_osName = QSysInfo::prettyProductName();
    m_kernel = QSysInfo::kernelVersion();
    m_logicalCpus = int(qMax(1L, sysconf(_SC_NPROCESSORS_ONLN)));

    QSet<QString> cores;
    QString physId;
    for (const QByteArray &line : procscan::slurp("/proc/cpuinfo").split('\n')) {
        const int c = line.indexOf(':');
        if (c < 0)
            continue;
        const QByteArray k = line.left(c).trimmed();
        const QString v = QString::fromUtf8(line.mid(c + 1).trimmed());
        if (k == "model name" && m_cpuModel.isEmpty())
            m_cpuModel = v;
        else if (k == "physical id")
            physId = v;
        else if (k == "core id")
            cores.insert(physId + QLatin1Char(':') + v);
    }
    m_cpuModel.remove(QStringLiteral("(R)")).remove(QStringLiteral("(TM)"));
    m_cpuModel = m_cpuModel.simplified();
    if (m_cpuModel.isEmpty())
        m_cpuModel = QSysInfo::currentCpuArchitecture();
    m_physicalCores = cores.isEmpty() ? m_logicalCpus : int(cores.size());

    const quint64 khz = procscan::readText(QStringLiteral("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq")).toULongLong();
    if (khz > 0)
        m_cpuMaxMhz = double(khz) / 1000.0;
}

void SystemMonitor::setInterval(int ms)
{
    ms = qBound(250, ms, 10000);
    if (ms == m_interval)
        return;
    m_interval = ms;
    QMetaObject::invokeMethod(m_sampler, [s = m_sampler, ms] { s->setIntervalMs(ms); }, Qt::QueuedConnection);
    Q_EMIT intervalChanged();
}

void SystemMonitor::setPaused(bool p)
{
    if (p == m_paused)
        return;
    m_paused = p;
    QMetaObject::invokeMethod(m_sampler, [s = m_sampler, p] { s->setPaused(p); }, Qt::QueuedConnection);
    Q_EMIT pausedChanged();
}

void SystemMonitor::unlock()
{
    QMetaObject::invokeMethod(m_sampler, &Sampler::startHelper, Qt::QueuedConnection);
}

void SystemMonitor::endProcess(int pid)
{
    QMetaObject::invokeMethod(m_sampler, [s = m_sampler, pid] { s->requestKill(pid, SIGTERM); }, Qt::QueuedConnection);
}

void SystemMonitor::killProcess(int pid)
{
    QMetaObject::invokeMethod(m_sampler, [s = m_sampler, pid] { s->requestKill(pid, SIGKILL); }, Qt::QueuedConnection);
}

QString SystemMonitor::formatBytes(double v) const { return fmt::bytes(v); }
QString SystemMonitor::formatRate(double v) const { return fmt::rate(v); }
QString SystemMonitor::formatDuration(double s) const { return fmt::duration(s); }
QString SystemMonitor::formatMhz(double m) const { return fmt::mhz(m); }

QString SystemMonitor::debugDump() const
{
    QVariantMap out;
    const QMetaObject *mo = metaObject();
    for (int i = mo->propertyOffset(); i < mo->propertyCount(); ++i) {
        const QMetaProperty p = mo->property(i);
        if (QByteArray(p.name()) == "processes" || QByteArray(p.name()).contains("istory"))
            continue;
        out.insert(QString::fromLatin1(p.name()), stripHistory(p.read(this)));
    }
    QVariantList procs;
    for (int r = 0; r < qMin(15, m_proxy->rowCount()); ++r) {
        QVariantMap m;
        for (int c = 0; c < ProcessModel::ColumnCount; ++c)
            m.insert(ProcessModel::columnTitle(c), m_proxy->index(r, c).data(Qt::DisplayRole));
        procs.append(m);
    }
    out.insert(QStringLiteral("processes(top 15 by CPU)"), procs);
    return QString::fromUtf8(QJsonDocument::fromVariant(out).toJson(QJsonDocument::Indented));
}

void SystemMonitor::onSnapshot(const Snapshot &s)
{
    // CPU
    m_cpuUsage = s.cpuUsage;
    push(m_hCpu, s.cpuUsage);
    m_cpuHistory = toList(m_hCpu);
    QVariantList cores;
    double mhzSum = 0;
    int mhzN = 0;
    for (int i = 0; i < s.cores.size(); ++i) {
        QVector<double> &h = m_hCores[i];
        push(h, s.cores[i].usage);
        cores.append(QVariantMap{{QStringLiteral("usage"), s.cores[i].usage},
                                 {QStringLiteral("mhz"), s.cores[i].mhz},
                                 {QStringLiteral("history"), toList(h)}});
        if (s.cores[i].mhz > 0) {
            mhzSum += s.cores[i].mhz;
            ++mhzN;
        }
    }
    m_cores = cores;
    m_cpuMhz = mhzN ? mhzSum / mhzN : -1;
    m_cpuTemp = s.cpuTemp;
    m_cpuPower = s.cpuPowerW;
    m_loadAvg = {s.load1, s.load5, s.load15};
    m_procCount = s.procCount;
    m_threadCount = s.threadCount;
    m_uptime = s.uptime;

    // Memory
    const double total = double(s.memTotal);
    const double used = s.memTotal > s.memAvailable ? double(s.memTotal - s.memAvailable) : 0.0;
    const double swapUsed = s.swapTotal > s.swapFree ? double(s.swapTotal - s.swapFree) : 0.0;
    m_memory = QVariantMap{
        {QStringLiteral("total"), total},
        {QStringLiteral("used"), used},
        {QStringLiteral("available"), double(s.memAvailable)},
        {QStringLiteral("free"), double(s.memFree)},
        {QStringLiteral("cached"), double(s.memCached)},
        {QStringLiteral("buffers"), double(s.memBuffers)},
        {QStringLiteral("shmem"), double(s.memShmem)},
        {QStringLiteral("dirty"), double(s.memDirty)},
        {QStringLiteral("swapTotal"), double(s.swapTotal)},
        {QStringLiteral("swapUsed"), swapUsed},
        {QStringLiteral("swapCached"), double(s.swapCached)},
    };
    push(m_hMem, total > 0 ? used / total * 100.0 : 0);
    push(m_hSwap, s.swapTotal > 0 ? swapUsed / double(s.swapTotal) * 100.0 : 0);
    m_memHistory = toList(m_hMem);
    m_swapHistory = toList(m_hSwap);

    QHash<int, const ProcSample *> byPid;
    for (const ProcSample &p : s.procs)
        byPid.insert(p.pid, &p);

    // Disks
    QVariantList disks;
    for (const DiskSample &d : s.disks) {
        QVector<double> &hr = m_hDisk[d.kname + QStringLiteral(":r")];
        QVector<double> &hw = m_hDisk[d.kname + QStringLiteral(":w")];
        push(hr, d.readRate);
        push(hw, d.writeRate);
        QList<const ProcSample *> users;
        for (int pid : d.pids)
            if (const ProcSample *p = byPid.value(pid))
                users.append(p);
        std::sort(users.begin(), users.end(), [](const ProcSample *a, const ProcSample *b) {
            const double ia = qMax(0.0, a->readRate) + qMax(0.0, a->writeRate);
            const double ib = qMax(0.0, b->readRate) + qMax(0.0, b->writeRate);
            if (ia != ib)
                return ia > ib;
            return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
        });
        QVariantList procs;
        for (const ProcSample *p : std::as_const(users))
            procs.append(QVariantMap{{QStringLiteral("pid"), p->pid},
                                     {QStringLiteral("name"), p->name},
                                     {QStringLiteral("user"), p->user},
                                     {QStringLiteral("read"), p->readRate},
                                     {QStringLiteral("write"), p->writeRate}});
        disks.append(QVariantMap{
            {QStringLiteral("device"), d.source},
            {QStringLiteral("kname"), d.kname},
            {QStringLiteral("parent"), d.parent},
            {QStringLiteral("label"), d.label},
            {QStringLiteral("uuid"), d.uuid},
            {QStringLiteral("fstype"), d.fstype},
            {QStringLiteral("model"), d.model},
            {QStringLiteral("mountPoints"), d.mountPoints},
            {QStringLiteral("mount"), d.mountPoints.value(0)},
            {QStringLiteral("total"), double(d.total)},
            {QStringLiteral("used"), double(d.used)},
            {QStringLiteral("avail"), double(d.avail)},
            {QStringLiteral("usedFraction"), d.total ? double(d.used) / double(d.used + d.avail) : 0.0},
            {QStringLiteral("readRate"), d.readRate},
            {QStringLiteral("writeRate"), d.writeRate},
            {QStringLiteral("busy"), d.busy},
            {QStringLiteral("rotational"), d.rotational},
            {QStringLiteral("removable"), d.removable},
            {QStringLiteral("readHistory"), toList(hr)},
            {QStringLiteral("writeHistory"), toList(hw)},
            {QStringLiteral("processes"), procs},
        });
    }
    m_disks = disks;
    m_diskRead = s.diskRead;
    m_diskWrite = s.diskWrite;
    push(m_hDiskR, s.diskRead);
    push(m_hDiskW, s.diskWrite);
    m_diskReadHistory = toList(m_hDiskR);
    m_diskWriteHistory = toList(m_hDiskW);

    // Network
    QVariantList nets;
    double rx = 0, tx = 0, rxAll = 0, txAll = 0;
    bool anyPhysical = false;
    for (const NetSample &n : s.nets) {
        QVector<double> &hr = m_hNet[n.name + QStringLiteral(":rx")];
        QVector<double> &ht = m_hNet[n.name + QStringLiteral(":tx")];
        push(hr, n.rxRate);
        push(ht, n.txRate);
        rxAll += n.rxRate;
        txAll += n.txRate;
        if (n.physical) {
            anyPhysical = true;
            rx += n.rxRate;
            tx += n.txRate;
        }
        nets.append(QVariantMap{
            {QStringLiteral("name"), n.name},
            {QStringLiteral("mac"), n.mac},
            {QStringLiteral("state"), n.state},
            {QStringLiteral("addresses"), n.addresses},
            {QStringLiteral("rxRate"), n.rxRate},
            {QStringLiteral("txRate"), n.txRate},
            {QStringLiteral("rxTotal"), double(n.rxTotal)},
            {QStringLiteral("txTotal"), double(n.txTotal)},
            {QStringLiteral("speed"), n.speedMbps},
            {QStringLiteral("physical"), n.physical},
            {QStringLiteral("wireless"), n.wireless},
            {QStringLiteral("rxHistory"), toList(hr)},
            {QStringLiteral("txHistory"), toList(ht)},
        });
    }
    m_nets = nets;
    m_netRx = anyPhysical ? rx : rxAll;
    m_netTx = anyPhysical ? tx : txAll;
    push(m_hNetRx, m_netRx);
    push(m_hNetTx, m_netTx);
    m_netRxHistory = toList(m_hNetRx);
    m_netTxHistory = toList(m_hNetTx);

    // Sensors, grouped by chip
    QVariantList groups;
    QString lastKey;
    QVariantMap group;
    QVariantList list;
    auto flush = [&] {
        if (!lastKey.isEmpty()) {
            group.insert(QStringLiteral("sensors"), list);
            groups.append(group);
        }
    };
    for (const SensorSample &t : s.sensors) {
        const QString key = t.chip + QLatin1Char('|') + t.device;
        if (key != lastKey) {
            flush();
            lastKey = key;
            list.clear();
            group = QVariantMap{{QStringLiteral("chip"), t.chip},
                                {QStringLiteral("title"), chipTitle(t.chip)},
                                {QStringLiteral("device"), t.device}};
        }
        list.append(QVariantMap{{QStringLiteral("label"), t.label},
                                {QStringLiteral("celsius"), t.celsius},
                                {QStringLiteral("crit"), t.crit}});
    }
    flush();
    m_sensorGroups = groups;
    m_sensorCount = int(s.sensors.size());

    // GPU
    {
        const GpuSample &g = s.gpu;
        push(m_hGpu, g.busy);
        QVariantList engines;
        for (const EngineSample &e : g.engines) {
            QVector<double> &h = m_hEngine[e.name];
            push(h, e.util);
            engines.append(QVariantMap{{QStringLiteral("name"), e.name},
                                       {QStringLiteral("label"), engineLabel(e.name)},
                                       {QStringLiteral("util"), e.util},
                                       {QStringLiteral("history"), toList(h)}});
        }
        QList<const ProcSample *> gp;
        for (const ProcSample &p : s.procs)
            if (p.usesGpu)
                gp.append(&p);
        std::sort(gp.begin(), gp.end(), [](const ProcSample *a, const ProcSample *b) {
            if (a->gpu != b->gpu)
                return a->gpu > b->gpu;
            return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
        });
        QVariantList gprocs;
        for (const ProcSample *p : std::as_const(gp))
            gprocs.append(QVariantMap{{QStringLiteral("pid"), p->pid},
                                      {QStringLiteral("name"), p->name},
                                      {QStringLiteral("user"), p->user},
                                      {QStringLiteral("gpu"), p->gpu}});
        m_gpu = QVariantMap{
            {QStringLiteral("present"), g.present},
            {QStringLiteral("name"), g.name},
            {QStringLiteral("driver"), g.driver},
            {QStringLiteral("card"), g.card},
            {QStringLiteral("slot"), g.slot},
            {QStringLiteral("curMhz"), g.curMhz},
            {QStringLiteral("maxMhz"), g.maxMhz},
            {QStringLiteral("busy"), g.busy},
            {QStringLiteral("powerW"), g.powerW},
            {QStringLiteral("engines"), engines},
            {QStringLiteral("processes"), gprocs},
            {QStringLiteral("history"), toList(m_hGpu)},
        };
    }

    // Top processes for the overview
    {
        QList<const ProcSample *> all;
        for (const ProcSample &p : s.procs)
            all.append(&p);
        std::partial_sort(all.begin(), all.begin() + qMin<qsizetype>(6, all.size()), all.end(),
                          [](const ProcSample *a, const ProcSample *b) {
                              if (a->cpu != b->cpu)
                                  return a->cpu > b->cpu;
                              return a->rss > b->rss;
                          });
        QVariantList top;
        for (int i = 0; i < qMin<qsizetype>(6, all.size()); ++i)
            top.append(QVariantMap{{QStringLiteral("pid"), all[i]->pid},
                                   {QStringLiteral("name"), all[i]->name},
                                   {QStringLiteral("cpu"), all[i]->cpu},
                                   {QStringLiteral("rss"), double(all[i]->rss)}});
        m_topProcesses = top;
    }

    m_privileged = s.privileged;
    m_model->update(s.procs);
    Q_EMIT updated();
}
