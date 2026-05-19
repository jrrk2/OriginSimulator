#include "StatusSender.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>

StatusSender::StatusSender(TelescopeState *state, QObject *parent) 
    : QObject(parent), m_telescopeState(state) {
}

void StatusSender::addWebSocketClient(WebSocketConnection *client) {
    if (!m_webSocketClients.contains(client)) {
        m_webSocketClients.append(client);
    }
}

void StatusSender::removeWebSocketClient(WebSocketConnection *client) {
    m_webSocketClients.removeAll(client);
}

void StatusSender::sendJsonMessage(WebSocketConnection *wsConn, const QJsonObject &obj) {
    if (!wsConn) return;
    QJsonDocument doc(obj);
    QString message = doc.toJson(QJsonDocument::Compact); // Compact like real telescope
    wsConn->sendTextMessage(message);
}

void StatusSender::sendJsonMessageToAll(const QJsonObject &obj) {
    QJsonDocument doc(obj);
    QString message = doc.toJson(QJsonDocument::Compact);
    
    for (WebSocketConnection *wsConn : m_webSocketClients) {
        wsConn->sendTextMessage(message);
    }
}

void StatusSender::sendMountStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    // Update coordinates before sending
    m_telescopeState->updateCelestialCoordinates();
    
    QJsonObject mountStatus;
    mountStatus["Command"] = "GetStatus";
    mountStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    mountStatus["Alt"] = m_telescopeState->altitude;
    mountStatus["AltitudeError"] = 0.0;
    mountStatus["Azm"] = m_telescopeState->azimuth;
    mountStatus["AzimuthError"] = 0.0;
    mountStatus["BatteryCurrent"] = m_telescopeState->batteryCurrent;
    mountStatus["BatteryLevel"] = m_telescopeState->batteryLevel;
    mountStatus["BatteryVoltage"] = m_telescopeState->batteryVoltage;
    mountStatus["ChargerStatus"] = m_telescopeState->chargerStatus;
    mountStatus["Date"] = m_telescopeState->getCurrentDate();
    mountStatus["Time"] = m_telescopeState->getCurrentTime();
    mountStatus["TimeZone"] = m_telescopeState->timeZone;
    mountStatus["Latitude"] = m_telescopeState->latitude;
    mountStatus["Longitude"] = m_telescopeState->longitude;
    mountStatus["IsAligned"] = m_telescopeState->isAligned;
    mountStatus["IsGotoOver"] = m_telescopeState->isGotoOver;
    mountStatus["IsTracking"] = m_telescopeState->isTracking;
    mountStatus["NumAlignRefs"] = m_telescopeState->numAlignRefs;
    mountStatus["Ra"] = m_telescopeState->ra;
    mountStatus["Dec"] = m_telescopeState->dec;
    mountStatus["Enc0"] = m_telescopeState->enc0;
    mountStatus["Enc1"] = m_telescopeState->enc1;
    mountStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    
    if (sequenceId != -1) {
        // Response to a specific command
        mountStatus["SequenceID"] = sequenceId;
        mountStatus["Source"] = "Mount";
        mountStatus["Type"] = "Response";
        mountStatus["ErrorCode"] = 0;
        mountStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, mountStatus);
        }
    } else {
        // Broadcast notification
        mountStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        mountStatus["Source"] = "Mount";
        mountStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, mountStatus);
        } else {
            sendJsonMessageToAll(mountStatus);
        }
    }
}

