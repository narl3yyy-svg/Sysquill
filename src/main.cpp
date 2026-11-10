// SPDX-License-Identifier: GPL-3.0-or-later
#include "systemmonitor.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QFile>
#include <QIcon>
#include <QPainter>
#include <QQmlApplicationEngine>
#include <QQuickImageProvider>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>

#include <cstdio>

// Lets QML show icons from the desktop icon theme: Image { source: "image://icon/drive-harddisk" }
class ThemeIconProvider : public QQuickImageProvider
{
public:
    ThemeIconProvider()
        : QQuickImageProvider(QQuickImageProvider::Pixmap)
    {
    }

    QPixmap requestPixmap(const QString &id, QSize *size, const QSize &requested) override
    {
        int w = requested.width() > 0 ? requested.width() : requested.height();
        int h = requested.height() > 0 ? requested.height() : requested.width();
        if (w <= 0 || h <= 0)
            w = h = 32;
        QIcon icon = QIcon::fromTheme(id);
        QPixmap pm;
        if (!icon.isNull()) {
            pm = icon.pixmap(QSize(w, h));
        } else {
            pm = QPixmap(w, h);
            pm.fill(Qt::transparent);
        }
        if (size)
            *size = pm.size();
        return pm;
    }
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("sysquill"));
    QApplication::setApplicationDisplayName(QStringLiteral("Sysquill"));
    QApplication::setApplicationVersion(QStringLiteral(SYSQUILL_VERSION));
    QApplication::setOrganizationName(QStringLiteral("Sysquill"));
    QApplication::setDesktopFileName(QStringLiteral(SYSQUILL_APP_ID));
    if (QIcon::themeName().isEmpty() || QIcon::themeName() == QLatin1String("hicolor"))
        QIcon::setThemeName(QStringLiteral("breeze"));
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(SYSQUILL_APP_ID),
                                                 QIcon(QStringLiteral(":/qt/qml/SysquillUi/data/" SYSQUILL_APP_ID ".svg"))));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Live system monitor for KDE Plasma."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption pageOpt(QStringLiteral("page"),
                                     QStringLiteral("Page to open: overview, processes, cpu, memory, disks, network, gpu, sensors."),
                                     QStringLiteral("name"));
    const QCommandLineOption unlockOpt(QStringLiteral("unlock"),
                                       QStringLiteral("Ask for administrator access at startup (shows all processes)."));
    const QCommandLineOption shotOpt(QStringLiteral("screenshot"),
                                     QStringLiteral("Render the window to an image file and quit (for testing)."),
                                     QStringLiteral("file"));
    const QCommandLineOption delayOpt(QStringLiteral("delay"),
                                      QStringLiteral("Milliseconds to wait before --screenshot (default 4000)."),
                                      QStringLiteral("ms"), QStringLiteral("4000"));
    const QCommandLineOption sizeOpt(QStringLiteral("size"),
                                     QStringLiteral("Window size for --screenshot, e.g. 1280x820."),
                                     QStringLiteral("WxH"));
    const QCommandLineOption dumpOpt(QStringLiteral("dump"),
                                     QStringLiteral("Print the collected data as JSON after a few seconds and quit."));
    parser.addOptions({pageOpt, unlockOpt, shotOpt, delayOpt, sizeOpt, dumpOpt});
    parser.process(app);

    auto *monitor = new SystemMonitor(&app);

    if (parser.isSet(dumpOpt)) {
        if (parser.isSet(unlockOpt))
            monitor->unlock();
        QTimer::singleShot(parser.isSet(unlockOpt) ? 6000 : 2500, &app, [monitor] {
            const QByteArray out = monitor->debugDump().toUtf8();
            fwrite(out.constData(), 1, size_t(out.size()), stdout);
            fflush(stdout);
            QCoreApplication::quit();
        });
        return app.exec();
    }

    QQmlApplicationEngine engine;
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        bool kde = false;
        const QStringList paths = engine.importPathList();
        for (const QString &p : paths) {
            if (QFile::exists(p + QStringLiteral("/org/kde/desktop/qmldir"))) {
                kde = true;
                break;
            }
        }
        QQuickStyle::setStyle(kde ? QStringLiteral("org.kde.desktop") : QStringLiteral("Fusion"));
    }
    engine.addImageProvider(QStringLiteral("icon"), new ThemeIconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("SysquillUi", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (window && parser.isSet(pageOpt))
        window->setProperty("startPage", parser.value(pageOpt));
    if (parser.isSet(unlockOpt))
        monitor->unlock();

    if (window && parser.isSet(shotOpt)) {
        const QStringList wh = parser.value(sizeOpt).split(QLatin1Char('x'));
        if (wh.size() == 2)
            window->resize(wh[0].toInt(), wh[1].toInt());
        const QString file = parser.value(shotOpt);
        QTimer::singleShot(parser.value(delayOpt).toInt(), window, [window, file] {
            const QImage img = window->grabWindow();
            const bool ok = img.save(file);
            fprintf(stderr, "%s %s (%dx%d)\n", ok ? "saved" : "FAILED to save", qPrintable(file), img.width(), img.height());
            QCoreApplication::exit(ok ? 0 : 2);
        });
    }
    return app.exec();
}
