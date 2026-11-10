// SPDX-License-Identifier: GPL-3.0-or-later
#include "sampler.h"

#include <QCollator>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <csignal>
#include <cstring>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pwd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <unistd.h>

using procscan::readText;
using procscan::slurp;

namespace {

quint64 readU64(const QString &path, bool *ok = nullptr)
{
    const QByteArray b = readText(path);
    bool k = false;
    const quint64 v = b.toULongLong(&k);
    if (ok)
        *ok = k && !b.isEmpty();
    return v;
}

QString canonical(const QString &p)
{
    const QString c = QFileInfo(p).canonicalFilePath();
    return c.isEmpty() ? p : c;
}

// mountinfo escapes space, tab, newline and backslash as \ooo
QString unescapeMount(const QByteArray &in)
{
    QByteArray out;
    out.reserve(in.size());
    for (int i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 3 < in.size() + 0 && in[i + 1] >= '0' && in[i + 1] <= '7') {
            out.append(char(in.mid(i + 1, 3).toInt(nullptr, 8)));
            i += 3;
        } else {
            out.append(in[i]);
        }
    }
    return QString::fromUtf8(out);
}

// udev escapes characters in /dev/disk/by-label names as \xHH
QString unescapeUdev(const QString &s)
{
    QByteArray out;
    const QByteArray in = s.toUtf8();
    for (int i = 0; i < in.size(); ++i) {
        if (in[i] == '\\' && i + 3 < in.size() + 0 && in[i + 1] == 'x') {
            bool ok = false;
            const int c = in.mid(i + 2, 2).toInt(&ok, 16);
            if (ok) {
                out.append(char(c));
                i += 3;
                continue;
            }
        }
        out.append(in[i]);
    }
    return QString::fromUtf8(out);
}

QHash<QString, QString> linkMap(const QString &dirPath, bool unescape)
{
    QHash<QString, QString> m;
    QDir dir(dirPath);
    const QFileInfoList list = dir.entryInfoList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
    for (const QFileInfo &fi : list) {
        const QString target = fi.canonicalFilePath();
        if (!target.isEmpty())
            m.insert(target, unescape ? unescapeUdev(fi.fileName()) : fi.fileName());
    }
    return m;
}

} // namespace

Sampler::Sampler(QObject *parent)
    : QObject(parent)
{
    m_hz = sysconf(_SC_CLK_TCK);
    m_pageSize = sysconf(_SC_PAGESIZE);
    m_ncpu = int(qMax(1L, sysconf(_SC_NPROCESSORS_ONLN)));
}

void Sampler::start()
{
    m_clock.start();
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &Sampler::sampleNow);
    m_timer->start(m_intervalMs);
    sampleNow();
}

void Sampler::setIntervalMs(int ms)
{
    m_intervalMs = qBound(250, ms, 10000);
    if (m_timer)
        m_timer->setInterval(m_intervalMs);
}

void Sampler::setPaused(bool paused)
{
    m_paused = paused;
    if (!m_timer)
        return;
    if (paused) {
        m_timer->stop();
    } else {
        m_lastNs = 0;  // don't report a huge interval after resuming
        m_prevCpu.clear();
        m_timer->start(m_intervalMs);
        sampleNow();
    }
}

void Sampler::sampleNow()
{
    const qint64 now = m_clock.nsecsElapsed();
    const double dt = m_lastNs ? double(now - m_lastNs) / 1e9 : 0.0;
    m_lastNs = now;

    Snapshot s;
    s.dt = dt;
    readCpu(s);
    readMem(s);
    readProcs(s, dt);
    readDisks(s, dt);
    readNet(s, dt);
    readSensors(s);
    readGpu(s);
    readPower(s, dt);
    Q_EMIT snapshotReady(s);
}

// ---------------------------------------------------------------------------
// CPU