void StatusSender::sendFocuserStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    QJsonObject focuserStatus;
    focuserStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    focuserStatus["Backlash"] = m_telescopeState->backlash;
    focuserStatus["CalibrationLowerLimit"] = m_telescopeState->calibrationLowerLimit;
    focuserStatus["CalibrationUpperLimit"] = m_telescopeState->calibrationUpperLimit;
    focuserStatus["IsCalibrationComplete"] = m_telescopeState->isCalibrationComplete;
    focuserStatus["IsMoveToOver"] = m_telescopeState->isMoveToOver;
    focuserStatus["NeedAutoFocus"] = m_telescopeState->needAutoFocus;
    focuserStatus["PercentageCalibrationComplete"] = m_telescopeState->percentageCalibrationComplete;
    focuserStatus["Position"] = m_telescopeState->position;
    focuserStatus["RequiresCalibration"] = m_telescopeState->requiresCalibration;
    focuserStatus["Velocity"] = m_telescopeState->velocity;
    focuserStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    
    if (sequenceId != -1) {
        focuserStatus["Command"] = "GetStatus";
        focuserStatus["SequenceID"] = sequenceId;
        focuserStatus["Source"] = "Focuser";
        focuserStatus["Type"] = "Response";
        focuserStatus["ErrorCode"] = 0;
        focuserStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, focuserStatus);
        }
    } else {
        focuserStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        focuserStatus["Source"] = "Focuser";
        focuserStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, focuserStatus);
        } else {
            sendJsonMessageToAll(focuserStatus);
        }
    }
}

void StatusSender::sendCameraParams(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    QJsonObject cameraParams;
    cameraParams["Destination"] = destination.isEmpty() ? "All" : destination;
    cameraParams["Binning"] = m_telescopeState->binning;
    cameraParams["BitDepth"] = m_telescopeState->bitDepth;
    cameraParams["ColorBBalance"] = m_telescopeState->colorBBalance;
    cameraParams["ColorGBalance"] = m_telescopeState->colorGBalance;
    cameraParams["ColorRBalance"] = m_telescopeState->colorRBalance;
    cameraParams["Exposure"] = m_telescopeState->exposure;
    cameraParams["ExposedTime"] = m_telescopeState->exposedTime;
    cameraParams["ISO"] = m_telescopeState->iso;
    cameraParams["Offset"] = m_telescopeState->offset;
    cameraParams["Temperature"] = m_telescopeState->cameraTemperature;
    cameraParams["ExpiredAt"] = m_telescopeState->getExpiredAt();
    // Command must be set on both Response and Notification — the App
    // filters notifications by Command and silently drops frames that
    // omit it, which suppresses the per-exposure progress bar.
    cameraParams["Command"] = "GetCaptureParameters";
    cameraParams["Source"]  = "Camera";

    if (sequenceId != -1) {
        cameraParams["SequenceID"] = sequenceId;
        cameraParams["Type"] = "Response";
        cameraParams["ErrorCode"] = 0;
        cameraParams["ErrorMessage"] = "";

        if (specificClient) {
            sendJsonMessage(specificClient, cameraParams);
        }
    } else {
        cameraParams["SequenceID"] = m_telescopeState->getNextSequenceId();
        cameraParams["Type"] = "Notification";

        if (specificClient) {
            sendJsonMessage(specificClient, cameraParams);
        } else {
            sendJsonMessageToAll(cameraParams);
        }
    }
}

