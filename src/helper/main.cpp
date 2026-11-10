// SPDX-License-Identifier: GPL-3.0-or-later
//
// sysquill-helper: started by Sysquill through pkexec when the user clicks
// "Unlock all processes". It runs as root and answers a tiny line protocol
// on stdin/stdout:
//
//   scan                -> one JSON line with I/O, open files and GPU use of every process
//   kill <pid> <signal> -> {"ok":true} or {"ok":false,"err":"..."}
//   quit                -> exit
//
// It exits on its own when Sysquill closes (stdin reaches end of file).
#include "procscan.h"

#include <QJsonDocument>
#include <QJsonObject>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>

static void reply(const QByteArray &line)
{
    fwrite(line.constData(), 1, size_t(line.size()), stdout);
    fputc('\n', stdout);
    fflush(stdout);
}

int main()
{
    reply("{\"ready\":true}");

    char buf[512];
    while (fgets(buf, sizeof buf, stdin)) {
        const QByteArray line = QByteArray(buf).trimmed();
        if (line == "scan") {
            QHash<int, PidExtras> all;
            const QList<int> pids = procscan::listPids();
            for (int pid : pids)
                all.insert(pid, procscan::scanPid(pid));
            reply(QJsonDocument(procscan::encode(all, procscan::readRapl())).toJson(QJsonDocument::Compact));
        } else if (line.startsWith("kill ")) {
            const QList<QByteArray> parts = line.split(' ');
            bool okPid = false, okSig = false;
            const int pid = parts.value(1).toInt(&okPid);
            const int sig = parts.value(2).toInt(&okSig);
            QJsonObject r;
            const bool allowedSignal = sig == SIGTERM || sig == SIGKILL || sig == SIGSTOP || sig == SIGCONT;
            if (parts.size() != 3 || !okPid || !okSig || pid <= 1 || !allowedSignal) {
                r.insert(QStringLiteral("ok"), false);
                r.insert(QStringLiteral("err"), QStringLiteral("Invalid request."));
            } else if (::kill(pid, sig) == 0) {
                r.insert(QStringLiteral("ok"), true);
            } else {
                r.insert(QStringLiteral("ok"), false);
                r.insert(QStringLiteral("err"), QString::fromLocal8Bit(strerror(errno)));
            }
            reply(QJsonDocument(r).toJson(QJsonDocument::Compact));
        } else if (line == "quit") {
            break;
        } else {
            reply("{\"ok\":false,\"err\":\"Unknown command.\"}");
        }
    }
    return 0;
}