void Sampler::readCpu(Snapshot &s)
{
    const QByteArray stat = slurp("/proc/stat");
    QVector<CpuTimes> now;
    for (const QByteArray &line : stat.split('\n')) {
        if (!line.startsWith("cpu"))
            continue;
        const QList<QByteArray> f = line.simplified().split(' ');
        if (f.size() < 9)
            continue;
        CpuTimes t;
        for (int i = 1; i <= 8; ++i)
            t.total += f[i].toULongLong();
        t.idle = f[4].toULongLong() + f[5].toULongLong();
        now.append(t);
    }
    auto usage = [&](int i) -> double {
        if (i >= m_prevCpu.size() || i >= now.size())
            return 0.0;
        const CpuTimes &a = m_prevCpu[i], &b = now[i];
        if (b.total <= a.total)
            return 0.0;
        const double dTotal = double(b.total - a.total);
        const double dIdle = b.idle >= a.idle ? double(b.idle - a.idle) : 0.0;
        return qBound(0.0, 100.0 * (1.0 - dIdle / dTotal), 100.0);
    };
    if (!now.isEmpty())
        s.cpuUsage = usage(0);

    QList<double> infoMhz;
    bool infoLoaded = false;
    for (int i = 1; i < now.size(); ++i) {
        CoreSample c;
        c.usage = usage(i);
        bool ok = false;
        const quint64 khz = readU64(QStringLiteral("/sys/devices/system/cpu/cpu%1/cpufreq/scaling_cur_freq").arg(i - 1), &ok);
        if (ok && khz > 0) {
            c.mhz = double(khz) / 1000.0;
        } else {
            if (!infoLoaded) {
                infoLoaded = true;
                for (const QByteArray &l : slurp("/proc/cpuinfo").split('\n'))
                    if (l.startsWith("cpu MHz"))
                        infoMhz.append(l.mid(l.indexOf(':') + 1).trimmed().toDouble());
            }
            c.mhz = infoMhz.value(i - 1, -1);
        }
        s.cores.append(c);
    }
    m_prevCpu = now;

    s.uptime = slurp("/proc/uptime").split(' ').value(0).toDouble();
    const QList<QByteArray> la = slurp("/proc/loadavg").split(' ');
    s.load1 = la.value(0).toDouble();
    s.load5 = la.value(1).toDouble();
    s.load15 = la.value(2).toDouble();
}

// ---------------------------------------------------------------------------
// Memory

void Sampler::readMem(Snapshot &s)
{
    QHash<QByteArray, quint64> m;
    for (const QByteArray &line : slurp("/proc/meminfo").split('\n')) {
        const int c = line.indexOf(':');
        if (c <= 0)
            continue;
        m.insert(line.left(c), line.mid(c + 1).simplified().split(' ').value(0).toULongLong() * 1024ULL);
    }
    s.memTotal = m.value("MemTotal");
    s.memFree = m.value("MemFree");
    s.memAvailable = m.contains("MemAvailable") ? m.value("MemAvailable") : s.memFree;
    s.memCached = m.value("Cached") + m.value("SReclaimable");
    s.memBuffers = m.value("Buffers");
    s.memShmem = m.value("Shmem");
    s.memDirty = m.value("Dirty");
    s.swapTotal = m.value("SwapTotal");
    s.swapFree = m.value("SwapFree");
    s.swapCached = m.value("SwapCached");
}

// ---------------------------------------------------------------------------
// Processes

QString Sampler::userName(uint uid)
{
    auto it = m_users.constFind(uid);
    if (it != m_users.constEnd())
        return it.value();
    QString name = QString::number(uid);
    struct passwd pw;
    struct passwd *res = nullptr;
    char buf[4096];
    if (getpwuid_r(uid, &pw, buf, sizeof buf, &res) == 0 && res)
        name = QString::fromLocal8Bit(res->pw_name);
    m_users.insert(uid, name);
    return name;
}

