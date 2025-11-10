#ifndef TELESCOPESTATE_H
#define TELESCOPESTATE_H

#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QRandomGenerator>

class TelescopeState {
public:
    QString countryCode = "GB";  // From real data
  
    // Mount data (updated with exact real telescope values from session1.pcapng)
    QString batteryLevel = "HIGH";
    double batteryVoltage = 10.38;
    QString chargerStatus = "CHARGING";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString timeZone = "Europe/London";
    double latitude = 51.5072;   // London
    double longitude = 0.1276;
    bool isAligned = true;  // Real telescope shows aligned
    bool isGotoOver = true;
    bool isTracking = false;
    int numAlignRefs = 3;   // Realistic alignment
    double enc0 = 0.0;
    double enc1 = 0.0;
    
    // Camera data (exact real values from session1.pcapng)
    int binning = 1;
    int bitDepth = 24;
    double colorBBalance = 120.0;
    double colorGBalance = 58.0;
    double colorRBalance = 78.0;
    double exposure = 0.5;  // Real exposure time
    int iso = 2000;         // Real ISO setting
    int offset = 0;
    
    // Focuser data (exact real values)
    int backlash = 255;
    int calibrationLowerLimit = 1975;
    int calibrationUpperLimit = 37527;
    bool isCalibrationComplete = true;
    bool isMoveToOver = true;
    bool needAutoFocus = false;
    int percentageCalibrationComplete = 100;
    int position = 18447;   // Exact position from real data
    bool requiresCalibration = false;
    double velocity = -0.0; // Real shows -0.0
    
    // Environment data (exact real values from session1.pcapng)
    double ambientTemperature = 15.988;  // Real value
    double cameraTemperature = 24.3;     // Real value
    bool cpuFanOn = true;
    double cpuTemperature = 42.842;      // Real value
    double dewPoint = 8.108;             // Real value
    double frontCellTemperature = 11.35;
    double humidity = 67.0;
    bool otaFanOn = true;
    bool recalibrating = false;
    
    // Image data (real values from session1.pcapng with realistic progression)
    QString fileLocation = "";
    QString imageType = "LIVE";
    double dec = 0;     // Real Dec from capture
    double ra = 0;      // Will be calculated based on time
    double orientation = 3.120206959973186; // Real orientation
    double fovX = 0.021893731343283578;  // Real FOV
    double fovY = 0.014672238805970147;  // Real FOV
    
    // Disk data (real values)
    qint64 capacity = 58281033728;
    qint64 freeBytes = 52705251328;  // Slightly less free space
    QString level = "OK";
    
    // Dew Heater data (real values)
    int aggression = 5;
    double heaterLevel = 0.0;
    double manualPowerLevel = 0.0;
    QString mode = "Auto";
    
    // Orientation data (real values with variation)
    int altitude = 59; // Real data shows 59-60
    
    // Task Controller data
    bool isReady = false;
    QString stage = "IN_PROGRESS";
    QString state = "IDLE";
    
    // System version data (real version from capture)
    QString versionNumber = "1.1.4248";
    QString versionString = "1.1.4248\n (C++ = 09-04-2024 18:19, Java = 09-04-2024 18:19)";
    
    // Image sequence (realistic cycling)
    int sequenceNumber = 0;
    int imageCounter = 0;
    
    // Commands being executed
    bool isSlewing = false;
    bool isImaging = false;
    double targetRa = 0.0;
    double targetDec = 0.0;
    int imagingTimeLeft = 0;
    
    // Available directories for download (more realistic names)
    QStringList astrophotographyDirs = {
        "M31_Andromeda_Galaxy",
        "M42_Orion_Nebula", 
        "M51_Whirlpool_Galaxy",
        "M81_Bodes_Galaxy",
        "M101_Pinwheel_Galaxy",
        "NGC7635_Bubble_Nebula",
        "IC1396_Elephant_Trunk"
    };
    
    // Sequence ID management (matches real telescope pattern)
    int currentSequenceId = 16816;  // Start with real sequence ID from capture
    
    // Realistic progression variables
    double baseRA = 186.15 * M_PI / 180.0;
    double baseDec = 8.0 * M_PI / 180.0;
    double startTime = 0.0;

    // Initialization state
    struct InitializationInfo {
	    int numPoints = 0;
	    int positionOfFocus = -1;
	    int numPointsRemaining = 2;  // Default to 2 based on trace
	    int percentComplete = 0;
	};