void StatusSender::sendNewImageReady(WebSocketConnection *specificClient) {
    // Update coordinates; only cycle to next LIVE .jpg path if there's no
    // pending one-shot frame (SNAPSHOT TIFF or STACKED_MASTER from
    // RunImaging). Otherwise we'd clobber the path the simulator set.
    m_telescopeState->updateCelestialCoordinates();
    if (m_telescopeState->imageType == "LIVE")
        m_telescopeState->fileLocation = m_telescopeState->getNextImageFile();

    QJsonObject newImage;
    newImage["Command"]      = "NewImageReady";
    newImage["Destination"]  = "All";
    newImage["Ra"]           = m_telescopeState->ra;
    newImage["Dec"]          = m_telescopeState->dec;
    newImage["FovX"]         = m_telescopeState->fovX;
    newImage["FovY"]         = m_telescopeState->fovY;
    newImage["Orientation"]  = m_telescopeState->orientation;
    newImage["Source"]       = "ImageServer";
    newImage["ImageType"]    = m_telescopeState->imageType;
    newImage["FileLocation"] = m_telescopeState->fileLocation;
    newImage["ExpiredAt"]    = m_telescopeState->getExpiredAt();
    newImage["SequenceID"]   = m_telescopeState->getNextSequenceId();
    newImage["Type"]         = "Notification";
    newImage["ImageWidth"]   = 3056;
    newImage["ImageHeight"]  = 2048;
    newImage["ISO"]          = m_telescopeState->iso;
    newImage["ExposureTime"] = m_telescopeState->exposure;
    newImage["ImageLocation"] = "/home/core/SmartScopeCore/" + m_telescopeState->fileLocation;
    newImage["StretchBackground"] = 0.035;
    // Real-telescope StretchStrength differs by image type (confirmed in
    // dither.pcapng + origin1.pcapng): LIVE = 0.85, STACKED_MASTER = 0.9,
    // SAMPLE_CAPTURE = 0.8. The App may use this to set the display histogram
    // appropriately per kind of frame.
    if (m_telescopeState->imageType == "STACKED_MASTER")
        newImage["StretchStrength"] = 0.9;
    else if (m_telescopeState->imageType == "SAMPLE_CAPTURE")
        newImage["StretchStrength"] = 0.8;
    else
        newImage["StretchStrength"] = 0.85;

    // Extra fields the App expects on STACKED_MASTER frames during a
    // RunImaging session — matches the real Origin's dither.pcapng output.
    if (m_telescopeState->imageType == "STACKED_MASTER") {
        newImage["StackDepth"]   = m_telescopeState->stackDepth;
        newImage["ObjectName"]   = m_telescopeState->imagingObjectName;
        newImage["Uuid"]         = m_telescopeState->imagingUuid;
        newImage["Latitude"]     = m_telescopeState->latitude;
        newImage["Longitude"]    = m_telescopeState->longitude;
        newImage["Time"]         = QDateTime::currentDateTime().toString("HH:mm:ss");
        newImage["Telescope"]    = "";
        newImage["Mount"]        = "";
        newImage["Reducer"]      = "";
    }

    if (specificClient) {
        sendJsonMessage(specificClient, newImage);
    } else {
        sendJsonMessageToAll(newImage);
    }
}

void StatusSender::sendEnvironmentStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    // Update environmental sensors
    m_telescopeState->updateEnvironmentalSensors();
    
    QJsonObject envStatus;
    envStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    envStatus["AmbientTemperature"] = m_telescopeState->ambientTemperature;
    envStatus["CameraTemperature"] = m_telescopeState->cameraTemperature;
    envStatus["CpuFanOn"] = m_telescopeState->cpuFanOn;
    envStatus["CpuTemperature"] = m_telescopeState->cpuTemperature;
    envStatus["DewPoint"] = m_telescopeState->dewPoint;
    envStatus["FrontCellTemperature"] = m_telescopeState->frontCellTemperature;
    envStatus["Humidity"] = m_telescopeState->humidity;
    envStatus["OtaFanOn"] = m_telescopeState->otaFanOn;
    envStatus["Recalibrating"] = m_telescopeState->recalibrating;
    envStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    
    if (sequenceId != -1) {
        envStatus["Command"] = "GetStatus";
        envStatus["SequenceID"] = sequenceId;
        envStatus["Source"] = "Environment";
        envStatus["Type"] = "Response";
        envStatus["ErrorCode"] = 0;
        envStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, envStatus);
        }
    } else {
        envStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        envStatus["Source"] = "Environment";
        envStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, envStatus);
        } else {
            sendJsonMessageToAll(envStatus);
        }
    }
}

void StatusSender::sendDiskStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    // Update disk space
    m_telescopeState->updateDiskSpace();
    
    QJsonObject diskStatus;
    diskStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    diskStatus["Capacity"] = m_telescopeState->capacity;
    diskStatus["FreeBytes"] = m_telescopeState->freeBytes;
    diskStatus["Level"] = m_telescopeState->level;
    diskStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    
    if (sequenceId != -1) {
        diskStatus["Command"] = "GetStatus";
        diskStatus["SequenceID"] = sequenceId;
        diskStatus["Source"] = "Disk";
        diskStatus["Type"] = "Response";
        diskStatus["ErrorCode"] = 0;
        diskStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, diskStatus);
        }
    } else {
        diskStatus["Command"] = "GetStatus";
        diskStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        diskStatus["Source"] = "Disk";
        diskStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, diskStatus);
        } else {
            sendJsonMessageToAll(diskStatus);
        }
    }
}