void Sampler::readProcs(Snapshot &s, double dt)
{
    QHash<int, PidExtras> helperData;
    bool priv = false;
    if (m_helperState == QLatin1String("on") && m_helper) {
        const QByteArray line = helperCommand("scan", 5000);
        if (!line.isEmpty()) {
            const QJsonDocument doc = QJsonDocument::fromJson(line);
            if (doc.isObject()) {
                procscan::decode(doc.object(), &helperData, &m_helperRapl);
                priv = true;
            }
        }
    }
    s.privileged = priv;

    m_engineUtil.clear();
    QHash<int, ProcPrev> nextPrev;
    QHash<QString, DrmPrev> nextDrm;
    QSet<QString> seenClients;
    const double ticksAvail = dt * double(m_hz) * double(m_ncpu);

    const QList<int> pids = procscan::listPids();
    s.procs.reserve(pids.size());
    char path[64];
    for (int pid : pids) {
        snprintf(path, sizeof path, "/proc/%d/stat", pid);
        const QByteArray st = slurp(path, 4096);
        const int lp = st.indexOf('(');
        const int rp = st.lastIndexOf(')');
        if (lp < 0 || rp < lp)
            continue;
        const QList<QByteArray> f = st.mid(rp + 2).split(' ');
        if (f.size() < 22)
            continue;

        ProcSample p;
        p.pid = pid;
        const QString comm = QString::fromUtf8(st.mid(lp + 1, rp - lp - 1));
        p.state = f[0].isEmpty() ? '?' : f[0].at(0);
        p.ppid = f[1].toInt();
        p.kernel = (f[6].toULongLong() & 0x00200000ULL) != 0;  // PF_KTHREAD
        const quint64 ticks = f[11].toULongLong() + f[12].toULongLong();
        p.nice = f[16].toInt();
        p.threads = f[17].toInt();
        p.startTicks = f[19].toULongLong();
        p.rss = f[21].toULongLong() * quint64(m_pageSize);
        s.threadCount += p.threads;

        struct stat sb;
        snprintf(path, sizeof path, "/proc/%d", pid);
        if (::stat(path, &sb) == 0)
            p.user = userName(sb.st_uid);

        p.name = comm;
        if (!p.kernel) {
            snprintf(path, sizeof path, "/proc/%d/cmdline", pid);
            QByteArray cl = slurp(path, 32768);
            const int z = cl.indexOf('\0');
            const QByteArray argv0 = z >= 0 ? cl.left(z) : cl;
            while (cl.endsWith('\0'))
                cl.chop(1);
            cl.replace('\0', ' ');
            p.cmdline = QString::fromUtf8(cl);
            QString base = QString::fromUtf8(argv0);
            base = base.mid(base.lastIndexOf(QLatin1Char('/')) + 1);
            // comm is cut to 15 characters; use the full program name when it matches
            if (comm.size() >= 15 && base.startsWith(comm))
                p.name = base;
        }

        PidExtras ex;
        if (!p.kernel) {
            auto it = helperData.constFind(pid);
            ex = it != helperData.constEnd() ? it.value() : procscan::scanPid(pid);
        }

        ProcPrev np;
        np.start = p.startTicks;
        np.ticks = ticks;
        np.ioOk = ex.ioOk;
        np.rb = ex.readBytes;
        np.wb = ex.writeBytes;
        auto prev = m_prevProc.constFind(pid);
        const bool same = prev != m_prevProc.constEnd() && prev->start == p.startTicks;
        if (same && ticksAvail > 0 && ticks >= prev->ticks)
            p.cpu = qMin(100.0, 100.0 * double(ticks - prev->ticks) / ticksAvail);
        if (ex.ioOk) {
            if (same && prev->ioOk && dt > 0) {
                p.readRate = ex.readBytes >= prev->rb ? double(ex.readBytes - prev->rb) / dt : 0.0;
                p.writeRate = ex.writeBytes >= prev->wb ? double(ex.writeBytes - prev->wb) / dt : 0.0;
            } else {
                p.readRate = 0;
                p.writeRate = 0;
            }
        }
        nextPrev.insert(pid, np);
        p.devs = ex.devs;

        if (!ex.drm.isEmpty()) {
            p.usesGpu = true;
            QHash<QString, double> pidEngines;
            for (const DrmClient &c : std::as_const(ex.drm)) {
                if (seenClients.contains(c.key))
                    continue;
                seenClients.insert(c.key);
                DrmPrev dp;
                dp.ns = c.engineNs;
                dp.cycles = c.cycles;
                auto pc = m_prevDrm.constFind(c.key);
                const bool havePrev = pc != m_prevDrm.constEnd() && dt > 0;
                QSet<QString> &known = m_knownEngines[c.pdev];
                QHash<QString, double> &agg = m_engineUtil[c.pdev];
                for (auto it = c.engineNs.cbegin(); it != c.engineNs.cend(); ++it) {
                    known.insert(it.key());
                    double u = 0;
                    if (havePrev) {
                        const quint64 old = pc->ns.value(it.key(), it.value());
                        if (it.value() >= old)
                            u = double(it.value() - old) / (dt * 1e9) * 100.0
                                / double(qMax(1, c.capacity.value(it.key(), 1)));
                    }
                    agg[it.key()] += u;
                    pidEngines[it.key()] += u;
                }
                for (auto it = c.cycles.cbegin(); it != c.cycles.cend(); ++it) {
                    known.insert(it.key());
                    double u = 0;
                    if (havePrev && pc->cycles.contains(it.key())) {
                        const auto old = pc->cycles.value(it.key());
                        const auto cur = it.value();
                        if (cur.second > old.second && cur.first >= old.first)
                            u = 100.0 * double(cur.first - old.first) / double(cur.second - old.second)
                                / double(qMax(1, c.capacity.value(it.key(), 1)));
                    }
                    agg[it.key()] += u;
                    pidEngines[it.key()] += u;
                }
                nextDrm.insert(c.key, dp);
            }
            double g = 0;
            for (double v : std::as_const(pidEngines))
                g = qMax(g, v);
            p.gpu = qMin(100.0, g);
        }
        s.procs.append(p);
    }
    s.procCount = int(s.procs.size());
    m_prevProc.swap(nextPrev);
    m_prevDrm.swap(nextDrm);
}

// ---------------------------------------------------------------------------
// Disks

