// SPDX-License-Identifier: GPL-3.0-or-later
#include "procscan.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QSet>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace procscan {

QByteArray slurp(const char *path, int maxBytes)
{
    const int fd = ::open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0)
        return {};
    QByteArray out;
    char buf[8192];
    while (out.size() < maxBytes) {
        const ssize_t n = ::read(fd, buf, sizeof buf);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            break;
        out.append(buf, int(n));
    }
    ::close(fd);
    return out;
}

QByteArray readText(const QString &path)
{
    return slurp(QFile::encodeName(path).constData()).trimmed();
}

QList<int> listPids()
{
    QList<int> pids;
    DIR *d = opendir("/proc");
    if (!d)
        return pids;
    while (dirent *de = readdir(d)) {
        const char *n = de->d_name;
        if (*n < '1' || *n > '9')
            continue;
        bool digits = true;
        for (const char *c = n; *c; ++c) {
            if (*c < '0' || *c > '9') {
                digits = false;
                break;
            }
        }
        if (digits)
            pids.append(atoi(n));
    }
    closedir(d);
    return pids;
}

static void parseDrmFdinfo(const QByteArray &info, PidExtras *e, QSet<QString> *seen)
{
    DrmClient c;
    QString clientId;
    bool isDrm = false;
    for (const QByteArray &line : info.split('\n')) {
        const int colon = line.indexOf(':');
        if (colon <= 0)
            continue;
        const QByteArray k = line.left(colon);
        const QByteArray v = line.mid(colon + 1).trimmed();
        if (k == "drm-driver") {
            isDrm = true;
        } else if (k == "drm-client-id") {
            clientId = QString::fromLatin1(v);
        } else if (k == "drm-pdev") {
            c.pdev = QString::fromLatin1(v);
        } else if (k.startsWith("drm-engine-capacity-")) {
            c.capacity.insert(QString::fromLatin1(k.mid(20)), v.toInt());
        } else if (k.startsWith("drm-engine-")) {
            c.engineNs.insert(QString::fromLatin1(k.mid(11)), v.split(' ').value(0).toULongLong());
        } else if (k.startsWith("drm-total-cycles-")) {
            c.cycles[QString::fromLatin1(k.mid(17))].second = v.toULongLong();
        } else if (k.startsWith("drm-cycles-")) {
            c.cycles[QString::fromLatin1(k.mid(11))].first = v.toULongLong();
        }
    }
    if (!isDrm || clientId.isEmpty())
        return;
    c.key = c.pdev + QLatin1Char('#') + clientId;
    if (seen->contains(c.key))
        return;
    seen->insert(c.key);
    e->drm.append(c);
}

PidExtras scanPid(int pid)
{
    PidExtras e;
    char path[96];

    snprintf(path, sizeof path, "/proc/%d/io", pid);
    const QByteArray io = slurp(path, 4096);
    if (!io.isEmpty()) {
        for (const QByteArray &line : io.split('\n')) {
            if (line.startsWith("read_bytes:"))
                e.readBytes = line.mid(11).trimmed().toULongLong();
            else if (line.startsWith("write_bytes:"))
                e.writeBytes = line.mid(12).trimmed().toULongLong();
        }
        e.ioOk = true;
    }

    QSet<quint64> devs;
    QSet<QString> seenClients;
    struct stat st;

    snprintf(path, sizeof path, "/proc/%d/cwd", pid);
    if (::stat(path, &st) == 0)
        devs.insert(quint64(st.st_dev));

    snprintf(path, sizeof path, "/proc/%d/fd", pid);
    DIR *d = opendir(path);
    if (d) {
        e.fdOk = true;
        while (dirent *de = readdir(d)) {
            if (de->d_name[0] < '0' || de->d_name[0] > '9')
                continue;
            char fdPath[384];
            snprintf(fdPath, sizeof fdPath, "/proc/%d/fd/%s", pid, de->d_name);
            char target[512];
            const ssize_t n = readlink(fdPath, target, sizeof target - 1);
            if (n <= 0)
                continue;
            target[n] = 0;
            if (target[0] != '/')
                continue;  // sockets, pipes, anon inodes
            if (strncmp(target, "/dev/dri/", 9) == 0) {
                char infoPath[384];
                snprintf(infoPath, sizeof infoPath, "/proc/%d/fdinfo/%s", pid, de->d_name);
                parseDrmFdinfo(slurp(infoPath, 16384), &e, &seenClients);
                continue;
            }
            if (strncmp(target, "/dev/", 5) == 0 || strncmp(target, "/proc/", 6) == 0
                || strncmp(target, "/sys/", 5) == 0)
                continue;
            if (::stat(fdPath, &st) == 0)
                devs.insert(quint64(st.st_dev));
        }
        closedir(d);
    }
    e.devs.reserve(devs.size());
    for (quint64 v : std::as_const(devs))
        e.devs.append(v);
    return e;
}

