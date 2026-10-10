#include <QApplication>
#include <QDebug>
#include "CelestronOriginSimulator.h"

int main(int argc, char *argv[]) {
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