void Sampler::readDisks(Snapshot &s, double dt)
{
    QHash<QString, DiskPrev> stats;
    for (const QByteArray &line : slurp("/proc/diskstats").split('\n')) {
        const QList<QByteArray> f = line.simplified().split(' ');
        if (f.size() < 13)
            continue;
        DiskPrev d;
        d.rs = f[5].toULongLong();
        d.ws = f[9].toULongLong();
        d.ticks = f[12].toULongLong();
        stats.insert(QString::fromLatin1(f[2]), d);
    }
    auto rates = [&](const QString &k, double *r, double *w, double *busy) {
        *r = *w = *busy = 0;
        auto c = stats.constFind(k);
        auto p = m_prevDisk.constFind(k);
        if (c == stats.constEnd() || p == m_prevDisk.constEnd() || dt <= 0)
            return;
        *r = c->rs >= p->rs ? double(c->rs - p->rs) * 512.0 / dt : 0;
        *w = c->ws >= p->ws ? double(c->ws - p->ws) * 512.0 / dt : 0;
        *busy = c->ticks >= p->ticks ? qMin(100.0, double(c->ticks - p->ticks) / (dt * 10.0)) : 0;
    };

    // Whole physical drives, for the "all disks" totals
    for (auto it = stats.cbegin(); it != stats.cend(); ++it) {
        const QString &k = it.key();
        if (!QFile::exists(QStringLiteral("/sys/block/%1/device").arg(k)))
            continue;
        double r, w, b;
        rates(k, &r, &w, &b);
        s.diskRead += r;
        s.diskWrite += w;
    }

    const QHash<QString, QString> labels = linkMap(QStringLiteral("/dev/disk/by-label"), true);
    const QHash<QString, QString> uuids = linkMap(QStringLiteral("/dev/disk/by-uuid"), false);

    QHash<QString, int> byDev;
    for (const QByteArray &line : slurp("/proc/self/mountinfo", 4 << 20).split('\n')) {
        const int sep = line.indexOf(" - ");
        if (sep < 0)
            continue;
        const QList<QByteArray> a = line.left(sep).split(' ');
        const QList<QByteArray> b = line.mid(sep + 3).split(' ');
        if (a.size() < 5 || b.size() < 2)
            continue;
        const QString fstype = QString::fromUtf8(b[0]);
        const QString source = unescapeMount(b[1]);
        if (!source.startsWith(QLatin1String("/dev/")) || source.startsWith(QLatin1String("/dev/loop"))
            || fstype == QLatin1String("squashfs"))
            continue;
        const QString mp = unescapeMount(a[4]);
        const QByteArray mm = a[2];
        auto existing = byDev.constFind(QString::fromLatin1(mm));
        if (existing != byDev.constEnd()) {
            s.disks[existing.value()].mountPoints.append(mp);
            continue;
        }
        DiskSample d;
        d.source = source;
        d.fstype = fstype;
        d.mountPoints.append(mp);
        const QList<QByteArray> mmParts = mm.split(':');
        d.devId = quint64(makedev(mmParts.value(0).toUInt(), mmParts.value(1).toUInt()));

        const QString real = canonical(source);
        d.kname = real.mid(real.lastIndexOf(QLatin1Char('/')) + 1);
        d.label = labels.value(real);
        d.uuid = uuids.value(real);

        const QString sysPath = canonical(QStringLiteral("/sys/class/block/") + d.kname);
        QString parent = d.kname;
        if (QFile::exists(sysPath + QStringLiteral("/partition"))) {
            parent = QFileInfo(sysPath).dir().dirName();
        } else if (d.kname.startsWith(QLatin1String("dm-")) || d.kname.startsWith(QLatin1String("md"))) {
            const QStringList slaves = QDir(sysPath + QStringLiteral("/slaves"))
                                           .entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
            if (!slaves.isEmpty()) {
                const QString sp = canonical(QStringLiteral("/sys/class/block/") + slaves.first());
                parent = QFile::exists(sp + QStringLiteral("/partition")) ? QFileInfo(sp).dir().dirName()
                                                                          : slaves.first();
            }
        }
        d.parent = parent;
        const QString parentSys = QStringLiteral("/sys/class/block/") + parent;
        d.model = QString::fromUtf8(readText(parentSys + QStringLiteral("/device/model"))).simplified();
        d.rotational = readText(parentSys + QStringLiteral("/queue/rotational")) == "1";
        d.removable = readText(parentSys + QStringLiteral("/removable")) == "1";
        rates(d.kname, &d.readRate, &d.writeRate, &d.busy);

        byDev.insert(QString::fromLatin1(mm), int(s.disks.size()));
        s.disks.append(d);
    }

    QHash<quint64, int> devIndex;
    for (int i = 0; i < s.disks.size(); ++i) {
        DiskSample &d = s.disks[i];
        std::sort(d.mountPoints.begin(), d.mountPoints.end(),
                  [](const QString &x, const QString &y) { return x.size() < y.size(); });
        struct statvfs v;
        if (::statvfs(QFile::encodeName(d.mountPoints.first()).constData(), &v) == 0) {
            const quint64 fr = v.f_frsize ? v.f_frsize : v.f_bsize;
            d.total = quint64(v.f_blocks) * fr;
            d.avail = quint64(v.f_bavail) * fr;
            d.used = quint64(v.f_blocks - v.f_bfree) * fr;
        }
        devIndex.insert(d.devId, i);
    }
    for (const ProcSample &p : std::as_const(s.procs)) {
        for (quint64 dv : p.devs) {
            auto it = devIndex.constFind(dv);
            if (it != devIndex.constEnd())
                s.disks[it.value()].pids.append(p.pid);
        }
    }
    std::sort(s.disks.begin(), s.disks.end(), [](const DiskSample &x, const DiskSample &y) {
        return x.mountPoints.first() < y.mountPoints.first();
    });
    m_prevDisk = stats;
}

