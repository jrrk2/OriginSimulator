#include <QApplication>
#include <QDebug>
#include "CelestronOriginSimulator.h"

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    CelestronOriginSimulator simulator;
    
//     qDebug() << "Celestron Origin Simulator is running...";
//     qDebug() << "WebSocket endpoint: ws://localhost/SmartScope-1.0/mountControlEndpoint";
//     qDebug() << "HTTP images: http://localhost/SmartScope-1.0/dev2/Images/Temp/";
//     qDebug() << "Press Ctrl+C to exit.";
    
    return app.exec();
}

// #include "main.moc"
