// SteamSwitch entry point. Inits Qt, installs the QNetwork HTTP fetcher into the
// core, exposes the Backend to QML, and loads the UI. No sidecar, no web view —
// the whole app is this one native process.

#include "Backend.h"
#include "QtFetcher.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QTextStream>

// Mirror every Qt log message (incl. QML load errors) to startup.log next to the
// exe, so failures are visible even in the windowed build. Temporary debug aid.
static void logToFile(QtMsgType, const QMessageLogContext&, const QString& msg) {
    static QFile f(QCoreApplication::applicationDirPath() + "/startup.log");
    static bool open = f.open(QIODevice::Append | QIODevice::Text);
    if (open) { QTextStream(&f) << msg << '\n'; f.flush(); }
    fprintf(stderr, "%s\n", msg.toLocal8Bit().constData());
    fflush(stderr);
}

int main(int argc, char* argv[]) {
    QGuiApplication app(argc, argv);
    app.setOrganizationName("SteamSwitch");
    app.setApplicationName("SteamSwitch");
    qInstallMessageHandler(logToFile);

    // Load the bundled UI fonts and make Hanken Grotesk the default, so the look
    // matches the web build instead of falling back to Segoe UI.
    for (const QString& f : {
             ":/fonts/hanken-grotesk-400.ttf", ":/fonts/hanken-grotesk-500.ttf",
             ":/fonts/hanken-grotesk-600.ttf", ":/fonts/hanken-grotesk-700.ttf",
             ":/fonts/geist-500.ttf", ":/fonts/geist-600.ttf",
             ":/fonts/cairo-arabic-400.ttf", ":/fonts/cairo-arabic-600.ttf",
             ":/fonts/cairo-arabic-700.ttf"})
        QFontDatabase::addApplicationFont(f);
    {
        QFont base("Hanken Grotesk");
        base.setPixelSize(14);
        app.setFont(base);
    }

    // core/ does HTTP through this injected fetcher (keeps core Qt-free).
    ss::ui::installQtFetcher();

    ss::ui::Backend backend;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);

    // Surface QML load failures explicitly instead of silently exiting.
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { qWarning("QML failed to load — see messages above / startup.log"); });
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app,
                     [](const QList<QQmlError>& ws) { for (const auto& w : ws) qWarning("%s", qPrintable(w.toString())); });

    engine.loadFromModule("SteamSwitch", "Main");
    if (engine.rootObjects().isEmpty()) {
        qWarning("No root QML object created — startup aborted.");
        return -1;
    }

    backend.refresh();  // initial scan -> stateChanged populates the grid
    return app.exec();
}
