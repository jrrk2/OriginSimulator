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
    QString message = doc.toJson();
    wsConn->sendTextMessage(message);
}

void StatusSender::sendJsonMessageToAll(const QJsonObject &obj) {
    QJsonDocument doc(obj);
    QString message = doc.toJson();
    
    for (WebSocketConnection *wsConn : m_webSocketClients) {
        wsConn->sendTextMessage(message);
    }
}

void StatusSender::sendMountStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    QJsonObject mountStatus;
    mountStatus["Command"] = "GetStatus";
    mountStatus["Destination"] = destination;
    mountStatus["BatteryLevel"] = m_telescopeState->batteryLevel;
    mountStatus["BatteryVoltage"] = m_telescopeState->batteryVoltage;
    mountStatus["ChargerStatus"] = m_telescopeState->chargerStatus;
    mountStatus["Date"] = m_telescopeState->dateTime.toString("dd MM yyyy");
    mountStatus["Time"] = m_telescopeState->dateTime.toString("hh:mm:ss");
    mountStatus["TimeZone"] = m_telescopeState->timeZone;
    mountStatus["Latitude"] = m_telescopeState->latitude;
    mountStatus["Longitude"] = m_telescopeState->longitude;
    mountStatus["IsAligned"] = m_telescopeState->isAligned;
    mountStatus["IsGotoOver"] = m_telescopeState->isGotoOver;
    mountStatus["IsTracking"] = m_telescopeState->isTracking;
    mountStatus["NumAlignRefs"] = m_telescopeState->numAlignRefs;
    mountStatus["Enc0"] = m_telescopeState->enc0;
    mountStatus["Enc1"] = m_telescopeState->enc1;
    mountStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
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
        mountStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
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
    focuserStatus["Command"] = "GetStatus";
    focuserStatus["Destination"] = destination;
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
    focuserStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        focuserStatus["SequenceID"] = sequenceId;
        focuserStatus["Source"] = "Focuser";
        focuserStatus["Type"] = "Response";
        focuserStatus["ErrorCode"] = 0;
        focuserStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, focuserStatus);
        }
    } else {
        focuserStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
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
    cameraParams["Command"] = "GetCaptureParameters";
    cameraParams["Destination"] = destination;
    cameraParams["Binning"] = m_telescopeState->binning;
    cameraParams["BitDepth"] = m_telescopeState->bitDepth;
    cameraParams["ColorBBalance"] = m_telescopeState->colorBBalance;
    cameraParams["ColorGBalance"] = m_telescopeState->colorGBalance;
    cameraParams["ColorRBalance"] = m_telescopeState->colorRBalance;
    cameraParams["Exposure"] = m_telescopeState->exposure;
    cameraParams["ISO"] = m_telescopeState->iso;
    cameraParams["Offset"] = m_telescopeState->offset;
    cameraParams["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        cameraParams["SequenceID"] = sequenceId;
        cameraParams["Source"] = "Camera";
        cameraParams["Type"] = "Response";
        cameraParams["ErrorCode"] = 0;
        cameraParams["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, cameraParams);
        }
    } else {
        cameraParams["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
        cameraParams["Source"] = "Camera";
        cameraParams["Type"] = "Notification";
        
        if (specificClient) {
            sendJsonMessage(specificClient, cameraParams);
        } else {
            sendJsonMessageToAll(cameraParams);
        }
    }
}

void StatusSender::sendNewImageReady(WebSocketConnection *specificClient) {
    QJsonObject newImage;
    newImage["Command"] = "NewImageReady";
    newImage["Destination"] = "All";
    newImage["Dec"] = m_telescopeState->dec;
    newImage["Ra"] = m_telescopeState->ra;
    newImage["FileLocation"] = m_telescopeState->fileLocation;
    newImage["FovX"] = m_telescopeState->fovX;
    newImage["FovY"] = m_telescopeState->fovY;
    newImage["ImageType"] = m_telescopeState->imageType;
    newImage["Orientation"] = m_telescopeState->orientation;
    newImage["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    newImage["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
    newImage["Source"] = "ImageServer";
    newImage["Type"] = "Notification";
    
    if (specificClient) {
        sendJsonMessage(specificClient, newImage);
    } else {
        sendJsonMessageToAll(newImage);
    }
}

void StatusSender::sendEnvironmentStatus(WebSocketConnection *specificClient, int sequenceId, const QString &destination) {
    QJsonObject envStatus;
    envStatus["Command"] = "GetStatus";
    envStatus["Destination"] = destination;
    envStatus["AmbientTemperature"] = m_telescopeState->ambientTemperature;
    envStatus["CameraTemperature"] = m_telescopeState->cameraTemperature;
    envStatus["CpuFanOn"] = m_telescopeState->cpuFanOn;
    envStatus["CpuTemperature"] = m_telescopeState->cpuTemperature;
    envStatus["DewPoint"] = m_telescopeState->dewPoint;
    envStatus["FrontCellTemperature"] = m_telescopeState->frontCellTemperature;
    envStatus["Humidity"] = m_telescopeState->humidity;
    envStatus["OtaFanOn"] = m_telescopeState->otaFanOn;
    envStatus["Recalibrating"] = m_telescopeState->recalibrating;
    envStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        envStatus["SequenceID"] = sequenceId;
        envStatus["Source"] = "Environment";
        envStatus["Type"] = "Response";
        envStatus["ErrorCode"] = 0;
        envStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, envStatus);
        }
    } else {
        envStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
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
    QJsonObject diskStatus;
    diskStatus["Command"] = "GetStatus";
    diskStatus["Destination"] = destination;
    diskStatus["Capacity"] = QString::number(m_telescopeState->capacity);
    diskStatus["FreeBytes"] = QString::number(m_telescopeState->freeBytes);
    diskStatus["Level"] = m_telescopeState->level;
    diskStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        diskStatus["SequenceID"] = sequenceId;
        diskStatus["Source"] = "Disk";
        diskStatus["Type"] = "Response";
        diskStatus["ErrorCode"] = 0;
        diskStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, diskStatus);
        }
    } else {
        diskStatus["SequenceID"] = 1000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 100);
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
    dewHeaterStatus["Command"] = "GetStatus";
    dewHeaterStatus["Destination"] = destination;
    dewHeaterStatus["Aggression"] = m_telescopeState->aggression;
    dewHeaterStatus["HeaterLevel"] = m_telescopeState->heaterLevel;
    dewHeaterStatus["ManualPowerLevel"] = m_telescopeState->manualPowerLevel;
    dewHeaterStatus["Mode"] = m_telescopeState->mode;
    dewHeaterStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        dewHeaterStatus["SequenceID"] = sequenceId;
        dewHeaterStatus["Source"] = "DewHeater";
        dewHeaterStatus["Type"] = "Response";
        dewHeaterStatus["ErrorCode"] = 0;
        dewHeaterStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, dewHeaterStatus);
        }
    } else {
        dewHeaterStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
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
    QJsonObject orientationStatus;
    orientationStatus["Command"] = "GetStatus";
    orientationStatus["Destination"] = destination;
    orientationStatus["Altitude"] = m_telescopeState->altitude;
    orientationStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        orientationStatus["SequenceID"] = sequenceId;
        orientationStatus["Source"] = "OrientationSensor";
        orientationStatus["Type"] = "Response";
        orientationStatus["ErrorCode"] = 0;
        orientationStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, orientationStatus);
        }
    } else {
        orientationStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
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
    taskStatus["Command"] = "GetStatus";
    taskStatus["Destination"] = destination;
    taskStatus["IsReady"] = m_telescopeState->isReady;
    taskStatus["Stage"] = m_telescopeState->stage;
    taskStatus["State"] = m_telescopeState->state;
    taskStatus["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    
    if (sequenceId != -1) {
        taskStatus["SequenceID"] = sequenceId;
        taskStatus["Source"] = "TaskController";
        taskStatus["Type"] = "Response";
        taskStatus["ErrorCode"] = 0;
        taskStatus["ErrorMessage"] = "";
        
        if (specificClient) {
            sendJsonMessage(specificClient, taskStatus);
        }
    } else {
        taskStatus["SequenceID"] = 4000 + (QDateTime::currentDateTime().toSecsSinceEpoch() % 1000);
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
    modelResponse["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
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
    filterResponse["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
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
    calResponse["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    calResponse["SequenceID"] = sequenceId;
    calResponse["Source"] = "FactoryCalibrationController";
    calResponse["Type"] = "Response";
    
    sendJsonMessage(wsConn, calResponse);
    
    // Also send notification
    QJsonObject calNotification;
    calNotification["Command"] = "GetStatus";
    calNotification["Destination"] = "All";
    calNotification["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    calNotification["IsCalibrated"] = true;
    calNotification["NumTimesCollimated"] = 2;
    calNotification["NumTimesHotSpotCentered"] = 2;
    calNotification["SequenceID"] = 4795;
    calNotification["Source"] = "FactoryCalibrationController";
    calNotification["Type"] = "Notification";
    
    QJsonArray completedPhases;
    completedPhases.append("UPDATE");
    completedPhases.append("HARDWARE_CALIBRATION");
    completedPhases.append("DARK_GENERATION");
    completedPhases.append("FLAT_GENERATION");
    completedPhases.append("FA_TEST");
    completedPhases.append("BATTERY");
    
    calNotification["CompletedPhases"] = completedPhases;
    calNotification["CurrentPhase"] = "IDLE";
    
    sendJsonMessage(wsConn, calNotification);
}