void StatusSender::sendDewHeaterStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    QJsonObject dewHeaterStatus;
    dewHeaterStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    dewHeaterStatus["Aggression"] = m_telescopeState->aggression;
    dewHeaterStatus["HeaterLevel"] = m_telescopeState->heaterLevel;
    dewHeaterStatus["ManualPowerLevel"] = m_telescopeState->manualPowerLevel;
    dewHeaterStatus["Mode"] = m_telescopeState->mode;
    dewHeaterStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    
    if (sequenceId != -1) {
        dewHeaterStatus["Command"] = "GetStatus";
        dewHeaterStatus["SequenceID"] = sequenceId;
        dewHeaterStatus["Source"] = "DewHeater";
        dewHeaterStatus["Type"] = "Response";
        dewHeaterStatus["ErrorCode"] = 0;
        dewHeaterStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, dewHeaterStatus);
        }
    } else {
        dewHeaterStatus["Command"] = "GetStatus";
        dewHeaterStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        dewHeaterStatus["Source"] = "DewHeater";
        dewHeaterStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, dewHeaterStatus);
        } else {
            sendJsonMessageToAll(dewHeaterStatus);
        }
    }
}

void StatusSender::sendOrientationStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    // Update altitude
    m_telescopeState->updateEnvironmentalSensors(); // This updates altitude too

    // Convert altitude from radians to degrees for the inclinometer reading,
    // and add ±1° noise to simulate real inclinometer accuracy
    double altDeg = m_telescopeState->altitude * 180.0 / M_PI;
    double noise = (QRandomGenerator::global()->bounded(2000) - 1000) / 1000.0; // ±1°
    double inclinometerAlt = altDeg + noise;

    QJsonObject orientationStatus;
    orientationStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    orientationStatus["Altitude"] = inclinometerAlt;
    orientationStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    
    if (sequenceId != -1) {
        orientationStatus["Command"] = "GetStatus";
        orientationStatus["SequenceID"] = sequenceId;
        orientationStatus["Source"] = "OrientationSensor";
        orientationStatus["Type"] = "Response";
        orientationStatus["ErrorCode"] = 0;
        orientationStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, orientationStatus);
        }
    } else {
        orientationStatus["Command"] = "GetStatus";
        orientationStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        orientationStatus["Source"] = "OrientationSensor";
        orientationStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, orientationStatus);
        } else {
            sendJsonMessageToAll(orientationStatus);
        }
    }
}

