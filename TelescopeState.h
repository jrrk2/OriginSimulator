#ifndef TELESCOPESTATE_H
#define TELESCOPESTATE_H

#include <QString>
#include <QDateTime>
#include <QStringList>
#include <QRandomGenerator>
#include <QDebug>

class TelescopeState {
public:
    QString countryCode = "GB";  // From real data
  
    // Mount data (updated with exact real telescope values from session1.pcapng)
    QString batteryLevel = "HIGH";
    double batteryVoltage = 10.38;
    QString chargerStatus = "CHARGING";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString timeZone = "Europe/London";
    double latitude = -0.5847;   // Cerro Observatory
    double longitude = 1.23; 
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
    // Note: altitude is now stored as double in radians for slew motion (see slew motion control section)
    
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
    bool isSlewing = false;        // True for GotoRaDec slewing
    bool isManualSlewing = false;  // True for manual Alt/Az Slew command
    bool isImaging = false;
    double targetRa = 0.0;
    double targetDec = 0.0;
    int imagingTimeLeft = 0;
    
    // Slew motion control (for manual Slew command)
    int slewAltRate = 0;   // Altitude slew rate (-9 to +9)
    int slewAzmRate = 0;   // Azimuth slew rate (-9 to +9)
    double azimuth = M_PI;  // Current azimuth in radians (start at 180°)
    double altitude = M_PI / 4.0; // Current altitude in radians (start at 45°)
    
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
  
    TelescopeState() {
        startTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
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
        
        // Note: altitude is now stored as double in radians for slew motion
        // The real telescope's "altitude" field in orientation status was 59-60 degrees
        // We keep our altitude in radians for motion calculations
    }
    
    // Get next image filename (cycles through 0-9 like real telescope)
    QString getNextImageFile() {
        imageCounter = (imageCounter + 1) % 10;
        return QString("Images/Temp/%1.jpg").arg(imageCounter);
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
    
    // Update slew motion (called every 100ms from timer)
    void updateSlewMotion() {
        if (!isManualSlewing || (slewAltRate == 0 && slewAzmRate == 0)) return;
        
        // Store old values for debug output
        double oldAltitude = altitude;
        double oldAzimuth = azimuth;
        
        // Update altitude based on slew rate
        // Rate of 9 = approximately 0.45° per 100ms = 4.5°/sec
        double altDelta = slewAltRate * 0.05 * (M_PI / 180.0);
        double azmDelta = slewAzmRate * 0.05 * (M_PI / 180.0);
        
        altitude += altDelta;
        azimuth += azmDelta;
        
        // Wrap azimuth to 0-2π
        while (azimuth < 0) azimuth += 2.0 * M_PI;
        while (azimuth >= 2.0 * M_PI) azimuth -= 2.0 * M_PI;
        
        // Clamp altitude to valid range (-90° to +90°)
        const double MAX_ALT = M_PI / 2.0;  // 90°
        const double MIN_ALT = -M_PI / 2.0; // -90°
        if (altitude > MAX_ALT) altitude = MAX_ALT;
        if (altitude < MIN_ALT) altitude = MIN_ALT;
        
        // Debug output showing motion
        double altDeg = altitude * 180.0 / M_PI;
        double azmDeg = azimuth * 180.0 / M_PI;
        double oldAltDeg = oldAltitude * 180.0 / M_PI;
        double oldAzmDeg = oldAzimuth * 180.0 / M_PI;
        
        if (slewAltRate != 0) {
            qDebug() << QString("🔭 ALT: %1° → %2° (Δ%3°, rate: %4)")
                        .arg(oldAltDeg, 6, 'f', 2)
                        .arg(altDeg, 6, 'f', 2)
                        .arg(altDeg - oldAltDeg, 5, 'f', 2)
                        .arg(slewAltRate);
        }
        
        if (slewAzmRate != 0) {
            qDebug() << QString("🔭 AZM: %1° → %2° (Δ%3°, rate: %4)")
                        .arg(oldAzmDeg, 6, 'f', 2)
                        .arg(azmDeg, 6, 'f', 2)
                        .arg(azmDeg - oldAzmDeg, 5, 'f', 2)
                        .arg(slewAzmRate);
        }
        
        // Convert Alt/Az back to RA/Dec (simplified, telescope does this automatically)
        updateCelestialCoordinates();
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
