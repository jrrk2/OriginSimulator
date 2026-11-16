#include <QApplication>
#include <QDebug>
#include "CelestronOriginSimulator.h"

#include <QCoreApplication>
#include "GaiaFetch.h"

int main(int argc, char *argv[]) {
    qputenv("QT_FORCE_STDERR_LOGGING", "1");      // the log on stderr even without a terminal, not in the system journal
    // maintenance: build the local star catalogue from ESA's Gaia archive (needs network once; the simulator itself never goes online for stars)
    for (int i = 1; i + 1 < argc; ++i)
        if (QString(argv[i]) == "--fetch-gaia") {
            QCoreApplication fetch(argc, argv);
            return GaiaFetch::run(QString(argv[i + 1]));
        } else if (QString(argv[i]) == "--fetch-hipparcos") {       // the bright stars Gaia lacks, from Hipparcos in ESA's archive
            QCoreApplication fetch(argc, argv);
            return GaiaFetch::runHipparcos(QString(argv[i + 1]));
        }
    for (int i = 1; i < argc; ++i)
        if (QString(argv[i]) == "--survey") qputenv("ORIGIN_SIM_SURVEY", "1");   // FITS snapshots from the HiPS survey image (needs network)
    QApplication app(argc, argv);
    
    CelestronOriginSimulator simulator;
    
//     if (false) qDebug() << "Celestron Origin Simulator is running...";
//     if (false) qDebug() << "WebSocket endpoint: ws://localhost/SmartScope-1.0/mountControlEndpoint";
//     if (false) qDebug() << "HTTP images: http://localhost/SmartScope-1.0/dev2/Images";
//     if (false) qDebug() << "Press Ctrl+C to exit.";
    
    return app.exec();
}

// #include "main.moc"
