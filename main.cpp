#include <QApplication>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QVector>

#include "CelestronOriginSimulator.h"
#include "SimulatorMainWindow.h"

// Pre-GUI message buffer: the handler is installed before the simulator is
// constructed (so the constructor's burst of qDebug calls is captured), but
// the GUI window only exists later. Until it does, messages queue here and
// are flushed when the window comes up. Capped so a misbehaving init burst
// can't grow memory unboundedly.
static QPointer<SimulatorMainWindow> g_mainWindow;
static QVector<QString>              g_preGuiQueue;
static QMutex                        g_preGuiMutex;
static constexpr int                 kPreGuiCap = 2000;

static void guiOnlyMessageHandler(QtMsgType type, const QMessageLogContext&, const QString& msg)
{
    const char* tag = nullptr;
    switch (type) {
        case QtDebugMsg:    tag = "DBG"; break;
        case QtInfoMsg:     tag = "INF"; break;
        case QtWarningMsg:  tag = "WRN"; break;
        case QtCriticalMsg: tag = "CRT"; break;
        case QtFatalMsg:    tag = "FAT"; break;
    }
    const QString line = QString("[%1] %2").arg(tag).arg(msg);

    if (g_mainWindow) {
        // Marshal across threads — Qt may emit log messages from worker
        // threads (network IO, image rendering). QueuedConnection hands the
        // append back to the GUI thread safely.
        QMetaObject::invokeMethod(g_mainWindow.data(), "appendLog",
                                  Qt::QueuedConnection, Q_ARG(QString, line));
    } else {
        QMutexLocker lk(&g_preGuiMutex);
        g_preGuiQueue.append(line);
        if (g_preGuiQueue.size() > kPreGuiCap)
            g_preGuiQueue.removeFirst();
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // Install the GUI-only message handler FIRST so the simulator's
    // constructor (which fires a long burst of qDebug/qInfo lines) is
    // captured rather than escaping to stderr before the window exists.
    // Messages queue in g_preGuiQueue and are flushed once we have a window.
    qInstallMessageHandler(guiOnlyMessageHandler);

    // Parse CLI flags. These still drive STARTUP defaults; the GUI exposes
    // the same knobs as live controls.
    //   --dso=<filename-fragment>   restrict overlay + park at that DSO
    //   --rayleigh                  enable airmass-based sky brightening
    //   --dso-attenuation=<float>   initial DSO overlay brightness multiplier
    //   --bortle=<1..9>             sky-photon pedestal level
    //   --astro-dir=<path>          override Astrophotography session base dir
    QString dsoFilter;
    bool    rayleighEnabled = false;
    double  dsoAttenuation  = 1.0;
    int     bortleClass     = 5;
    QString astroDir;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--dso=")) {
            dsoFilter = arg.mid(6);
        } else if (arg == "--rayleigh") {
            rayleighEnabled = true;
        } else if (arg.startsWith("--dso-attenuation=")) {
            bool ok = false;
            const double v = arg.mid(18).toDouble(&ok);
            if (ok && v > 0.0) dsoAttenuation = v;
        } else if (arg.startsWith("--bortle=")) {
            bool ok = false;
            const int v = arg.mid(9).toInt(&ok);
            if (ok && v >= 1 && v <= 9) bortleClass = v;
        } else if (arg.startsWith("--astro-dir=")) {
            astroDir = arg.mid(12);
        }
    }

    CelestronOriginSimulator simulator(dsoFilter, rayleighEnabled,
                                       dsoAttenuation, bortleClass, astroDir);

    SimulatorMainWindow window(&simulator);
    g_mainWindow = &window;

    // Flush anything the simulator's constructor wrote before the window
    // existed. Take a copy under lock so any concurrent appends during the
    // flush still queue (they'll arrive via the normal post-window path).
    {
        QMutexLocker lk(&g_preGuiMutex);
        for (const QString& line : qAsConst(g_preGuiQueue))
            window.appendLog(line);
        g_preGuiQueue.clear();
    }

    qInfo() << "Startup: dso='" << dsoFilter << "' rayleigh=" << rayleighEnabled
            << " atten=" << dsoAttenuation << " bortle=" << bortleClass
            << " astro-dir=" << (astroDir.isEmpty() ? "<default>" : astroDir);

    window.show();
    const int rc = app.exec();
    g_mainWindow.clear();
    qInstallMessageHandler(nullptr);
    return rc;
}
