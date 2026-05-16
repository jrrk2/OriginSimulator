#include <QApplication>
#include <QDebug>
#include "CelestronOriginSimulator.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    // Parse CLI flags.
    //   --dso=<filename-fragment>   restrict overlay + park at that DSO
    //   --rayleigh                  enable airmass-based sky brightening
    //                               (off by default — washes out the App's
    //                                star detector at moderate altitudes)
    //   --dso-attenuation=<float>   multiply DSO overlay brightness
    //                               (1.0=default, <1 dims, >1 brightens)
    QString dsoFilter;
    bool    rayleighEnabled = false;
    double  dsoAttenuation  = 1.0;
    int     bortleClass     = 5;
    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg.startsWith("--dso=")) {
            dsoFilter = arg.mid(6);
        } else if (arg == "--rayleigh") {
            rayleighEnabled = true;
        } else if (arg.startsWith("--dso-attenuation=")) {
            bool ok = false;
            const double v = arg.mid(18).toDouble(&ok);
            if (ok && v > 0.0) {
                dsoAttenuation = v;
            } else {
                qWarning() << "Ignoring invalid --dso-attenuation:" << arg;
            }
        } else if (arg.startsWith("--bortle=")) {
            bool ok = false;
            const int v = arg.mid(9).toInt(&ok);
            if (ok && v >= 1 && v <= 9) {
                bortleClass = v;
            } else {
                qWarning() << "Ignoring invalid --bortle (use 1..9):" << arg;
            }
        }
    }
    qInfo() << "Startup flags: dso='" << dsoFilter
            << "'  rayleigh=" << (rayleighEnabled ? "on" : "off")
            << "  dso-attenuation=" << dsoAttenuation
            << "  bortle=" << bortleClass;

    CelestronOriginSimulator simulator(dsoFilter, rayleighEnabled, dsoAttenuation, bortleClass);
    return app.exec();
}