QHash<QString, quint64> readRapl()
{
    QHash<QString, quint64> out;
    QDir dir(QStringLiteral("/sys/class/powercap"));
    const QStringList zones = dir.entryList({QStringLiteral("intel-rapl:*")},
                                            QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
    for (const QString &z : zones) {
        const QString base = dir.filePath(z);
        const QByteArray energy = readText(base + QStringLiteral("/energy_uj"));
        if (energy.isEmpty())
            continue;
        QString name = QString::fromUtf8(readText(base + QStringLiteral("/name")));
        if (name.isEmpty())
            name = z;
        if (out.contains(name))
            name += QLatin1Char('@') + z;
        out.insert(name, energy.toULongLong());
        out.insert(name + QStringLiteral("#max"),
                   readText(base + QStringLiteral("/max_energy_range_uj")).toULongLong());
    }
    return out;
}

// ---------------------------------------------------------------------------
// JSON transport between helper and app (one compact line per scan)

QJsonObject encode(const QHash<int, PidExtras> &procs, const QHash<QString, quint64> &rapl)
{
    QJsonObject p;
    for (auto it = procs.cbegin(); it != procs.cend(); ++it) {
        const PidExtras &e = it.value();
        QJsonObject o;
        if (e.ioOk) {
            o.insert(QStringLiteral("r"), double(e.readBytes));
            o.insert(QStringLiteral("w"), double(e.writeBytes));
        }
        if (e.fdOk)
            o.insert(QStringLiteral("f"), true);
        if (!e.devs.isEmpty()) {
            QJsonArray a;
            for (quint64 v : e.devs)
                a.append(double(v));
            o.insert(QStringLiteral("d"), a);
        }
        if (!e.drm.isEmpty()) {
            QJsonArray g;
            for (const DrmClient &c : e.drm) {
                QJsonObject co;
                co.insert(QStringLiteral("k"), c.key);
                co.insert(QStringLiteral("p"), c.pdev);
                QJsonObject en, ca, cy;
                for (auto i = c.engineNs.cbegin(); i != c.engineNs.cend(); ++i)
                    en.insert(i.key(), double(i.value()));
                for (auto i = c.capacity.cbegin(); i != c.capacity.cend(); ++i)
                    ca.insert(i.key(), i.value());
                for (auto i = c.cycles.cbegin(); i != c.cycles.cend(); ++i)
                    cy.insert(i.key(), QJsonArray{double(i.value().first), double(i.value().second)});
                co.insert(QStringLiteral("e"), en);
                co.insert(QStringLiteral("c"), ca);
                co.insert(QStringLiteral("y"), cy);
                g.append(co);
            }
            o.insert(QStringLiteral("g"), g);
        }
        p.insert(QString::number(it.key()), o);
    }
    QJsonObject r;
    for (auto it = rapl.cbegin(); it != rapl.cend(); ++it)
        r.insert(it.key(), double(it.value()));
    return QJsonObject{{QStringLiteral("p"), p}, {QStringLiteral("rapl"), r}};
}

void decode(const QJsonObject &obj, QHash<int, PidExtras> *procs, QHash<QString, quint64> *rapl)
{
    const QJsonObject p = obj.value(QStringLiteral("p")).toObject();
    for (auto it = p.constBegin(); it != p.constEnd(); ++it) {
        const QJsonObject o = it.value().toObject();
        PidExtras e;
        if (o.contains(QStringLiteral("r"))) {
            e.ioOk = true;
            e.readBytes = quint64(o.value(QStringLiteral("r")).toDouble());
            e.writeBytes = quint64(o.value(QStringLiteral("w")).toDouble());
        }
        e.fdOk = o.value(QStringLiteral("f")).toBool();
        const QJsonArray d = o.value(QStringLiteral("d")).toArray();
        for (const QJsonValue &v : d)
            e.devs.append(quint64(v.toDouble()));
        const QJsonArray g = o.value(QStringLiteral("g")).toArray();
        for (const QJsonValue &gv : g) {
            const QJsonObject co = gv.toObject();
            DrmClient c;
            c.key = co.value(QStringLiteral("k")).toString();
            c.pdev = co.value(QStringLiteral("p")).toString();
            const QJsonObject en = co.value(QStringLiteral("e")).toObject();
            for (auto i = en.constBegin(); i != en.constEnd(); ++i)
                c.engineNs.insert(i.key(), quint64(i.value().toDouble()));
            const QJsonObject ca = co.value(QStringLiteral("c")).toObject();
            for (auto i = ca.constBegin(); i != ca.constEnd(); ++i)
                c.capacity.insert(i.key(), i.value().toInt());
            const QJsonObject cy = co.value(QStringLiteral("y")).toObject();
            for (auto i = cy.constBegin(); i != cy.constEnd(); ++i) {
                const QJsonArray a = i.value().toArray();
                c.cycles.insert(i.key(), {quint64(a.at(0).toDouble()), quint64(a.at(1).toDouble())});
            }
            e.drm.append(c);
        }
        procs->insert(it.key().toInt(), e);
    }
    if (rapl) {
        rapl->clear();
        const QJsonObject r = obj.value(QStringLiteral("rapl")).toObject();
        for (auto it = r.constBegin(); it != r.constEnd(); ++it)
            rapl->insert(it.key(), quint64(it.value().toDouble()));
    }
}

} // namespace procscan
