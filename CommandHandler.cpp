#include "CommandHandler.h"
#include "StellariumDSOOverlay.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonValue>
#include <QUuid>

CommandHandler::CommandHandler(TelescopeState *state, QObject *parent) 
    : QObject(parent), m_telescopeState(state) {
}

void CommandHandler::processCommand(const QJsonObject &obj, WebSocketConnection *wsConn) {
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    int sequenceId = obj["SequenceID"].toInt();
    QString source = obj["Source"].toString();
    QString type = obj["Type"].toString();
    
//     qDebug() << "Processing command:" << command << "to" << destination << "from" << source;
    
    // Handle different commands
    if (command == "RunInitialize") {
        handleRunInitialize(obj, wsConn, sequenceId, source, destination);
    } else if (command == "StartAlignment") {
        handleStartAlignment(obj, wsConn, sequenceId, source, destination);
    } else if (command == "AddAlignmentPoint") {
        handleAddAlignmentPoint(obj, wsConn, sequenceId, source, destination);
    } else if (command == "FinishAlignment") {
        handleFinishAlignment(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GotoRaDec") {
        handleGotoRaDec(obj, wsConn, sequenceId, source, destination);
    } else if (command == "Slew") {
        handleSlew(obj, wsConn, sequenceId, source, destination);
    } else if (command == "AbortAxisMovement") {
        handleAbortAxisMovement(obj, wsConn, sequenceId, source, destination);
    } else if (command == "StartTracking") {
        handleStartTracking(obj, wsConn, sequenceId, source, destination);
    } else if (command == "StopTracking") {
        handleStopTracking(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunImaging") {
        handleRunImaging(obj, wsConn, sequenceId, source, destination);
    } else if (command == "CancelImaging") {
        handleCancelImaging(obj, wsConn, sequenceId, source, destination);
    } else if (command == "MoveToPosition" && destination == "Focuser") {
        handleMoveToPosition(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetListOfAvailableDirectories" && destination == "ImageServer") {
        handleGetDirectoryList(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetDirectoryContents" && destination == "ImageServer") {
        handleGetDirectoryContents(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetCaptureParameters") {
        handleSetCaptureParameters(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetBacklash" && destination == "Focuser") {
        handleSetFocuserBacklash(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetMode" && destination == "DewHeater") {
        handleSetDewHeaterMode(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetSerialNumber" && destination == "FactoryCalibrationController") {
        handleGetSerialNumber(obj, wsConn, sequenceId, source, destination);
    } else if (command == "HasUpdateAvailable" && destination == "System") {
        handleHasUpdateAvailable(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetUpdateChannel" && destination == "System") {
        handleGetUpdateChannel(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetRegulatoryDomain" && destination == "Network") {
        handleSetRegulatoryDomain(obj, wsConn, sequenceId, source, destination);
    } else if (command == "HasInternetConnection" && destination == "Network") {
        handleHasInternetConnection(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetForceDirectConnect" && destination == "Network") {
        handleGetForceDirectConnect(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetCameraInfo" && destination == "Camera") {
        handleGetCameraInfo(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetSensors" && destination == "Environment") {
        handleGetSensors(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetBrightnessLevel" && destination == "LedRing") {
        handleGetBrightnessLevel(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetFocuserAdvancedSettings" && destination == "Focuser") {
        handleGetFocuserAdvancedSettings(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetMountConfig" && destination == "Mount") {
        handleGetMountConfig(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetPositionLimits" && destination == "Focuser") {
        handleGetPositionLimits(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetEnableManual" && destination == "LiveStream") {
        handleGetEnableManual(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetEnableManual" && destination == "LiveStream") {
        handleSetEnableManual(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetEnableAuto" && destination == "LiveStream") {
        handleSetEnableAuto(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunSampleCapture" && destination == "TaskController") {
        handleRunSampleCapture(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetFilter" && destination == "Camera") {
        handleGetFilter(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetDirectConnectPassword" && destination == "Network") {
        handleGetDirectConnectPassword(obj, wsConn, sequenceId, source, destination);
    }
    // === New commands from Origin.app v1.1.4 ===
    // Mount commands
    else if (command == "GotoAltAzm" && destination == "Mount") {
        handleGotoAltAzm(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GotoEnc" && destination == "Mount") {
        handleGotoEnc(obj, wsConn, sequenceId, source, destination);
    } else if (command == "StopAll" && destination == "Mount") {
        handleStopAll(obj, wsConn, sequenceId, source, destination);
    } else if (command == "DeleteAllAlignRefs" && destination == "Mount") {
        handleDeleteAllAlignRefs(obj, wsConn, sequenceId, source, destination);
    } else if (command == "EnablePec" && destination == "Mount") {
        handleEnablePec(obj, wsConn, sequenceId, source, destination);
    } else if (command == "EnableTracking" && destination == "Mount") {
        handleEnableTracking(obj, wsConn, sequenceId, source, destination);
    } else if (command == "IsTracking" && destination == "Mount") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        response["IsTracking"] = m_telescopeState->isTracking;
        sendJsonResponse(wsConn, response);
    } else if (command == "IsAligned" && destination == "Mount") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        response["IsAligned"] = m_telescopeState->isAligned;
        sendJsonResponse(wsConn, response);
    } else if (command == "IsGotoOver" && destination == "Mount") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        response["IsGotoOver"] = m_telescopeState->isGotoOver;
        sendJsonResponse(wsConn, response);
    } else if (command == "SetMountConfig" && destination == "Mount") {
        handleSetMountConfig(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "MoveRa" || command == "MoveDec") && destination == "Mount") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    } else if (command == "EnableCustomRate9" && destination == "Mount") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    } else if (command == "TrackMovingTarget" && destination == "Mount") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    } else if (command == "AddAlignRef" && destination == "Mount") {
        m_telescopeState->numAlignRefs++;
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // Camera commands
    else if (command == "SetCooling" && destination == "Camera") {
        handleSetCooling(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetCooling" && destination == "Camera") {
        handleGetCooling(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetSavePlateSolves" && destination == "Camera") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        response["SavePlateSolves"] = m_telescopeState->savePlateSolves;
        sendJsonResponse(wsConn, response);
    } else if (command == "SetSavePlateSolves" && destination == "Camera") {
        if (obj.contains("SavePlateSolves")) m_telescopeState->savePlateSolves = obj["SavePlateSolves"].toBool();
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    } else if (command == "SetFilter" && destination == "Camera") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // Environment commands
    else if (command == "SetFans" && destination == "Environment") {
        handleSetFans(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetFans" && destination == "Environment") {
        handleGetFans(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RecalibrateEnvironmentSensor" && destination == "Environment") {
        m_telescopeState->recalibrating = true;
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // LedRing commands
    else if (command == "SetBrightnessLevel" && destination == "LedRing") {
        handleSetBrightnessLevel(obj, wsConn, sequenceId, source, destination);
    }
    // ElPanel commands
    else if (destination == "ElPanel") {
        handleElPanelCommand(obj, wsConn, sequenceId, source, destination);
    }
    // OrientationSensor commands
    else if (command == "GetAccelerometer" && destination == "OrientationSensor") {
        handleGetAccelerometer(obj, wsConn, sequenceId, source, destination);
    } else if (command == "ZeroCalibrate" && destination == "OrientationSensor") {
        handleZeroCalibrate(obj, wsConn, sequenceId, source, destination);
    }
    // Network commands
    else if (command == "GetVisibleWiFi" && destination == "Network") {
        handleGetVisibleWiFi(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetWiFiNetworks" && destination == "Network") {
        handleGetWiFiNetworks(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "AddWiFiNetwork" || command == "DeleteWiFiNetwork" ||
                command == "ConnectToSpecificNetwork") && destination == "Network") {
        handleWiFiNetworkCommand(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "ForceDirectConnect" || command == "SetForceDirectConnect" ||
                command == "Get5GHzAccessPoint" || command == "5GHzAccessPoint" ||
                command == "ReconnectDirectConnect" || command == "ReconnectWiFi" ||
                command == "RemoveAllKnownNetworks" || command == "SetDeviceIsHost" ||
                command == "SetDirectConnectPassword") && destination == "Network") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // System commands
    else if (command == "GetVersionInfo" && destination == "System") {
        handleGetVersionInfo(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetLogs" && destination == "System") {
        handleGetLogs(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetAutomaticUpdates" && destination == "System") {
        handleGetAutomaticUpdates(obj, wsConn, sequenceId, source, destination);
    } else if (command == "SetAutomaticUpdates" && destination == "System") {
        handleSetAutomaticUpdates(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "Reboot" || command == "PowerDownOrigin" || command == "ShutDownCore") && destination == "System") {
        handleSystemPower(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "EnableCustomerMode" || command == "EnableFTPAccess" ||
                command == "SetUpdateChannel" || command == "Update" ||
                command == "RunFactoryReset" || command == "CriticalError") && destination == "System") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // TaskController commands
    else if (command == "RunCenterTarget" && destination == "TaskController") {
        handleRunCenterTarget(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunObservingList" && destination == "TaskController") {
        handleRunObservingList(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetObservingListInfo" && destination == "TaskController") {
        handleGetObservingListInfo(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "RunAutoFocus" || command == "RunInfinityAutoFocus" ||
                command == "RunTemperatureShiftAutoFocus" || command == "RunTerrestrialAutoFocus") && destination == "TaskController") {
        handleRunAutoFocus(obj, wsConn, sequenceId, source, destination);
    } else if (command == "CancelAutoFocus" && destination == "TaskController") {
        handleCancelAutoFocus(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunManualFocus" && destination == "TaskController") {
        handleRunManualFocus(obj, wsConn, sequenceId, source, destination);
    } else if (command == "PolarAlign" && destination == "TaskController") {
        handlePolarAlign(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunGenerateNewDarks" && destination == "TaskController") {
        handleRunGenerateNewDarks(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunGenerateNewFlat" && destination == "TaskController") {
        handleRunGenerateNewFlat(obj, wsConn, sequenceId, source, destination);
    } else if (command == "ImageNextTarget" && destination == "TaskController") {
        handleImageNextTarget(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunCaptureMosaic" && destination == "TaskController") {
        handleRunCaptureMosaic(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunComposeMosaic" && destination == "TaskController") {
        handleRunComposeMosaic(obj, wsConn, sequenceId, source, destination);
    } else if (command == "CopyDirectoryToFlashDrive" && destination == "TaskController") {
        handleCopyDirectoryToFlashDrive(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "CancelAutoCapture" || command == "CalibrateFocuser" ||
                command == "CaptureVideo" || command == "MeasureDarkCurrent" ||
                command == "RestoreDarksToFactory" || command == "RestoreFlatsToFactory" ||
                command == "RunAddReference" || command == "RecaptureImagingListObject" ||
                command == "RecaptureMosaicTile" || command == "CopyAllDirectoriesToFlashDrive" ||
                command == "SafeToRemoveFlashDrive" || command == "RunGenerateAmplitudeVsGain") && destination == "TaskController") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // ImageServer commands
    else if (destination == "ImageServer" && command != "GetListOfAvailableDirectories" && command != "GetDirectoryContents") {
        handleImageServerCommand(obj, wsConn, sequenceId, source, destination);
    }
    // Focuser commands (beyond existing handlers)
    else if ((command == "RunCalibration" || command == "HaltCalibration" ||
              command == "IsCalibrationComplete" || command == "IsMoveToOver" ||
              command == "GetVelocity" || command == "GetPosition" ||
              command == "AutoFocusAfterGoto" || command == "AutoFocusOnTemperatureChange" ||
              command == "MoveTo") && destination == "Focuser") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        if (command == "IsCalibrationComplete") response["IsCalibrationComplete"] = m_telescopeState->isCalibrationComplete;
        if (command == "IsMoveToOver") response["IsMoveToOver"] = m_telescopeState->isMoveToOver;
        if (command == "GetVelocity") response["Velocity"] = m_telescopeState->velocity;
        if (command == "GetPosition") response["Position"] = m_telescopeState->position;
        if (command == "MoveTo" && obj.contains("Position")) m_telescopeState->position = obj["Position"].toInt();
        sendJsonResponse(wsConn, response);
    }
    // Optics commands
    else if (destination == "Optics") {
        handleGetOpticsConfig(obj, wsConn, sequenceId, source, destination);
    }
    // Mosaic commands
    else if (destination == "Mosaic") {
        handleGetAvailableMosaics(obj, wsConn, sequenceId, source, destination);
    }
    // HostController commands
    else if (destination == "HostController") {
        handleHostControllerCommand(obj, wsConn, sequenceId, source, destination);
    }
    // LiveStream commands (beyond existing handlers)
    else if (destination == "LiveStream" && command != "GetEnableManual" &&
             command != "SetEnableManual" && command != "SetEnableAuto") {
        handleLiveStreamCommand(obj, wsConn, sequenceId, source, destination);
    }
    // DewHeater commands (beyond existing handler)
    else if ((command == "EnableAuto" || command == "EnableManual") && destination == "DewHeater") {
        if (command == "EnableAuto") m_telescopeState->mode = "Auto";
        else m_telescopeState->mode = "Manual";
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // Disk commands
    else if ((command == "DeleteAllImageDirectories" || command == "DeleteImageDirectory") && destination == "Disk") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // FactoryCalibrationController commands
    else if (destination == "FactoryCalibrationController" && command != "GetSerialNumber") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // Autoguider commands
    else if (destination == "Autoguider") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    // LampController commands
    else if (destination == "LampController") {
        handleElPanelCommand(obj, wsConn, sequenceId, source, destination);
    }
    // Commands missing from earlier dispatch — added for App-init handshake
    // (See Origin protocol capture analysis 2026-05-14.)
    else if (command == "GetModel" && destination == "System") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        QJsonArray devices;
        for (const auto& d : { "Autoguider", "Camera", "DewHeater", "Disk",
                               "Environment", "FactoryCalibrationController",
                               "FlatPanel", "Focuser", "ImageServer",
                               "LampController", "LedRing", "LiveStream",
                               "Mosaic", "Mount", "Network", "Optics",
                               "OrientationSensor", "System", "TaskController" })
            devices.append(d);
        response["Devices"] = devices;
        sendJsonResponse(wsConn, response);
    } else if (command == "GetVersion" && destination == "System") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        response["Number"] = "1.3.5330";
        response["Version"] = "1.3.5330";
        sendJsonResponse(wsConn, response);
    } else if (command == "HaltTasks" && destination == "TaskController") {
        // Pretend to abort whatever is running.
        const bool wasImaging = m_telescopeState->isImaging;
        const QString sessionDir = m_telescopeState->imagingSessionDir;
        const QString sessionUuid = m_telescopeState->imagingUuid;
        m_telescopeState->state = "IDLE";
        m_telescopeState->stage = "IN_PROGRESS";
        m_telescopeState->isImaging = false;
        m_telescopeState->exposedTime = 0.0;
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
        // If imaging was actually active, signal the simulator to render and
        // persist a FinalStackedMaster.tiff to disk in the session directory.
        if (wasImaging && !sessionDir.isEmpty())
            emit observationEnded(sessionDir, sessionUuid);
    } else if (command == "GetStretch" && destination == "ImageServer") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        response["Background"] = 0.5;
        response["Strength"] = 0.75;
        sendJsonResponse(wsConn, response);
    } else if (command == "SetStretch" && destination == "ImageServer") {
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
    else {
        // Default response for unimplemented commands
        QJsonObject response = makeResponse(command, sequenceId, source, destination);
        sendJsonResponse(wsConn, response);
    }
}

// In CommandHandler.cpp

void CommandHandler::handleRunInitialize(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Update telescope state
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    
    if (obj.contains("Date")) m_telescopeState->dateTime.setDate(QDate::fromString(obj["Date"].toString(), "dd MM yyyy"));
    if (obj.contains("Time")) m_telescopeState->dateTime.setTime(QTime::fromString(obj["Time"].toString(), "hh:mm:ss"));
    if (obj.contains("Latitude")) m_telescopeState->latitude = obj["Latitude"].toDouble();
    if (obj.contains("Longitude")) m_telescopeState->longitude = obj["Longitude"].toDouble();
    if (obj.contains("TimeZone")) m_telescopeState->timeZone = obj["TimeZone"].toString();
    
    // Set fake initialization flag if specified
    if (obj.contains("FakeInitialize")) {
        m_telescopeState->isFakeInitialized = obj["FakeInitialize"].toBool();
    } else {
        m_telescopeState->isFakeInitialized = false;
    }
    
    // Start the initialization process
    m_telescopeState->isInitializing = true;
    m_telescopeState->initializationProgress = 0;
    m_telescopeState->state = "INITIALIZING";
    m_telescopeState->stage = "IN_PROGRESS";
    m_telescopeState->isReady = false;
    
    // Reset initialization info
    m_telescopeState->initInfo.numPoints = 0;
    m_telescopeState->initInfo.positionOfFocus = -1;
    m_telescopeState->initInfo.numPointsRemaining = 2;
    m_telescopeState->initInfo.percentageComplete = 0;
    m_telescopeState->initInfo.currentStep = "NONE";
    m_telescopeState->focusInfo.position = 0;
    m_telescopeState->focusInfo.percentageComplete = 0;
    m_telescopeState->isAligned = true;
    
    // Send immediate response
    QJsonObject response;
    response["Command"] = "RunInitialize";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
    
    // Emit signal to start the initialization simulation
    emit initializationStarted(obj.contains("FakeInitialize") && obj["FakeInitialize"].toBool());
}

void CommandHandler::handleStartAlignment(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isAligned = false;
    m_telescopeState->numAlignRefs = 0;
    
    QJsonObject response;
    response["Command"] = "StartAlignment";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleAddAlignmentPoint(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->numAlignRefs++;
    
    QJsonObject response;
    response["Command"] = "AddAlignmentPoint";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleFinishAlignment(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (m_telescopeState->numAlignRefs >= 1) {
        m_telescopeState->isAligned = true;
    }
    
    QJsonObject response;
    response["Command"] = "FinishAlignment";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGotoRaDec(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (m_telescopeState->isAligned) {
        m_telescopeState->isGotoOver = false;
        m_telescopeState->isSlewing = true;

        // ADD THESE DEBUG LINES:
        double received_ra = obj["Ra"].toDouble();
        double received_dec = obj["Dec"].toDouble();
//         // qDebug() << "*** GOTO COMMAND RECEIVED ***";
        qDebug() << "Raw RA from JSON:" << received_ra << "radians";
        qDebug() << "Raw Dec from JSON:" << received_dec << "radians";
        qDebug() << "RA in hours:" << (received_ra * 12.0 / M_PI);
        qDebug() << "Dec in degrees:" << (received_dec * 180.0 / M_PI);
        
        m_telescopeState->targetRa = received_ra;
        m_telescopeState->targetDec = received_dec;
        
        qDebug() << "Stored targetRa:" << m_telescopeState->targetRa;
        qDebug() << "Stored targetDec:" << m_telescopeState->targetDec;	
        
        emit slewStarted();
        
        QJsonObject response;
        response["Command"] = "GotoRaDec";
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonResponse(wsConn, response);
    } else {
        QJsonObject response;
        response["Command"] = "GotoRaDec";
        response["Destination"] = source;
        response["ErrorCode"] = 1;
        response["ErrorMessage"] = "Telescope not aligned";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonResponse(wsConn, response);
    }
}

void CommandHandler::handleSlew(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Extract slew rates from command (may be fractional deg/s)
    double altRate = obj["AltRate"].toDouble();
    double azmRate = obj["AzmRate"].toDouble();

    // If both rates are zero, we're stopping
    if (altRate == 0 && azmRate == 0) {
        m_telescopeState->isManualSlewing = false;
        qDebug() << QString("Slew STOPPED  Alt=%1 deg  Azm=%2 deg  RA=%3 rad  Dec=%4 rad")
                    .arg(m_telescopeState->altitude * 180.0 / M_PI, 0, 'f', 2)
                    .arg(m_telescopeState->azimuth * 180.0 / M_PI, 0, 'f', 2)
                    .arg(m_telescopeState->ra, 0, 'f', 6)
                    .arg(m_telescopeState->dec, 0, 'f', 6);
    } else {
        m_telescopeState->isManualSlewing = true;
        qDebug() << QString("Slew STARTED  AltRate=%1 deg/s  AzmRate=%2 deg/s  from Alt=%3 deg Azm=%4 deg")
                    .arg(altRate, 0, 'f', 3)
                    .arg(azmRate, 0, 'f', 3)
                    .arg(m_telescopeState->altitude * 180.0 / M_PI, 0, 'f', 2)
                    .arg(m_telescopeState->azimuth * 180.0 / M_PI, 0, 'f', 2);
    }

    // Update telescope state based on rates
    m_telescopeState->slewAltRate = altRate;
    m_telescopeState->slewAzmRate = azmRate;

    if (altRate == 0 && azmRate == 0)
        emit manualSlewStopped();
    else
        emit manualSlewStarted();

    // Send response
    QJsonObject response;
    response["Command"] = "Slew";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleAbortAxisMovement(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isGotoOver = true;
    m_telescopeState->isSlewing = false;
    
    QJsonObject response;
    response["Command"] = "AbortAxisMovement";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleStartTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isTracking = true;
    
    QJsonObject response;
    response["Command"] = "StartTracking";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleStopTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isTracking = false;
    
    QJsonObject response;
    response["Command"] = "StopTracking";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Honour the App's requested exposure if present; default 5s.
    double exposureTime = obj["Exposure"].toDouble(obj["ExposureTime"].toDouble(5.0));
    if (obj.contains("ISO"))      m_telescopeState->iso     = obj["ISO"].toInt();
    if (obj.contains("Binning"))  m_telescopeState->binning = obj["Binning"].toInt();
    m_telescopeState->exposure = exposureTime;
    m_telescopeState->exposedTime = 0.0;
    m_telescopeState->isImaging = true;
    m_telescopeState->imagingTimeLeft = qMax(1, static_cast<int>(exposureTime));
    m_telescopeState->state = "IMAGING_OBJECT";

    // Session bookkeeping for STACKED_MASTER notifications, matching the
    // real Origin's "<Object>_<YYYY-MM-DD_HH-MM-SS>" directory + UUID per
    // RunImaging call. Reset stack depth.
    m_telescopeState->imagingObjectName = obj["Name"].toString();

    // If the App didn't supply a name, look up the largest DSO whose
    // footprint overlaps the current pointing's FOV — much friendlier than
    // a string of "Untitled_<timestamp>" folders piling up. Halfdiag for
    // the Origin's sensor (3056x2048 @ 1.4777"/px) is ~0.754°.
    QString resolvedName = m_telescopeState->imagingObjectName;
    if (resolvedName.isEmpty() && m_dsoOverlay != nullptr) {
        constexpr double kHalfDiagDeg = 0.754;
        const double raDeg  = m_telescopeState->ra  * 180.0 / M_PI;
        const double decDeg = m_telescopeState->dec * 180.0 / M_PI;
        const QString dso = m_dsoOverlay->largestVisible(raDeg, decDeg, kHalfDiagDeg);
        if (!dso.isEmpty()) {
            resolvedName = dso;
            m_telescopeState->imagingObjectName = dso;
            qInfo() << "RunImaging: no Name supplied, using largest DSO in FOV:" << dso;
        }
    }

    const QString safeName = resolvedName.isEmpty()
                                ? QStringLiteral("Untitled")
                                : QString(resolvedName).replace(' ', '_');
    m_telescopeState->imagingSessionDir =
        safeName + "_" +
        QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
    m_telescopeState->imagingUuid = obj.contains("Uuid")
                                       ? obj["Uuid"].toString()
                                       : QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_telescopeState->stackDepth  = 0;
    if (obj.contains("Duration"))
        m_telescopeState->imagingTotalSeconds = obj["Duration"].toDouble();
    else if (obj.contains("TotalTime"))
        m_telescopeState->imagingTotalSeconds = obj["TotalTime"].toDouble();
    else
        m_telescopeState->imagingTotalSeconds = 600.0;
    m_telescopeState->imagingStartEpochMs =
        QDateTime::currentDateTime().toMSecsSinceEpoch();

    emit imagingStarted();
    
    QJsonObject response;
    response["Command"] = "RunImaging";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleCancelImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isImaging = false;
    m_telescopeState->imagingTimeLeft = 0;
    m_telescopeState->exposedTime = 0.0;
    m_telescopeState->state = "IDLE";
    
    QJsonObject response;
    response["Command"] = "CancelImaging";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleMoveToPosition(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    int targetPosition = obj["Position"].toInt();
    m_telescopeState->position = targetPosition;
    
    QJsonObject response;
    response["Command"] = "MoveToPosition";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetDirectoryList(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetListOfAvailableDirectories";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = 0;
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    // Live-scan the configured astroBaseDir so the App sees real captured
    // sessions. Fall back to the hardcoded list only if the path is missing.
    QJsonArray dirList;
    QDir astroDir(m_telescopeState->astroBaseDir);
    if (astroDir.exists()) {
        const QStringList subs = astroDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot,
                                                     QDir::Name);
        for (const QString& d : subs) dirList.append(d);
    }
    if (dirList.isEmpty()) {
        for (const QString& d : m_telescopeState->astrophotographyDirs)
            dirList.append(d);
    }
    response["DirectoryList"] = dirList;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetDirectoryContents(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString dir = obj["Directory"].toString();
    
    QJsonObject response;
    response["Command"] = "GetDirectoryContents";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = 0;
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    QJsonArray fileList;
    fileList.append("frame_1.jpg");
    fileList.append("frame_2.jpg");
    fileList.append("frame_3.jpg");
    fileList.append("FinalStackedMaster.tiff");
    response["FileList"] = fileList;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetCaptureParameters(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Update camera parameters from the command
    if (obj.contains("Exposure")) m_telescopeState->exposure = obj["Exposure"].toDouble();
    if (obj.contains("ISO")) m_telescopeState->iso = obj["ISO"].toInt();
    if (obj.contains("Binning")) m_telescopeState->binning = obj["Binning"].toInt();
    if (obj.contains("Offset")) m_telescopeState->offset = obj["Offset"].toInt();
    if (obj.contains("ColorRBalance")) m_telescopeState->colorRBalance = obj["ColorRBalance"].toDouble();
    if (obj.contains("ColorGBalance")) m_telescopeState->colorGBalance = obj["ColorGBalance"].toDouble();
    if (obj.contains("ColorBBalance")) m_telescopeState->colorBBalance = obj["ColorBBalance"].toDouble();
    
    QJsonObject response;
    response["Command"] = "SetCaptureParameters";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetFocuserBacklash(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (obj.contains("Backlash")) {
        m_telescopeState->backlash = obj["Backlash"].toInt();
    }
    
    QJsonObject response;
    response["Command"] = "SetBacklash";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetDewHeaterMode(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (obj.contains("Mode")) {
        m_telescopeState->mode = obj["Mode"].toString();
    }
    if (obj.contains("Aggression")) {
        m_telescopeState->aggression = obj["Aggression"].toInt();
    }
    if (obj.contains("ManualPowerLevel")) {
        m_telescopeState->manualPowerLevel = obj["ManualPowerLevel"].toDouble();
    }
    
    QJsonObject response;
    response["Command"] = "SetMode";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}


void CommandHandler::handleGetSerialNumber(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetSerialNumber";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["SerialNumber"] = "OTU140020"; // Example serial number
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleHasUpdateAvailable(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "HasUpdateAvailable";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["Available"] = false;
    response["Version"] = "";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetUpdateChannel(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetUpdateChannel";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["Channel"] = "Release";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetRegulatoryDomain(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString countryCode = obj["CountryCode"].toString();
    m_telescopeState->countryCode = countryCode;
    
    QJsonObject response;
    response["Command"] = "SetRegulatoryDomain";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleHasInternetConnection(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "HasInternetConnection";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["Connected"] = true;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetForceDirectConnect(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetForceDirectConnect";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["ForceDirectConnect"] = false;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetCameraInfo(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetCameraInfo";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["ModelName"] = "Origin Camera";
    response["SensorWidth"] = 14.8;
    response["SensorHeight"] = 11.1;
    response["PixelSize"] = 4.63;
    response["EffectiveFocalLength"] = 700;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetSensors(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetSensors";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    
    QJsonArray sensors;
    sensors.append("AMBIENT_TEMPERATURE");
    sensors.append("HUMIDITY");
    sensors.append("DEW_POINT");
    sensors.append("FRONT_CELL_TEMPERATURE");
    sensors.append("CPU_TEMPERATURE");
    sensors.append("CAMERA_TEMPERATURE");
    response["Sensors"] = sensors;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetBrightnessLevel(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetBrightnessLevel";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["Level"] = 50;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetFocuserAdvancedSettings(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetFocuserAdvancedSettings";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["BacklashSteps"] = 255;
    response["DefaultSpeed"] = 250;
    response["DefaultAcceleration"] = 800;
    response["DirectionToggleDelayMs"] = 500;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetMountConfig(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetMountConfig";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["MaximumSpeed"] = 3.0;
    response["SlewSettleTime"] = 1.0;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetPositionLimits(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetPositionLimits";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["MaximumPosition"] = 40000;
    response["MinimumPosition"] = 0;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetEnableManual(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetEnableManual";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["EnableManual"] = true;
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetFilter(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetFilter";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["Filter"] = "Clear";
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetDirectConnectPassword(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = "GetDirectConnectPassword";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    response["Password"] = "celestron"; // Default password
    
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetEnableManual(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isManualMode = true;

    QJsonObject response;
    response["Command"] = "SetEnableManual";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetEnableAuto(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isManualMode = false;

    QJsonObject response;
    response["Command"] = "SetEnableAuto";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunSampleCapture(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Extract capture parameters
    double exposureTime = obj["ExposureTime"].toDouble(5.0);
    int iso = obj["ISO"].toInt(800);
    int binning = obj["Binning"].toInt(1);

    m_telescopeState->exposure = exposureTime;
    m_telescopeState->iso = iso;
    m_telescopeState->binning = binning;
    m_telescopeState->state = "SAMPLE_CAPTURE";
    m_telescopeState->stage = "IN_PROGRESS";

    // Send immediate response
    QJsonObject response;
    response["Command"] = "RunSampleCapture";
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    sendJsonResponse(wsConn, response);

    // Trigger snapshot generation after simulated exposure delay
    emit snapshotRequested(exposureTime, iso, binning);
}

void CommandHandler::sendJsonResponse(WebSocketConnection *wsConn, const QJsonObject &response) {
    QJsonDocument doc(response);
    QString message = doc.toJson();
    wsConn->sendTextMessage(message);
}

QJsonObject CommandHandler::makeResponse(const QString &command, int sequenceId, const QString &source, const QString &destination) {
    QJsonObject response;
    response["Command"] = command;
    response["Destination"] = source;
    response["ErrorCode"] = 0;
    response["ErrorMessage"] = "";
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    return response;
}

// ==================== Mount commands ====================

void CommandHandler::handleGotoAltAzm(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    double alt = obj["Alt"].toDouble();
    double azm = obj["Azm"].toDouble();

    qDebug() << QString("GotoAltAzm: Alt=%1 rad (%2 deg) Azm=%3 rad (%4 deg)")
                .arg(alt, 0, 'f', 6).arg(alt * 180.0 / M_PI, 0, 'f', 2)
                .arg(azm, 0, 'f', 6).arg(azm * 180.0 / M_PI, 0, 'f', 2);

    m_telescopeState->isGotoOver = false;
    m_telescopeState->isAltAzSlewing = true;
    m_telescopeState->targetAltitude = alt;
    m_telescopeState->targetAzimuth = azm;

    emit altAzSlewStarted();

    QJsonObject response = makeResponse("GotoAltAzm", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGotoEnc(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    double enc0 = obj["Enc0"].toDouble();
    double enc1 = obj["Enc1"].toDouble();

    m_telescopeState->enc0 = enc0;
    m_telescopeState->enc1 = enc1;
    m_telescopeState->isGotoOver = false;
    m_telescopeState->isAltAzSlewing = true;

    // Treat encoder values as approximate alt/az in radians
    m_telescopeState->targetAltitude = enc0;
    m_telescopeState->targetAzimuth = enc1;

    emit altAzSlewStarted();

    QJsonObject response = makeResponse("GotoEnc", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleStopAll(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    qDebug() << "StopAll: halting all motion";

    m_telescopeState->isSlewing = false;
    m_telescopeState->isAltAzSlewing = false;
    m_telescopeState->isManualSlewing = false;
    m_telescopeState->isGotoOver = true;
    m_telescopeState->slewAltRate = 0;
    m_telescopeState->slewAzmRate = 0;

    emit allStopped();

    QJsonObject response = makeResponse("StopAll", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleDeleteAllAlignRefs(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    m_telescopeState->numAlignRefs = 0;
    m_telescopeState->isAligned = false;

    QJsonObject response = makeResponse("DeleteAllAlignRefs", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleEnablePec(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->pecEnabled = obj["Enabled"].toBool();

    QJsonObject response = makeResponse("EnablePec", sequenceId, source, destination);
    response["Enabled"] = m_telescopeState->pecEnabled;
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleEnableTracking(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isTracking = obj["Enabled"].toBool();

    QJsonObject response = makeResponse("EnableTracking", sequenceId, source, destination);
    response["IsTracking"] = m_telescopeState->isTracking;
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetMountConfig(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Accept and acknowledge any config values
    QJsonObject response = makeResponse("SetMountConfig", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

// ==================== Camera commands ====================

void CommandHandler::handleSetCooling(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (obj.contains("Temperature")) m_telescopeState->coolingTargetTemp = obj["Temperature"].toDouble();
    if (obj.contains("Enabled")) m_telescopeState->coolingEnabled = obj["Enabled"].toBool();

    QJsonObject response = makeResponse("SetCooling", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetCooling(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetCooling", sequenceId, source, destination);
    response["Temperature"] = m_telescopeState->coolingTargetTemp;
    response["Enabled"] = m_telescopeState->coolingEnabled;
    response["CameraTemperature"] = m_telescopeState->cameraTemperature;
    sendJsonResponse(wsConn, response);
}

// ==================== Environment commands ====================

void CommandHandler::handleSetFans(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (obj.contains("CpuFanOn")) m_telescopeState->cpuFanOn = obj["CpuFanOn"].toBool();
    if (obj.contains("OtaFanOn")) m_telescopeState->otaFanOn = obj["OtaFanOn"].toBool();

    QJsonObject response = makeResponse("SetFans", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetFans(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetFans", sequenceId, source, destination);
    response["CpuFanOn"] = m_telescopeState->cpuFanOn;
    response["OtaFanOn"] = m_telescopeState->otaFanOn;
    sendJsonResponse(wsConn, response);
}

// ==================== LedRing commands ====================

void CommandHandler::handleSetBrightnessLevel(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (obj.contains("Level")) m_telescopeState->ledBrightnessLevel = obj["Level"].toInt();

    QJsonObject response = makeResponse("SetBrightnessLevel", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

// ==================== ElPanel / LampController commands ====================

void CommandHandler::handleElPanelCommand(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();

    if (command == "TurnOnLight") {
        m_telescopeState->lightOn = true;
    } else if (command == "TurnOffLight") {
        m_telescopeState->lightOn = false;
    } else if (command == "SetLightLevel") {
        if (obj.contains("Level")) m_telescopeState->lightLevel = obj["Level"].toInt();
    }

    QJsonObject response = makeResponse(command, sequenceId, source, destination);
    if (command == "IsLightOn") {
        response["IsLightOn"] = m_telescopeState->lightOn;
    } else if (command == "GetLightLevel") {
        response["Level"] = m_telescopeState->lightLevel;
    }
    sendJsonResponse(wsConn, response);
}

// ==================== OrientationSensor commands ====================

void CommandHandler::handleGetAccelerometer(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetAccelerometer", sequenceId, source, destination);
    // Return accelerometer values based on current altitude
    double altDeg = m_telescopeState->altitude * 180.0 / M_PI;
    response["X"] = sin(m_telescopeState->altitude);
    response["Y"] = 0.0;
    response["Z"] = cos(m_telescopeState->altitude);
    response["Altitude"] = altDeg;
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleZeroCalibrate(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("ZeroCalibrate", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

// ==================== Network commands ====================

void CommandHandler::handleGetVisibleWiFi(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetVisibleWiFi", sequenceId, source, destination);

    QJsonArray networks;
    QJsonObject net1; net1["SSID"] = "HomeNetwork"; net1["Signal"] = -45; net1["Security"] = "WPA2"; networks.append(net1);
    QJsonObject net2; net2["SSID"] = "Observatory_WiFi"; net2["Signal"] = -52; net2["Security"] = "WPA2"; networks.append(net2);
    QJsonObject net3; net3["SSID"] = "Neighbor5G"; net3["Signal"] = -68; net3["Security"] = "WPA3"; networks.append(net3);
    response["Networks"] = networks;

    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetWiFiNetworks(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetWiFiNetworks", sequenceId, source, destination);

    QJsonArray networks;
    QJsonObject net1; net1["SSID"] = "HomeNetwork"; net1["Connected"] = true; networks.append(net1);
    response["Networks"] = networks;

    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleWiFiNetworkCommand(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    QJsonObject response = makeResponse(command, sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

// ==================== System commands ====================

void CommandHandler::handleGetVersionInfo(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetVersionInfo", sequenceId, source, destination);
    response["Version"] = m_telescopeState->versionString;
    response["Number"] = m_telescopeState->versionNumber;
    response["BuildDate"] = "09-04-2024 18:19";
    response["Model"] = "Origin";
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetLogs(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetLogs", sequenceId, source, destination);
    QJsonArray logs;
    logs.append("2024-04-09 18:19:00 System started");
    logs.append("2024-04-09 18:19:01 Mount initialized");
    logs.append("2024-04-09 18:19:02 Camera connected");
    response["Logs"] = logs;
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetAutomaticUpdates(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetAutomaticUpdates", sequenceId, source, destination);
    response["Enabled"] = m_telescopeState->automaticUpdates;
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSetAutomaticUpdates(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    if (obj.contains("Enabled")) m_telescopeState->automaticUpdates = obj["Enabled"].toBool();
    QJsonObject response = makeResponse("SetAutomaticUpdates", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleSystemPower(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    qDebug() << "System power command:" << command;
    QJsonObject response = makeResponse(command, sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

// ==================== TaskController commands ====================

void CommandHandler::handleRunCenterTarget(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    double ra = obj["Ra"].toDouble();
    double dec = obj["Dec"].toDouble();
    qDebug() << QString("RunCenterTarget: RA=%1 rad Dec=%2 rad").arg(ra, 0, 'f', 6).arg(dec, 0, 'f', 6);

    m_telescopeState->isGotoOver = false;
    m_telescopeState->isSlewing = true;
    m_telescopeState->targetRa = ra;
    m_telescopeState->targetDec = dec;
    m_telescopeState->state = "CENTERING_TARGET";

    emit slewStarted();

    QJsonObject response = makeResponse("RunCenterTarget", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunObservingList(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString listUuid = obj["ListUuid"].toString();
    qDebug() << "RunObservingList:" << listUuid;

    m_telescopeState->state = "IMAGING_OBJECT";
    m_telescopeState->stage = "IN_PROGRESS";
    if (m_telescopeState->imagingStartEpochMs == 0)
        m_telescopeState->imagingStartEpochMs =
            QDateTime::currentDateTime().toMSecsSinceEpoch();

    QJsonObject response = makeResponse("RunObservingList", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetObservingListInfo(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetObservingListInfo", sequenceId, source, destination);

    QJsonArray targets;
    QJsonObject t1;
    t1["Name"] = "M31 Andromeda Galaxy";
    t1["Ra"] = 0.1847;
    t1["Dec"] = 0.7224;
    t1["ExposureTime"] = 300;
    t1["NumExposures"] = 20;
    targets.append(t1);
    response["Targets"] = targets;
    response["TotalExposureTime"] = 6000;

    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunAutoFocus(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    qDebug() << command << "started";
    m_telescopeState->state = "FOCUSING";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse(command, sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleCancelAutoFocus(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    m_telescopeState->state = "IDLE";
    m_telescopeState->stage = "COMPLETE";

    QJsonObject response = makeResponse("CancelAutoFocus", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunManualFocus(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    int range = obj["Range"].toInt();
    int startPos = obj["StartPosition"].toInt();
    int stepSize = obj["StepSize"].toInt();
    Q_UNUSED(range); Q_UNUSED(startPos); Q_UNUSED(stepSize);

    m_telescopeState->state = "FOCUSING";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse("RunManualFocus", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handlePolarAlign(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    qDebug() << "PolarAlign started";
    m_telescopeState->state = "POLAR_ALIGNING";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse("PolarAlign", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunGenerateNewDarks(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    int maxDarks = obj["MaxDarks"].toInt();
    int numDarks = obj["NumDarks"].toInt();
    Q_UNUSED(maxDarks); Q_UNUSED(numDarks);
    qDebug() << "RunGenerateNewDarks:" << numDarks << "of" << maxDarks;

    m_telescopeState->state = "GENERATING_DARKS";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse("RunGenerateNewDarks", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunGenerateNewFlat(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    qDebug() << "RunGenerateNewFlat started";

    m_telescopeState->state = "GENERATING_FLAT";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse("RunGenerateNewFlat", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleImageNextTarget(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("ImageNextTarget", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunCaptureMosaic(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    int offsetX = obj["OffsetX"].toInt();
    int offsetY = obj["OffsetY"].toInt();
    int nbrSubs = obj["NbrSubs"].toInt();
    Q_UNUSED(offsetX); Q_UNUSED(offsetY); Q_UNUSED(nbrSubs);

    m_telescopeState->state = "CAPTURING_MOSAIC";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse("RunCaptureMosaic", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunComposeMosaic(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    m_telescopeState->state = "COMPOSING_MOSAIC";
    m_telescopeState->stage = "IN_PROGRESS";

    QJsonObject response = makeResponse("RunComposeMosaic", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleGetAvailableImagingLists(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    Q_UNUSED(obj);
    QJsonObject response = makeResponse("GetAvailableImagingLists", sequenceId, source, destination);

    QJsonArray lists;
    QJsonObject list1;
    list1["Uuid"] = "a1b2c3d4-e5f6-7890-abcd-ef1234567890";
    list1["Name"] = "Deep Sky Objects";
    list1["NumTargets"] = 5;
    lists.append(list1);
    response["ImagingLists"] = lists;

    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleCopyDirectoryToFlashDrive(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString dirName = obj["DirName"].toString();
    qDebug() << "CopyDirectoryToFlashDrive:" << dirName;

    QJsonObject response = makeResponse("CopyDirectoryToFlashDrive", sequenceId, source, destination);
    sendJsonResponse(wsConn, response);
}

// ==================== Optics commands ====================

void CommandHandler::handleGetOpticsConfig(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    QJsonObject response = makeResponse(command, sequenceId, source, destination);

    if (command == "GetConfiguration") {
        response["Model"] = m_telescopeState->opticsModel;
        response["FocalLength"] = m_telescopeState->focalLength;
        response["Aperture"] = m_telescopeState->aperture;
    } else if (command == "GetSupportedModels") {
        QJsonArray models;
        models.append("Origin");
        response["Models"] = models;
    } else if (command == "SetConfiguration") {
        if (obj.contains("Model")) m_telescopeState->opticsModel = obj["Model"].toString();
    }

    sendJsonResponse(wsConn, response);
}

// ==================== Mosaic commands ====================

void CommandHandler::handleGetAvailableMosaics(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    QJsonObject response = makeResponse(command, sequenceId, source, destination);

    if (command == "GetAvailableMosaics") {
        QJsonArray mosaics;
        response["Mosaics"] = mosaics;
    } else if (command == "GetMosaicMetadata" || command == "GetMosaicTiles") {
        response["Tiles"] = QJsonArray();
    }
    // DeleteAllMosaics, DeleteMosaic — just acknowledge

    sendJsonResponse(wsConn, response);
}

// ==================== HostController commands ====================

void CommandHandler::handleHostControllerCommand(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    QJsonObject response = makeResponse(command, sequenceId, source, destination);

    if (command == "GetDeviceIsHost") {
        response["DeviceIsHost"] = m_telescopeState->deviceIsHost;
    } else if (command == "GetPin") {
        response["Pin"] = m_telescopeState->hostPin;
    } else if (command == "SetPin") {
        if (obj.contains("Pin")) m_telescopeState->hostPin = obj["Pin"].toString();
    } else if (command == "ResetPin") {
        m_telescopeState->hostPin = "1234";
    }

    sendJsonResponse(wsConn, response);
}

// ==================== LiveStream commands ====================

void CommandHandler::handleLiveStreamCommand(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    QJsonObject response = makeResponse(command, sequenceId, source, destination);

    if (command == "CloseLiveStream") {
        // Acknowledge
    } else if (command == "GetDisableLiveStream") {
        response["Disabled"] = m_telescopeState->liveStreamDisabled;
    } else if (command == "SetDisableLiveStream") {
        if (obj.contains("Disabled")) m_telescopeState->liveStreamDisabled = obj["Disabled"].toBool();
    }

    sendJsonResponse(wsConn, response);
}

// ==================== ImageServer commands ====================

void CommandHandler::handleImageServerCommand(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    QString command = obj["Command"].toString();
    QJsonObject response = makeResponse(command, sequenceId, source, destination);

    if (command == "GetAvailableImagingLists") {
        QJsonArray lists;
        response["ImagingLists"] = lists;
    } else if (command == "GetFinalStackedMaster") {
        // The App fetches the .tiff at this path to display the final stack.
        // Our astro HTTP handler falls back to m_imageDataStack when no
        // on-disk file exists, so the App receives the most recent stacked
        // master TIFF (DSO-bearing if --dso=... was used).
        const QString dir = m_telescopeState->imagingSessionDir.isEmpty()
                            ? QString("Origin")
                            : m_telescopeState->imagingSessionDir;
        const QString fileLocation =
            QString("Images/Astrophotography/%1/FinalStackedMaster.tiff").arg(dir);
        response["FileLocation"] = fileLocation;
        // Echo back the ImageUuid so the App can correlate the response with
        // its outstanding request. Real-telescope responses use ExpiredAt=0
        // for static replies (a current-time timestamp reads as "already
        // expired" and the App ignores the FileLocation).
        const QString imageUuid = obj.value("ImageUuid").toString();
        if (!imageUuid.isEmpty())
            response["ImageUuid"] = imageUuid;
        response["ExpiredAt"] = 0;
        // The App's actual download trigger is a NewImageReady notification,
        // not this synchronous response. Send the response first (below) then
        // emit the signal so the simulator can schedule a delayed render +
        // notification — that's what kicks off the App's HTTP fetch.
        sendJsonResponse(wsConn, response);
        emit finalStackedMasterRequested(fileLocation, imageUuid);
        return;
    } else if (command == "GetStackedMasterFromDirectory") {
        // Real-telescope response shape (verified against filemanager.pcapng):
        // just { FileExists: true|false, ExpiredAt: 0, ErrorCode/Message }.
        // No FileLocation, no Uuid, no ObjectName. The App downloads directly
        // via HTTP GET on the canonical path
        //   Images/Astrophotography/<DirName>/FinalStackedMaster.tiff
        // — no notification trigger needed.
        const QString dirName = obj.value("DirectoryName").toString();
        bool fileExists = false;
        if (!dirName.isEmpty()) {
            const QString tiffPath = QDir(m_telescopeState->astroBaseDir)
                                       .filePath(dirName + "/FinalStackedMaster.tiff");
            fileExists = QFile::exists(tiffPath);
        }
        response["FileExists"] = fileExists;
        response["ExpiredAt"]  = 0;
    } else if (command == "GetAvailableStackedMasters") {
        // Real telescope returns AvailableStackedMasters: null when none of
        // the requested UuidList match anything it has. We do better — try to
        // map App's known UUIDs to on-disk session directories by reading
        // each session's info.json. UUIDs the App doesn't know about (e.g.
        // sessions captured on a different App instance) are simply not
        // returned, matching real-telescope semantics.
        QJsonArray available;
        const QJsonArray uuidList = obj.value("UuidList").toArray();
        QDir astroDir(m_telescopeState->astroBaseDir);
        if (!uuidList.isEmpty() && astroDir.exists()) {
            const QStringList subs = astroDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
            for (const QJsonValue& uv : uuidList) {
                const QString reqUuid = uv.toString();
                if (reqUuid.isEmpty()) continue;
                for (const QString& d : subs) {
                    QFile f(astroDir.filePath(d + "/info.json"));
                    if (!f.open(QIODevice::ReadOnly)) continue;
                    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
                    f.close();
                    const QString sUuid = doc.object().value("StackedInfo")
                                              .toObject().value("uuid").toString();
                    if (sUuid.compare(reqUuid, Qt::CaseInsensitive) == 0) {
                        QJsonObject entry;
                        entry["Uuid"]          = reqUuid;
                        entry["DirectoryName"] = d;
                        available.append(entry);
                        break;
                    }
                }
            }
        }
        response["AvailableStackedMasters"] = available.isEmpty()
            ? QJsonValue(QJsonValue::Null) : QJsonValue(available);
    } else if (command == "GetStretch") {
        response["BlackPoint"] = m_telescopeState->stretchBlackPoint;
        response["WhitePoint"] = m_telescopeState->stretchWhitePoint;
    } else if (command == "SetStretch") {
        if (obj.contains("BlackPoint")) m_telescopeState->stretchBlackPoint = obj["BlackPoint"].toDouble();
        if (obj.contains("WhitePoint")) m_telescopeState->stretchWhitePoint = obj["WhitePoint"].toDouble();
    } else if (command == "GetThumbnail") {
        response["FileLocation"] = "";
    }
    // DeleteAllImagingLists, DeleteImagingList — just acknowledge

    sendJsonResponse(wsConn, response);
}