// ---------------------------------------------------------------------------
// Network

void Sampler::readNet(Snapshot &s, double dt)
{
    QHash<QString, QStringList> addrs;
    struct ifaddrs *ifa = nullptr;
    if (getifaddrs(&ifa) == 0) {
        for (struct ifaddrs *i = ifa; i; i = i->ifa_next) {
            if (!i->ifa_addr)
                continue;
            char buf[INET6_ADDRSTRLEN] = {0};
            const int fam = i->ifa_addr->sa_family;
            if (fam == AF_INET) {
                inet_ntop(AF_INET, &reinterpret_cast<sockaddr_in *>(i->ifa_addr)->sin_addr, buf, sizeof buf);
                int prefix = 0;
                if (i->ifa_netmask)
                    prefix = __builtin_popcount(reinterpret_cast<sockaddr_in *>(i->ifa_netmask)->sin_addr.s_addr);
                addrs[QString::fromLatin1(i->ifa_name)].prepend(QStringLiteral("%1/%2").arg(QLatin1String(buf)).arg(prefix));
            } else if (fam == AF_INET6) {
                inet_ntop(AF_INET6, &reinterpret_cast<sockaddr_in6 *>(i->ifa_addr)->sin6_addr, buf, sizeof buf);
                addrs[QString::fromLatin1(i->ifa_name)].append(QString::fromLatin1(buf));
            }
        }
        freeifaddrs(ifa);
    }

    QHash<QString, QPair<quint64, quint64>> now;
    const QList<QByteArray> lines = slurp("/proc/net/dev").split('\n');
    for (int li = 2; li < lines.size(); ++li) {
        const QByteArray &line = lines[li];
        const int c = line.indexOf(':');
        if (c <= 0)
            continue;
        const QString name = QString::fromLatin1(line.left(c).trimmed());
        const QList<QByteArray> f = line.mid(c + 1).simplified().split(' ');
        if (f.size() < 16 || name == QLatin1String("lo"))
            continue;
        NetSample n;
        n.name = name;
        n.rxTotal = f[0].toULongLong();
        n.txTotal = f[8].toULongLong();
        now.insert(name, {n.rxTotal, n.txTotal});
        const QString base = QStringLiteral("/sys/class/net/") + name;
        n.physical = QFile::exists(base + QStringLiteral("/device"));
        n.wireless = QFile::exists(base + QStringLiteral("/wireless")) || QFile::exists(base + QStringLiteral("/phy80211"));
        n.mac = QString::fromLatin1(readText(base + QStringLiteral("/address")));
        n.state = QString::fromLatin1(readText(base + QStringLiteral("/operstate")));
        bool ok = false;
        const int sp = readText(base + QStringLiteral("/speed")).toInt(&ok);
        n.speedMbps = ok && sp > 0 ? sp : -1;
        n.addresses = addrs.value(name);
        auto p = m_prevNet.constFind(name);
        if (p != m_prevNet.constEnd() && dt > 0) {
            n.rxRate = n.rxTotal >= p->first ? double(n.rxTotal - p->first) / dt : 0;
            n.txRate = n.txTotal >= p->second ? double(n.txTotal - p->second) / dt : 0;
        }
        s.nets.append(n);
    }
    std::sort(s.nets.begin(), s.nets.end(), [](const NetSample &a, const NetSample &b) {
        if (a.physical != b.physical)
            return a.physical;
        return a.name < b.name;
    });
    m_prevNet = now;
}

// ---------------------------------------------------------------------------
// Temperatures

