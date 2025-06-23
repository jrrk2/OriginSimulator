#ifndef TELESCOPESTATE_H
#define TELESCOPESTATE_H

#include <QString>
#include <QDateTime>
#include <QStringList>

class TelescopeState {
public:
    QString countryCode;
  
    // Mount data (matches real Origin telescope values from pcapng)
    QString batteryLevel = "HIGH";
    double batteryVoltage = 10.38;
    QString chargerStatus = "CHARGING";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString timeZone = "Europe/London";
    double latitude = 0.5907141501744784;  // Real value from capture
    double longitude = -2.065392832281757; // Real value from capture
    bool isAligned = false;
    bool isGotoOver = true;
    bool isTracking = false;
    int numAlignRefs = 0;
    double enc0 = 0.0;
    double enc1 = 0.0;
    
    // Camera data (real values from capture)
    int binning = 1;
    int bitDepth = 24;
    double colorBBalance = 120.0;
    double colorGBalance = 58.0;
    double colorRBalance = 78.0;
    double exposure = 0.5;  // Changed from 0.03 to match real data
    int iso = 2000;         // Changed from 100 to match real data
    int offset = 0;
    
    // Focuser data (real values from capture)
    int backlash = 255;
    int calibrationLowerLimit = 1975;
    int calibrationUpperLimit = 37527;
    bool isCalibrationComplete = true;
    bool isMoveToOver = true;
    bool needAutoFocus = false;
    int percentageCalibrationComplete = 100;
    int position = 18481;   // Changed from 18386 to match real data
    bool requiresCalibration = false;
    double velocity = 0.0;
    
    // Environment data (real values from capture)
    double ambientTemperature = 18.778;  // Changed to match real data
    double cameraTemperature = 21.7;
    bool cpuFanOn = true;
    double cpuTemperature = 47.712;      // Changed to match real data
    double dewPoint = 6.29;
    double frontCellTemperature = 11.35;
    double humidity = 67.0;
    bool otaFanOn = true;
    bool recalibrating = false;
    
    // Image data (real values from capture)
    QString fileLocation = "Images/Temp/0.jpg";
    QString imageType = "LIVE";
    double dec = 1.4352459007788536;     // Real Dec from capture
    double ra = 3.8422300000000074;     // Real RA from capture
    double orientation = 0.003215;
    double fovX = 0.021893731343283578;
    double fovY = 0.014672238805970147;
    
    // Disk data (real values from capture)
    qint64 capacity = 58281033728;
    qint64 freeBytes = 52024094720;
    QString level = "OK";
    
    // Dew Heater data (real values from capture)
    int aggression = 5;
    double heaterLevel = 0.0;
    double manualPowerLevel = 0.0;
    QString mode = "Auto";
    
    // Orientation data (real values from capture)
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
    
    // Available directories for download (real directories from capture)
    QStringList astrophotographyDirs = {
        "Messier",
        "Whirlpool", 
        "Jupiter"
    };
    
    // Sequence ID management (matches real telescope pattern)
    int currentSequenceId = 4663;  // Start with real sequence ID from capture
    
    int getNextSequenceId() {
        return ++currentSequenceId;
    }
    
    // ExpiredAt timestamp (matches real telescope format)
    qint64 getExpiredAt() {
        // Real telescope uses timestamps like 1746455593965
        // This appears to be milliseconds since epoch + some offset
        return QDateTime::currentDateTime().toMSecsSinceEpoch() + 1000; // 1 second in future
    }
    
    // Get current date/time in Origin telescope format
    QString getCurrentDate() {
        return dateTime.toString("dd MM yyyy");
    }
    
    QString getCurrentTime() {
        return dateTime.toString("hh:mm:ss");
    }
};

#endif // TELESCOPESTATE_H
