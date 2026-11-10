// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>
#include <cmath>

namespace fmt {

inline QString bytes(double b)
{
    if (b < 0 || std::isnan(b))
        return QStringLiteral("–");
    static const char *units[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB"};
    int i = 0;
    while (b >= 1024.0 && i < 5) {
        b /= 1024.0;
        ++i;
    }
    if (i == 0)
        return QString::number(qint64(b)) + QStringLiteral(" B");
    const int prec = b >= 100 ? 0 : 1;
    return QString::number(b, 'f', prec) + QLatin1Char(' ') + QLatin1String(units[i]);
}

inline QString rate(double b)
{
    if (b < 0 || std::isnan(b))
        return QStringLiteral("–");
    return bytes(b) + QStringLiteral("/s");
}

inline QString duration(double secs)
{
    if (secs < 0)
        return QStringLiteral("–");
    const qint64 t = qint64(secs);
    const qint64 d = t / 86400, h = (t % 86400) / 3600, m = (t % 3600) / 60, s = t % 60;
    if (d > 0)
        return QStringLiteral("%1d %2h %3m").arg(d).arg(h).arg(m);
    if (h > 0)
        return QStringLiteral("%1h %2m").arg(h).arg(m);
    return QStringLiteral("%1m %2s").arg(m).arg(s);
}

inline QString mhz(double v)
{
    if (v < 0)
        return QStringLiteral("–");
    if (v >= 1000)
        return QString::number(v / 1000.0, 'f', 2) + QStringLiteral(" GHz");
    return QString::number(qRound(v)) + QStringLiteral(" MHz");
}

} // namespace fmt