void StatusSender::sendTaskControllerStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    QJsonObject taskStatus;
    taskStatus["Destination"] = destination.isEmpty() ? "All" : destination;
    taskStatus["IsReady"] = m_telescopeState->isReady;
    taskStatus["Stage"] = m_telescopeState->stage;
    taskStatus["State"] = m_telescopeState->state;
    taskStatus["ExpiredAt"] = m_telescopeState->getExpiredAt();
    taskStatus["IsFakeInitialized"] = m_telescopeState->isFakeInitialized;

    // ImagingInfo drives the App's "target in progress" UI. Emit only while
    // an imaging task is active. Two shapes match the real device:
    //   • CENTERING_TARGET — minimal block (no ObjectInfoList yet)
    //   • IMAGING_OBJECT   — full block with ObjectInfoList[0] populated
    const QString &st = m_telescopeState->state;
    if (st == "CENTERING_TARGET" || st == "IMAGING_OBJECT") {
        QJsonObject imagingInfo;
        imagingInfo["ListUuid"] = "";
        imagingInfo["NumObjectsRemaining"] = 0;
        imagingInfo["TotalObjects"] = 1;
        if (st == "IMAGING_OBJECT") {
            imagingInfo["ImageUuid"] = m_telescopeState->imagingUuid;
            QJsonObject obj;
            obj["ExposedTime"]      = m_telescopeState->exposedTime;
            obj["MinimumStartTime"] = "";
            obj["ObjectName"]       = m_telescopeState->imagingObjectName;
            const double total = m_telescopeState->imagingTotalSeconds;
            double elapsedSec = 0.0;
            if (m_telescopeState->imagingStartEpochMs > 0)
                elapsedSec = (QDateTime::currentDateTime().toMSecsSinceEpoch()
                              - m_telescopeState->imagingStartEpochMs) / 1000.0;
            obj["RemainingTime"]    = qMax(0.0, total - elapsedSec);
            obj["StackDepth"]       = m_telescopeState->stackDepth;
            obj["StackingStatus"]   = "RUNNING";
            obj["TotalTime"]        = total;
            obj["Uuid"]             = m_telescopeState->imagingUuid;
            QJsonArray list;
            list.append(obj);
            imagingInfo["ObjectInfoList"] = list;
        }
        taskStatus["ImagingInfo"] = imagingInfo;
    }

    // Add initialization info if in INITIALIZING state or just completed
    if (m_telescopeState->state == "INITIALIZING" || m_telescopeState->stage == "COMPLETE") {
        QJsonObject initInfo;
        initInfo["CurrentStep"] = m_telescopeState->initInfo.currentStep;
        initInfo["NumPoints"] = m_telescopeState->initInfo.numPoints;
        initInfo["PercentageComplete"] = m_telescopeState->initInfo.percentageComplete;

        taskStatus["InitializationInfo"] = initInfo;

        // Add focus info when focusing or after focus is found
        if (m_telescopeState->focusInfo.position > 0) {
            QJsonObject focusInfo;
            focusInfo["Position"] = m_telescopeState->focusInfo.position;
            focusInfo["PercentageComplete"] = m_telescopeState->focusInfo.percentageComplete;
            taskStatus["FocusInfo"] = focusInfo;
        }

        // Only add this flag if we're in a completion or post-init state
        if (m_telescopeState->stage == "COMPLETE" || m_telescopeState->state == "IDLE") {
            taskStatus["IsFakeInitialized"] = m_telescopeState->isFakeInitialized;
        }
    }
    
    if (sequenceId != -1) {
        taskStatus["Command"] = "GetStatus";
        taskStatus["SequenceID"] = sequenceId;
        taskStatus["Source"] = "TaskController";
        taskStatus["Type"] = "Response";
        taskStatus["ErrorCode"] = 0;
        taskStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, taskStatus);
        }
    } else {
        taskStatus["Command"] = "GetStatus";
        taskStatus["SequenceID"] = m_telescopeState->getNextSequenceId();
        taskStatus["Source"] = "TaskController";
        taskStatus["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, taskStatus);
        } else {
            sendJsonMessageToAll(taskStatus);
        }
    }
}

void StatusSender::sendSystemVersion(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject versionResponse;
    versionResponse["Command"] = "GetVersion";
    versionResponse["Destination"] = destination;
    versionResponse["ErrorCode"] = 0;
    versionResponse["ErrorMessage"] = "";
    versionResponse["ExpiredAt"] = 0;
    versionResponse["Number"] = m_telescopeState->versionNumber;
    versionResponse["SequenceID"] = sequenceId;
    versionResponse["Source"] = "System";
    versionResponse["Type"] = "Response";
    versionResponse["Version"] = m_telescopeState->versionString;
    
    sendJsonMessage(wsConn, versionResponse);
}

void StatusSender::sendSystemModel(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject modelResponse;
    modelResponse["Command"] = "GetModel";
    modelResponse["Destination"] = destination;
    modelResponse["ErrorCode"] = 0;
    modelResponse["ErrorMessage"] = "";
    modelResponse["ExpiredAt"] = m_telescopeState->getExpiredAt();
    modelResponse["SequenceID"] = sequenceId;
    modelResponse["Source"] = "System";
    modelResponse["Type"] = "Response";
    modelResponse["Value"] = "Origin";
    
    QJsonArray devices;
    devices.append("System");
    devices.append("TaskController");
    devices.append("Imaging");
    devices.append("Mount");
    devices.append("Focuser");
    devices.append("Camera");
    devices.append("WiFi");
    devices.append("DewHeater");
    devices.append("Environment");
    devices.append("LedRing");
    devices.append("OrientationSensor");
    devices.append("Debug");
    
    modelResponse["Devices"] = devices;
    
    sendJsonMessage(wsConn, modelResponse);
}

void StatusSender::sendCameraFilter(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject filterResponse;
    filterResponse["Command"] = "GetFilter";
    filterResponse["Destination"] = destination;
    filterResponse["ErrorCode"] = 0;
    filterResponse["ErrorMessage"] = "";
    filterResponse["ExpiredAt"] = m_telescopeState->getExpiredAt();
    filterResponse["Filter"] = "Clear";
    filterResponse["SequenceID"] = sequenceId;
    filterResponse["Source"] = "Camera";
    filterResponse["Type"] = "Response";
    
    sendJsonMessage(wsConn, filterResponse);
}