void Sampler::readSensors(Snapshot &s)
{
    QDir hw(QStringLiteral("/sys/class/hwmon"));
    const QStringList mons = hw.entryList(QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &h : mons) {
        const QString base = hw.filePath(h);
        const QString chip = QString::fromUtf8(readText(base + QStringLiteral("/name")));
        const QString device = QString::fromUtf8(readText(base + QStringLiteral("/device/model"))).simplified();
        const QStringList inputs = QDir(base).entryList({QStringLiteral("temp*_input")}, QDir::Files | QDir::System);
        for (const QString &f : inputs) {
            const QString idx = f.mid(4, f.indexOf(QLatin1Char('_')) - 4);
            const QByteArray raw = readText(base + QLatin1Char('/') + f);
            bool ok = false;
            const qint64 milli = raw.toLongLong(&ok);
            if (!ok)
                continue;
            SensorSample t;
            t.chip = chip;
            t.device = device;
            t.label = QString::fromUtf8(readText(base + QStringLiteral("/temp%1_label").arg(idx)));
            if (t.label.isEmpty())
                t.label = QStringLiteral("Sensor %1").arg(idx);
            t.celsius = double(milli) / 1000.0;
            t.crit = double(readText(base + QStringLiteral("/temp%1_crit").arg(idx)).toLongLong()) / 1000.0;
            s.sensors.append(t);
        }
    }
    QCollator col;
    col.setNumericMode(true);
    std::sort(s.sensors.begin(), s.sensors.end(), [&col](const SensorSample &a, const SensorSample &b) {
        if (a.chip != b.chip)
            return a.chip < b.chip;
        if (a.device != b.device)
            return a.device < b.device;
        return col.compare(a.label, b.label) < 0;
    });

    // Pick the CPU package temperature
    for (const SensorSample &t : std::as_const(s.sensors)) {
        if (t.chip == QLatin1String("coretemp") && t.label.startsWith(QLatin1String("Package"))) {
            s.cpuTemp = t.celsius;
            break;
        }
        if ((t.chip == QLatin1String("k10temp") || t.chip == QLatin1String("zenpower"))
            && (t.label == QLatin1String("Tctl") || t.label == QLatin1String("Tdie"))) {
            s.cpuTemp = t.celsius;
            break;
        }
    }
    if (s.cpuTemp < 0) {
        for (const SensorSample &t : std::as_const(s.sensors)) {
            if (t.chip == QLatin1String("coretemp") || t.chip == QLatin1String("k10temp")) {
                s.cpuTemp = t.celsius;
                break;
            }
        }
    }
    if (s.cpuTemp < 0) {
        QDir tz(QStringLiteral("/sys/class/thermal"));
        for (const QString &z : tz.entryList({QStringLiteral("thermal_zone*")}, QDir::AllEntries | QDir::System)) {
            if (readText(tz.filePath(z) + QStringLiteral("/type")) == "x86_pkg_temp") {
                s.cpuTemp = double(readText(tz.filePath(z) + QStringLiteral("/temp")).toLongLong()) / 1000.0;
                break;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// GPU

void Sampler::readGpu(Snapshot &s)
{
    if (!m_gpu.probed) {
        m_gpu.probed = true;
        QDir drm(QStringLiteral("/sys/class/drm"));
        static const QRegularExpression cardRe(QStringLiteral("^card\\d+$"));
        QStringList cards = drm.entryList({QStringLiteral("card*")}, QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
        cards.erase(std::remove_if(cards.begin(), cards.end(), [](const QString &c) { return !cardRe.match(c).hasMatch(); }),
                    cards.end());
        QCollator col;
        col.setNumericMode(true);
        std::sort(cards.begin(), cards.end(), [&col](const QString &a, const QString &b) { return col.compare(a, b) < 0; });
        for (const QString &card : std::as_const(cards)) {
            const QString dev = canonical(drm.filePath(card) + QStringLiteral("/device"));
            const QString driver = QFileInfo(canonical(dev + QStringLiteral("/driver"))).fileName();
            if (driver.isEmpty() || driver.startsWith(QLatin1String("simple")) || driver == QLatin1String("vkms"))
                continue;
            m_gpu.present = true;
            m_gpu.card = card;
            m_gpu.sysCard = drm.filePath(card);
            m_gpu.driver = driver;
            m_gpu.slot = QFileInfo(dev).fileName();

            QProcess lspci;
            lspci.start(QStringLiteral("lspci"), {QStringLiteral("-mm"), QStringLiteral("-s"), m_gpu.slot});
            if (lspci.waitForFinished(2000)) {
                static const QRegularExpression q(QStringLiteral("\"([^\"]*)\""));
                QStringList fields;
                auto it = q.globalMatch(QString::fromUtf8(lspci.readAllStandardOutput()));
                while (it.hasNext())
                    fields.append(it.next().captured(1));
                if (fields.size() >= 3) {
                    QString vendor = fields[1];
                    vendor.remove(QStringLiteral(" Corporation"));
                    if (vendor.contains(QLatin1String("AMD")))
                        vendor = QStringLiteral("AMD");
                    if (vendor.contains(QLatin1String("NVIDIA")))
                        vendor = QStringLiteral("NVIDIA");
                    m_gpu.name = vendor + QLatin1Char(' ') + fields[2];
                }
            }
            if (m_gpu.name.isEmpty())
                m_gpu.name = driver.toUpper() + QStringLiteral(" GPU");

            QSet<QString> &known = m_knownEngines[m_gpu.slot];
            if (driver == QLatin1String("i915")) {
                for (const char *e : {"render", "video", "video-enhance", "copy"})
                    known.insert(QLatin1String(e));
            } else if (driver == QLatin1String("xe")) {
                for (const char *e : {"rcs", "vcs", "vecs", "bcs", "ccs"})
                    known.insert(QLatin1String(e));
            }
            break;
        }
    }
    if (!m_gpu.present)
        return;

    GpuSample &g = s.gpu;
    g.present = true;
    g.name = m_gpu.name;
    g.driver = m_gpu.driver;
    g.card = m_gpu.card;
    g.slot = m_gpu.slot;

    bool ok = false;
    const QString c = m_gpu.sysCard;
    if (m_gpu.driver == QLatin1String("i915")) {
        quint64 v = readU64(c + QStringLiteral("/gt_act_freq_mhz"), &ok);
        if (!ok)
            v = readU64(c + QStringLiteral("/gt_cur_freq_mhz"), &ok);
        if (ok)
            g.curMhz = double(v);
        v = readU64(c + QStringLiteral("/gt_RP0_freq_mhz"), &ok);
        if (!ok)
            v = readU64(c + QStringLiteral("/gt_max_freq_mhz"), &ok);
        if (ok)
            g.maxMhz = double(v);
    } else if (m_gpu.driver == QLatin1String("xe")) {
        const QString f = c + QStringLiteral("/device/tile0/gt0/freq0/");
        quint64 v = readU64(f + QStringLiteral("act_freq"), &ok);
        if (!ok)
            v = readU64(f + QStringLiteral("cur_freq"), &ok);
        if (ok)
            g.curMhz = double(v);
        v = readU64(f + QStringLiteral("rp0_freq"), &ok);
        if (!ok)
            v = readU64(f + QStringLiteral("max_freq"), &ok);
        if (ok)
            g.maxMhz = double(v);
    } else if (m_gpu.driver == QLatin1String("amdgpu")) {
        static const QRegularExpression mhzRe(QStringLiteral("(\\d+)\\s*Mhz"), QRegularExpression::CaseInsensitiveOption);
        for (const QByteArray &l : slurp(QFile::encodeName(c + QStringLiteral("/device/pp_dpm_sclk")).constData()).split('\n')) {
            const auto m = mhzRe.match(QString::fromLatin1(l));
            if (!m.hasMatch())
                continue;
            const double v = m.captured(1).toDouble();
            g.maxMhz = qMax(g.maxMhz, v);
            if (l.contains('*'))
                g.curMhz = v;
        }
    }

    QSet<QString> engines = m_knownEngines.value(g.slot);
    const QHash<QString, double> util = m_engineUtil.value(g.slot);
    const QHash<QString, double> noPdev = m_engineUtil.value(QString());
    for (auto it = util.cbegin(); it != util.cend(); ++it)
        engines.insert(it.key());
    for (auto it = noPdev.cbegin(); it != noPdev.cend(); ++it)
        engines.insert(it.key());

    static const QStringList order = {QStringLiteral("render"), QStringLiteral("rcs"), QStringLiteral("gfx"),
                                      QStringLiteral("video"), QStringLiteral("vcs"), QStringLiteral("dec"),
                                      QStringLiteral("enc"), QStringLiteral("video-enhance"), QStringLiteral("vecs"),
                                      QStringLiteral("copy"), QStringLiteral("bcs"), QStringLiteral("compute"),
                                      QStringLiteral("ccs")};
    QStringList sorted(engines.cbegin(), engines.cend());
    std::sort(sorted.begin(), sorted.end(), [](const QString &a, const QString &b) {
        const int ia = order.indexOf(a), ib = order.indexOf(b);
        if (ia != ib)
            return (ia < 0 ? 1000 : ia) < (ib < 0 ? 1000 : ib);
        return a < b;
    });
    for (const QString &e : std::as_const(sorted)) {
        EngineSample es;
        es.name = e;
        es.util = qMin(100.0, util.value(e) + noPdev.value(e));
        g.busy = qMax(g.busy, es.util);
        g.engines.append(es);
    }
}

// ---------------------------------------------------------------------------
// Power (Intel RAPL; readable by root only on most kernels)

void Sampler::readPower(Snapshot &s, double dt)
{
    QHash<QString, quint64> r = procscan::readRapl();
    if (r.isEmpty() && s.privileged)
        r = m_helperRapl;
    if (!r.isEmpty() && !m_prevRapl.isEmpty() && dt > 0) {
        auto watts = [&](const QString &n) -> double {
            if (!r.contains(n) || !m_prevRapl.contains(n))
                return -1;
            const quint64 a = m_prevRapl.value(n), b = r.value(n);
            quint64 d;
            if (b >= a) {
                d = b - a;
            } else {
                const quint64 mx = r.value(n + QStringLiteral("#max"));
                if (!mx || mx < a)
                    return -1;
                d = mx - a + b;
            }
            return double(d) / 1e6 / dt;
        };
        s.cpuPowerW = watts(QStringLiteral("package-0"));
        if (s.gpu.present)
            s.gpu.powerW = watts(QStringLiteral("uncore"));
    }
    m_prevRapl = r;
}

// ---------------------------------------------------------------------------
// Privileged helper

void Sampler::setHelperState(const QString &state, const QString &message)
{
    m_helperState = state;
    if (state == QLatin1String("off") && !m_pendingKills.isEmpty()) {
        const auto pending = m_pendingKills;
        m_pendingKills.clear();
        for (const auto &k : pending)
            Q_EMIT killResult(k.first, false,
                              message.isEmpty() ? tr("Administrator access is needed to end this process.") : message);
    }
    Q_EMIT helperStateChanged(state, message);
}

void Sampler::startHelper()
{
    if (m_helper)
        return;
    QString path = QStringLiteral(SYSQUILL_HELPER_PATH);
    if (!QFile::exists(path)) {
        const QString local = QCoreApplication::applicationDirPath() + QStringLiteral("/sysquill-helper");
        if (QFile::exists(local)) {
            path = local;
        } else {
            setHelperState(QStringLiteral("off"), tr("The helper program is missing (%1).").arg(path));
            return;
        }
    }
    auto *proc = new QProcess(this);
    m_helper = proc;
    proc->setProgram(QStringLiteral("pkexec"));
    proc->setArguments({path});
    connect(proc, &QProcess::readyRead, this, [this, proc] {
        if (m_helperState != QLatin1String("starting") || !proc->canReadLine())
            return;
        const QByteArray line = proc->readLine();
        if (!line.contains("ready"))
            return;
        setHelperState(QStringLiteral("on"), QString());
        const auto pending = m_pendingKills;
        m_pendingKills.clear();
        for (const auto &k : pending)
            helperKill(k.first, k.second);
        sampleNow();
    });
    connect(proc, &QProcess::finished, this, [this, proc](int code, QProcess::ExitStatus) {
        QString msg;
        if (m_helperState == QLatin1String("starting"))
            msg = (code == 126 || code == 127) ? tr("Administrator access was not granted.")
                                               : tr("The helper stopped (exit code %1).").arg(code);
        else if (m_helperState == QLatin1String("on"))
            msg = tr("Administrator access ended.");
        if (m_helper == proc)
            m_helper = nullptr;
        proc->deleteLater();
        setHelperState(QStringLiteral("off"), msg);
    });
    connect(proc, &QProcess::errorOccurred, this, [this, proc](QProcess::ProcessError e) {
        if (e != QProcess::FailedToStart)
            return;
        if (m_helper == proc)
            m_helper = nullptr;
        proc->deleteLater();
        setHelperState(QStringLiteral("off"), tr("Could not run pkexec. Install the \"pkexec\" package."));
    });
    setHelperState(QStringLiteral("starting"), tr("Waiting for administrator password…"));
    proc->start();
}

void Sampler::stopHelper()
{
    if (!m_helper)
        return;
    QProcess *proc = m_helper;
    proc->write("quit\n");
    proc->closeWriteChannel();
    proc->waitForFinished(1000);
}

QByteArray Sampler::helperCommand(const QByteArray &cmd, int timeoutMs)
{
    if (!m_helper)
        return {};
    m_helper->write(cmd + '\n');
    QElapsedTimer t;
    t.start();
    while (m_helper && !m_helper->canReadLine()) {
        const int remaining = timeoutMs - int(t.elapsed());
        if (remaining <= 0 || !m_helper->waitForReadyRead(remaining))
            return {};
    }
    if (!m_helper)
        return {};
    return m_helper->readLine().trimmed();
}

void Sampler::helperKill(int pid, int sig)
{
    const QByteArray r = helperCommand("kill " + QByteArray::number(pid) + ' ' + QByteArray::number(sig), 3000);
    const QJsonObject o = QJsonDocument::fromJson(r).object();
    if (o.value(QStringLiteral("ok")).toBool())
        Q_EMIT killResult(pid, true, QString());
    else
        Q_EMIT killResult(pid, false, o.value(QStringLiteral("err")).toString(tr("The helper did not answer.")));
}

void Sampler::requestKill(int pid, int sig)
{
    if (pid <= 1) {
        Q_EMIT killResult(pid, false, tr("Refusing to signal process %1.").arg(pid));
        return;
    }
    if (::kill(pid, sig) == 0) {
        Q_EMIT killResult(pid, true, QString());
        return;
    }
    const int err = errno;
    if (err == ESRCH) {
        Q_EMIT killResult(pid, false, tr("The process has already exited."));
        return;
    }
    if (err != EPERM) {
        Q_EMIT killResult(pid, false, QString::fromLocal8Bit(strerror(err)));
        return;
    }
    if (m_helperState == QLatin1String("on")) {
        helperKill(pid, sig);
        return;
    }
    m_pendingKills.append({pid, sig});
    startHelper();
}
