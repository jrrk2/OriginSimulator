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
    double batteryCurrent = 0.68;   // Real battery current in amps
    double batteryVoltage = 10.38;
    QString chargerStatus = "CHARGING";
    QDateTime dateTime = QDateTime::currentDateTime();
    QString timeZone = "Europe/London";
    double latitude = 0.9112;    // ~52.2°N (Cambridge, UK) in radians
    double longitude = 0.00209;  // ~0.12°E in radians
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
    double exposedTime = 0.0; // Progress counter for App's exposure bar
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
    // Continuous-imaging session state (RunImaging). Distinct from the
    // one-shot Snapshot path which uses fileLocation alone.
    int     stackDepth = 0;             // increments per completed exposure
    QString imagingSessionDir;          // e.g. "Crab_Nebula_2026-05-15_07-30-00"
    QString imagingObjectName;          // from RunImaging "Name" field
    QString imagingUuid;                // GUID stamped on every STACKED_MASTER
    // ImagingInfo.{TotalTime,RemainingTime,ObjectInfoList} feed the App's
    // session-progress UI. TotalTime is the planned per-object duration in
    // seconds; the start epoch lets us compute RemainingTime live.
    double  imagingTotalSeconds = 600.0;
    qint64  imagingStartEpochMs = 0;
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
    
    // Camera mode
    bool isManualMode = false;     // True when camera is in manual mode

    // Commands being executed
    bool isSlewing = false;        // True for GotoRaDec slewing
    bool isAltAzSlewing = false;   // True for GotoAltAzm slewing
    bool isManualSlewing = false;  // True for manual Alt/Az Slew command
    bool isImaging = false;
    double targetRa = 0.0;
    double targetDec = 0.0;
    double targetAltitude = 0.0;   // Target for GotoAltAzm
    double targetAzimuth = 0.0;    // Target for GotoAltAzm
    int imagingTimeLeft = 0;

    // Slew motion control (for manual Slew command)
    double slewAltRate = 0;   // Altitude slew rate (deg/s, fractional allowed)
    double slewAzmRate = 0;   // Azimuth slew rate (deg/s, fractional allowed)
    double azimuth = M_PI;  // Current azimuth in radians (start at 180°)
    double altitude = M_PI / 4.0; // Current altitude in radians (start at 45°)

    // Cooling state (Camera)
    double coolingTargetTemp = -10.0;
    bool coolingEnabled = false;

    // LED Ring
    int ledBrightnessLevel = 50;

    // ElPanel (light panel)
    bool lightOn = false;
    int lightLevel = 0;

    // PEC
    bool pecEnabled = false;

    // WiFi / Network
    bool automaticUpdates = true;
    bool savePlateSolves = false;
    QString directConnectPassword = "celestron";
    bool forceDirectConnect = false;
    bool deviceIsHost = false;
    QString hostPin = "1234";

    // Optics
    QString opticsModel = "Origin";
    double focalLength = 700.0;
    double aperture = 140.0;

    // Autoguider
    bool autoguiderActive = false;
    double autoguiderRmsRA = 0.0;
    double autoguiderRmsDec = 0.0;

    // LiveStream
    bool liveStreamDisabled = false;

    // Stretch settings (ImageServer)
    double stretchBlackPoint = 0.0;
    double stretchWhitePoint = 1.0;
    
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
    // Random starting position — client must plate-solve to find it
    // Starting RA/Dec are computed from the default Alt/Az + observer
    // location in the constructor via altAzToRaDec(), so the simulator
    // always points somewhere reliably above the horizon with stars in
    // view. Random init here would leave the App with no stars at startup.
    double baseRA = 0.0;
    double baseDec = 0.0;
    double startTime = 0.0;

    // Initialization state
    struct InitializationInfo {
        int numPoints = 0;
        int positionOfFocus = -1;
        int numPointsRemaining = 2;  // Default to 2 based on trace
        int percentageComplete = 0;
        QString currentStep = "NONE"; // NONE, FOCUSING, MOVING_MOUNT, ALIGNING
    };

    struct FocusInfo {
        int position = 0;
        int percentageComplete = 0;
    };

    InitializationInfo initInfo;
    FocusInfo focusInfo;
    bool isFakeInitialized = false;
    bool isInitializing = false;
    int initializationProgress = 0;  // 0-100%
  
    TelescopeState() {
        startTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
        // Initialize with some random variation like real telescope
        ambientTemperature += (QRandomGenerator::global()->bounded(200) - 100) / 100.0; // ±1°C variation
        cpuTemperature += (QRandomGenerator::global()->bounded(400) - 200) / 100.0;     // ±2°C variation
        dewPoint += (QRandomGenerator::global()->bounded(100) - 50) / 100.0;            // ±0.5°C variation
        // Compute starting RA/Dec from default Alt=45°/Az=180° + latitude.
        // This guarantees the App opens onto a populated star field above
        // the horizon, rather than a random sky point that may be empty
        // (or below horizon) for the configured observer location.
        altAzToRaDec();
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
    
    // Update celestial coordinates based on time. For an actively-tracking
    // mount, RA/Dec should stay locked on the target — the mount compensates
    // for Earth's rotation. The previous version drifted RA at 2.4″/sec
    // (~16% of sidereal), which compounded to a 2° offset after an hour of
    // sitting on a target and effectively walked the DSO out of frame.
    // Per-frame mount jitter (walking noise + PE) is modelled separately in
    // renderGaiaImageForPosition, so leave the canonical pointing untouched.
    void updateCelestialCoordinates() {
        double currentTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
        double elapsedTime = currentTime - startTime;
        ra  = baseRA;
        dec = baseDec;
        // Slowly evolve the reported instrument orientation so status reads
        // aren't bit-identical frame to frame.
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

        // Update altitude/azimuth based on slew rate
        // Rate is in deg/sec, timer fires every 100ms = 0.1s
        double altDelta = slewAltRate * 0.1 * (M_PI / 180.0);  // deg/s * 0.1s -> rad
        double azmDelta = slewAzmRate * 0.1 * (M_PI / 180.0);

        altitude += altDelta;
        azimuth += azmDelta;

        // Wrap azimuth to 0-2π
        while (azimuth < 0) azimuth += 2.0 * M_PI;
        while (azimuth >= 2.0 * M_PI) azimuth -= 2.0 * M_PI;

        // Clamp altitude to valid range (-90° to +90°)
        const double MAX_ALT = M_PI / 2.0;
        const double MIN_ALT = -M_PI / 2.0;
        if (altitude > MAX_ALT) altitude = MAX_ALT;
        if (altitude < MIN_ALT) altitude = MIN_ALT;

        // Convert Alt/Az back to RA/Dec using observer location
        altAzToRaDec();
    }

    // Convert current Alt/Az to RA/Dec using observer latitude and current LST
    void altAzToRaDec() {
        double alt = altitude;
        double az = azimuth;
        double lat = latitude;  // already in radians

        // Dec = asin(sin(alt)*sin(lat) + cos(alt)*cos(lat)*cos(az))
        double sinDec = sin(alt)*sin(lat) + cos(alt)*cos(lat)*cos(az);
        if (sinDec > 1.0) sinDec = 1.0;
        if (sinDec < -1.0) sinDec = -1.0;
        dec = asin(sinDec);

        // Hour angle: cos(HA) = (sin(alt) - sin(lat)*sin(dec)) / (cos(lat)*cos(dec))
        double cosHA = (sin(alt) - sin(lat)*sin(dec)) / (cos(lat)*cos(dec));
        if (cosHA > 1.0) cosHA = 1.0;
        if (cosHA < -1.0) cosHA = -1.0;
        double ha = acos(cosHA);

        // HA is positive west, negative east; azimuth > π means west
        if (sin(az) > 0) ha = 2.0*M_PI - ha;

        // Compute LST from current UTC time
        double jd = computeJD();
        double T = (jd - 2451545.0) / 36525.0;
        double gmst = 280.46061837 + 360.98564736629*(jd - 2451545.0) +
                       0.000387933*T*T - T*T*T/38710000.0;
        gmst = fmod(gmst, 360.0);
        if (gmst < 0) gmst += 360.0;

        // LST = GMST + observer longitude (radians -> degrees)
        double lst = gmst + longitude * 180.0 / M_PI;  // longitude in radians
        lst = fmod(lst, 360.0);
        if (lst < 0) lst += 360.0;
        double lstRad = lst * M_PI / 180.0;

        // RA = LST - HA
        ra = lstRad - ha;
        while (ra < 0) ra += 2.0 * M_PI;
        while (ra >= 2.0 * M_PI) ra -= 2.0 * M_PI;

        // Update base coordinates so sidereal tracking stays in sync
        baseRA = ra;
        baseDec = dec;
        startTime = QDateTime::currentDateTime().toMSecsSinceEpoch() / 1000.0;
    }

    // Julian Date from current UTC
    double computeJD() const {
        QDateTime utc = QDateTime::currentDateTimeUtc();
        int y = utc.date().year();
        int m = utc.date().month();
        int d = utc.date().day();
        double h = utc.time().hour() + utc.time().minute()/60.0 + utc.time().second()/3600.0;
        if (m <= 2) { y--; m += 12; }
        int A = y / 100;
        int B = 2 - A + A/4;
        return (int)(365.25*(y+4716)) + (int)(30.6001*(m+1)) + d + h/24.0 + B - 1524.5;
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
