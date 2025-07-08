// simple_hips_test.cpp - Minimal HiPS connectivity test
#include <QCoreApplication>
#include <QDebug>
#include "ProperHipsClient.h"

int main(int argc, char *argv[]) {
    QCoreApplication app(argc, argv);
    
    if (false) qDebug() << "Simple HiPS Test - Testing M51 position";
    
    ProperHipsClient client;
    
    // Test M51 position with working survey
    SkyPosition m51 = {202.4695833, 47.1951667, "M51", "Whirlpool Galaxy"};
    QString url = client.buildTileUrl("DSS2_Color", m51, 6);
    
    if (false) qDebug() << "M51 test URL:" << url;
    
    if (!url.isEmpty()) {
        if (false) qDebug() << "✅ URL generation successful";
        client.testSurveyAtPosition("DSS2_Color", m51);
    } else {
        if (false) qDebug() << "❌ URL generation failed";
        return 1;
    }
    
    return app.exec();
}