	InitializationInfo initInfo;
	bool isFakeInitialized = false;
	bool isInitializing = false;
	int initializationProgress = 0;  // 0-100%

  void syncTracking() {
    startTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
  }
    TelescopeState() {
        syncTracking();
        // Initialize with some random variation like real telescope
        ambientTemperature += (QRandomGenerator::global()->bounded(200) - 100) / 100.0; // ±1°C variation
        cpuTemperature += (QRandomGenerator::global()->bounded(400) - 200) / 100.0;     // ±2°C variation
        dewPoint += (QRandomGenerator::global()->bounded(100) - 50) / 100.0;            // ±0.5°C variation
    }
    
    int getNextSequenceId() {
        return ++currentSequenceId;
    }
    
    // ExpiredAt timestamp (matches real telescope format - milliseconds)
    qint64 getExpiredAt() {
        // Real telescope uses timestamps around 1746444725915 (future timestamp)
        // This is roughly year 2025 + some months
        QDateTime future = QDateTime::currentDateTime().addSecs(60); // 1 minute in future
        return future.toMSecsSinceEpoch();
    }
    
    // Get current date/time in Origin telescope format
    QString getCurrentDate() {
        return dateTime.toString("dd MM yyyy");
    }
    
    QString getCurrentTime() {
        return dateTime.toString("hh:mm:ss");
    }
    
    // Update celestial coordinates based on time (simulate tracking)
    void updateCelestialCoordinates() {
        double currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
        double elapsedTime = currentTime - startTime;
        
        // Simulate sidereal tracking - RA changes ~15 arcsec/sec, Dec stays relatively stable
        // Real telescope shows small progressive changes
        double deltaRA = elapsedTime * 0.0000116; // Approximate sidereal rate
        double deltaDec = (QRandomGenerator::global()->bounded(20) - 10) * 0.0000001; // Small random variation
        
        ra = baseRA + deltaRA;
        dec = baseDec + deltaDec;
        
        // Orientation changes slightly over time
        orientation = 3.120206959973186 + (elapsedTime * 0.00001);
    }
    
    // Update environmental sensors with realistic variation
    void updateEnvironmentalSensors() {
        // Add small random variations like real sensors
        ambientTemperature += (QRandomGenerator::global()->bounded(10) - 5) / 1000.0;  // ±0.005°C
        cpuTemperature += (QRandomGenerator::global()->bounded(20) - 10) / 1000.0;     // ±0.01°C
        dewPoint += (QRandomGenerator::global()->bounded(6) - 3) / 1000.0;             // ±0.003°C
        
        // Keep within reasonable bounds
        if (ambientTemperature < 15.0) ambientTemperature = 15.0;
        if (ambientTemperature > 17.0) ambientTemperature = 17.0;
        if (cpuTemperature < 42.0) cpuTemperature = 42.0;
        if (cpuTemperature > 45.0) cpuTemperature = 45.0;
        
        // Altitude varies between 59-60 like real data
        altitude = 59 + (QRandomGenerator::global()->bounded(2));
    }
    
    // Get next image filename (cycles through 0-9 like real telescope)
    QString getNextImageFile() {
        imageCounter = (imageCounter + 1) % 10;
        return QString("Images/Temp/%1.jpg").arg(imageCounter);
    }
    
    // Get next image filename (cycles through 0-9 like real telescope)
    QString getNextTIFFFile() {
        imageCounter = (imageCounter + 1) % 10;
        return QString("/tmp/Images_%1.tiff").arg(imageCounter);
    }
    
    // Update disk space (slowly decreasing like real usage)
    void updateDiskSpace() {
        // Decrease free space slowly (simulate image storage)
        static int updateCount = 0;
        if (++updateCount % 100 == 0) { // Every 100 updates
            freeBytes -= QRandomGenerator::global()->bounded(1000000); // Remove ~1MB
            if (freeBytes < capacity / 2) {
                freeBytes = capacity - 10000000; // Reset to reasonable level
            }
        }
    }
    
    // Factory calibration status (more realistic)
    bool isFactoryCalibrated = true;
    int numTimesCollimated = 2;
    int numTimesHotSpotCentered = 2;
    QStringList completedPhases = {
        "UPDATE",
        "HARDWARE_CALIBRATION", 
        "DARK_GENERATION",
        "FLAT_GENERATION",
        "FA_TEST",
        "BATTERY"
    };
    QString currentPhase = "IDLE";
};

#endif // TELESCOPESTATE_H