void StatusSender::sendCalibrationStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject calResponse;
    calResponse["Command"] = "GetStatus";
    calResponse["Destination"] = destination;
    calResponse["ErrorCode"] = 0;
    calResponse["ErrorMessage"] = "";
    calResponse["ExpiredAt"] = m_telescopeState->getExpiredAt();
    calResponse["SequenceID"] = sequenceId;
    calResponse["Source"] = "FactoryCalibrationController";
    calResponse["Type"] = "Response";
    
    sendJsonMessage(wsConn, calResponse);
    
    // Also send factory calibration notification with realistic data
    QJsonObject calNotification;
    calNotification["Destination"] = "All";
    calNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    calNotification["IsCalibrated"] = m_telescopeState->isFactoryCalibrated;
    calNotification["NumTimesCollimated"] = m_telescopeState->numTimesCollimated;
    calNotification["NumTimesHotSpotCentered"] = m_telescopeState->numTimesHotSpotCentered;
    calNotification["SequenceID"] = m_telescopeState->getNextSequenceId();
    calNotification["Source"] = "FactoryCalibrationController";
    calNotification["Type"] = "Notification";
    calNotification["CurrentPhase"] = m_telescopeState->currentPhase;
    
    QJsonArray completedPhases;
    for (const QString &phase : m_telescopeState->completedPhases) {
        completedPhases.append(phase);
    }
    calNotification["CompletedPhases"] = completedPhases;
    
    sendJsonMessage(wsConn, calNotification);
}

void StatusSender::sendAutoguiderStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "Autoguider";
    status["Type"] = "Response";
    status["Active"] = m_telescopeState->autoguiderActive;
    status["RmsRA"] = m_telescopeState->autoguiderRmsRA;
    status["RmsDec"] = m_telescopeState->autoguiderRmsDec;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendLedRingStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "LedRing";
    status["Type"] = "Response";
    status["Level"] = m_telescopeState->ledBrightnessLevel;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendElPanelStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "ElPanel";
    status["Type"] = "Response";
    status["IsLightOn"] = m_telescopeState->lightOn;
    status["Level"] = m_telescopeState->lightLevel;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendNetworkStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "Network";
    status["Type"] = "Response";
    status["Connected"] = true;
    status["ForceDirectConnect"] = m_telescopeState->forceDirectConnect;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendLiveStreamStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "LiveStream";
    status["Type"] = "Response";
    status["EnableManual"] = m_telescopeState->isManualMode;
    status["Disabled"] = m_telescopeState->liveStreamDisabled;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendOpticsStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "Optics";
    status["Type"] = "Response";
    status["Model"] = m_telescopeState->opticsModel;
    status["FocalLength"] = m_telescopeState->focalLength;
    status["Aperture"] = m_telescopeState->aperture;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendHostControllerStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "HostController";
    status["Type"] = "Response";
    status["DeviceIsHost"] = m_telescopeState->deviceIsHost;

    sendJsonMessage(wsConn, status);
}

void StatusSender::sendCameraStatus(WebSocketConnection *wsConn, int sequenceId, const QString &destination) {
    QJsonObject status;
    status["Command"] = "GetStatus";
    status["Destination"] = destination;
    status["ErrorCode"] = 0;
    status["ErrorMessage"] = "";
    status["ExpiredAt"] = m_telescopeState->getExpiredAt();
    status["SequenceID"] = sequenceId;
    status["Source"] = "Camera";
    status["Type"] = "Response";
    status["Binning"] = m_telescopeState->binning;
    status["Exposure"] = m_telescopeState->exposure;
    status["ISO"] = m_telescopeState->iso;
    status["CoolingEnabled"] = m_telescopeState->coolingEnabled;
    status["CoolingTargetTemp"] = m_telescopeState->coolingTargetTemp;
    status["CameraTemperature"] = m_telescopeState->cameraTemperature;

    sendJsonMessage(wsConn, status);
}
