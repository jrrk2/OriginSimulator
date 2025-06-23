#ifndef TELESCOPESTATE_H
#define TELESCOPESTATE_H

#include <QString>
#include <QDateTime>
#include <QStringList>

class TelescopeState {
public:
    // Mount data
    QString batteryLevel = "HIGH";
    double batteryVoltage = 10.38;
    QString chargerStatus = "CHARGING";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString timeZone = "Europe/London";
    double latitude = 0.5907141501744784;  // Radians (approximately Cambridge, UK)
    double longitude = -2.065392832281757; // Radians
    bool isAligned = false;
    bool isGotoOver = true;
    bool isTracking = false;
    int numAlignRefs = 0;
    double enc0 = 0.0;
    double enc1 = 0.0;
    
    // Camera data
    int binning = 1;
    int bitDepth = 24;
    double colorBBalance = 120.0;
    double colorGBalance = 58.0;
    double colorRBalance = 78.0;
    double exposure = 0.03;
    int iso = 100;
    int offset = 0;
    
    // Focuser data
    int backlash = 255;
    int calibrationLowerLimit = 1975;
    int calibrationUpperLimit = 37527;
    bool isCalibrationComplete = true;
    bool isMoveToOver = true;
    bool needAutoFocus = false;
    int percentageCalibrationComplete = 100;
    int position = 18386;
    bool requiresCalibration = false;
    double velocity = 0.0;
    
    // Environment data
    double ambientTemperature = 12.14;
    double cameraTemperature = 21.7;
    bool cpuFanOn = true;
    double cpuTemperature = 40.4;
    double dewPoint = 6.29;
    double frontCellTemperature = 11.35;
    double humidity = 67.0;
    bool otaFanOn = true;
    bool recalibrating = false;
    
    // Image data
    QString fileLocation = "Images/Temp/0.jpg";
    QString imageType = "LIVE";
    double dec = 0.973655;
    double ra = 3.83883;
    double orientation = 0.003215;
    double fovX = 0.021893731343283578;
    double fovY = 0.014672238805970147;
    
    // Disk data
    qint64 capacity = 58281033728;
    qint64 freeBytes = 52024094720;
    QString level = "OK";
    
    // Dew Heater data
    int aggression = 5;
    double heaterLevel = 0.0;
    double manualPowerLevel = 0.0;
    QString mode = "Auto";
    
    // Orientation data
    int altitude = 28;
    
    // Task Controller data
    bool isReady = false;
    QString stage = "IN_PROGRESS";
    QString state = "IDLE";
    
    // System version data
    QString versionNumber = "1.1.4248";
    QString versionString = "1.1.4248\n (C++ = 09-04-2024 18:19, Java = 09-04-2024 18:19)";
    
    // Image sequence
    int sequenceNumber = 1;
    
    // Commands being executed
    bool isSlewing = false;
    bool isImaging = false;
    double targetRa = 0.0;
    double targetDec = 0.0;
    int imagingTimeLeft = 0;
    
    // Available directories for download
    QStringList astrophotographyDirs = {
        "(4)_Vesta_05-05-25_22_30_25",
        "Bode's_Nebulae_05-05-25_22_00_53",
        "Messier_3_05-05-25_21_51_52",
        "Messier_101_05-03-25_22_33_50",
        "Whirlpool_Galaxy_05-03-25_21_58_46",
        "Jupiter_05-03-25_21_55_57"
    };
};

#endif // TELESCOPESTATE_H