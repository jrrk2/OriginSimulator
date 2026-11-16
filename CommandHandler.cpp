#include "CommandHandler.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <libnova/transform.h>
#include <libnova/julian_day.h>

CommandHandler::CommandHandler(TelescopeState *state, QObject *parent)
    : QObject(parent), m_telescopeState(state) {
}

void CommandHandler::processCommand(const QJsonObject &obj, WebSocketConnection *wsConn) {
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    int sequenceId = obj["SequenceID"].toInt();
    QString source = obj["Source"].toString();
    QString type = obj["Type"].toString();
    
    if (false) qDebug() << "Processing command:" << command << "to" << destination << "from" << source;
    
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
    } else if (command == "AbortAxisMovement") {
        handleAbortAxisMovement(obj, wsConn, sequenceId, source, destination);
    } else if (command == "StartTracking") {
        handleStartTracking(obj, wsConn, sequenceId, source, destination);
    } else if (command == "StopTracking") {
        handleStopTracking(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunImaging") {
        handleRunImaging(obj, wsConn, sequenceId, source, destination);
    } else if (command == "RunSampleCapture") {
        handleRunSampleCapture(obj, wsConn, sequenceId, source, destination);
    } else if (command == "CancelImaging") {
        handleCancelImaging(obj, wsConn, sequenceId, source, destination);
    } else if ((command == "MoveToPosition" || command == "MoveTo") && destination == "Focuser") {
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
    } else if (command == "GetFilter" && destination == "Camera") {
        handleGetFilter(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetDirectConnectPassword" && destination == "Network") {
        handleGetDirectConnectPassword(obj, wsConn, sequenceId, source, destination);
    } else if (command == "Slew" && destination == "Mount") {
        handleSlew(obj, wsConn, sequenceId, source, destination);
    } else if (command == "TrackMovingTarget" && destination == "Mount") {
        handleTrackMovingTarget(obj, wsConn, sequenceId, source, destination);
    } else if (command == "HaltTasks" && destination == "TaskController") {
        // real firmware (capture origin1.txt) acks HaltTasks, and the running goto or track stops
        m_telescopeState->mount.halt();
        reply(wsConn, command, sequenceId, source, destination);
    } else if (command == "SetDisableLiveStream" && destination == "LiveStream") {
        m_telescopeState->liveStreamDisabled = obj["Disable"].toBool(true);
        reply(wsConn, command, sequenceId, source, destination);
    } else {
        // Default response for unimplemented commands
        QJsonObject response;
        response["Command"] = command;
        response["Destination"] = source;
        response["ErrorCode"] = 0;
        response["ErrorMessage"] = "";
        response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
        response["SequenceID"] = sequenceId;
        response["Source"] = destination;
        response["Type"] = "Response";
        
        sendJsonResponse(wsConn, response);
    }
}

// In CommandHandler.cpp

void CommandHandler::reply(WebSocketConnection *wsConn, const QString &command, int sequenceId, const QString &source, const QString &destination, int errorCode, const QString &message) {
    QJsonObject response;
    response["Command"] = command;
    response["Destination"] = source;
    response["ErrorCode"] = errorCode;
    response["ErrorMessage"] = message;
    response["ExpiredAt"] = QDateTime::currentDateTime().toSecsSinceEpoch();
    response["SequenceID"] = sequenceId;
    response["Source"] = destination;
    response["Type"] = "Response";
    sendJsonResponse(wsConn, response);
}

void CommandHandler::handleRunInitialize(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // A real initialisation already running refuses a second (2026-11-07: -73 after a reboot, when the firmware starts one by itself)
    if (m_telescopeState->isInitializing) {
        reply(wsConn, "RunInitialize", sequenceId, source, destination, -73, "Initialization already in progress");
        return;
    }
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    if (obj.contains("Date")) m_telescopeState->dateTime.setDate(QDate::fromString(obj["Date"].toString(), "dd MM yyyy"));
    if (obj.contains("Time")) m_telescopeState->dateTime.setTime(QTime::fromString(obj["Time"].toString(), "hh:mm:ss"));
    if (obj.contains("TimeZone")) m_telescopeState->timeZone = obj["TimeZone"].toString();
    // the site comes in radians, as the mount status reports it
    MountModel& m = m_telescopeState->mount;
    m.setSite(obj.contains("Latitude") ? obj["Latitude"].toDouble() : m.latitudeRad(), obj.contains("Longitude") ? obj["Longitude"].toDouble() : m.longitudeRad());

    const bool fake = obj["FakeInitialize"].toBool(false);
    m_telescopeState->isFakeInitialized = fake;
    if (fake) {
        // declared at once: this pointing is azimuth 180 at the tilt sensor's altitude; the mount does not move
        m.initialise(true, TelescopeState::julianDateNow());
        m_telescopeState->isReady = true;
        m_telescopeState->isAligned = true;
        m_telescopeState->numAlignRefs = 0;
        m_telescopeState->state = "INITIALIZING";
        m_telescopeState->stage = "IN_PROGRESS";
    } else {
        // the real one runs for a while (CelestronOriginSimulator::updateInitialization) and needs the live stream for its references
        m.halt();
        m_telescopeState->isInitializing = true;
        m_telescopeState->isReady = false;
        m_telescopeState->isAligned = false;
        m_telescopeState->numAlignRefs = 0;
        m_telescopeState->state = "INITIALIZING";
        m_telescopeState->stage = "IN_PROGRESS";
        m_telescopeState->initInfo.numPoints = 0;
        m_telescopeState->initInfo.positionOfFocus = -1;
        m_telescopeState->initInfo.numPointsRemaining = 2;
        m_telescopeState->initInfo.percentComplete = 0;
    }
    reply(wsConn, "RunInitialize", sequenceId, source, destination);
    emit taskControllerStatusChanged();
    emit initializationStarted(fake);
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
    // J2000 radians; refused until the firmware is ready (an initialisation, real or fake), as the real one answers -5
    const double ra = obj["Ra"].toDouble(), dec = obj["Dec"].toDouble();
    if (!m_telescopeState->isReady || !m_telescopeState->mount.gotoJ2000(ra, dec, TelescopeState::julianDateNow())) {
        reply(wsConn, "GotoRaDec", sequenceId, source, destination, -5, "Not ready");
        return;
    }
    m_telescopeState->targetRa = ra;
    m_telescopeState->targetDec = dec;
    qDebug() << "GotoRaDec: RA" << ra * 12.0 / M_PI << "h Dec" << dec * 180.0 / M_PI << "deg, the short way round";
    reply(wsConn, "GotoRaDec", sequenceId, source, destination);
}

void CommandHandler::handleTrackMovingTarget(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // TargetTrack: [{Ra, Dec (J2000 radians), JulianDate (UTC)}], followed by interpolation; the firmware follows it from the first point to the last
    std::vector<MountModel::TrackPoint> points;
    for (const QJsonValue& v : obj["TargetTrack"].toArray()) {
        const QJsonObject p = v.toObject();
        points.push_back({ p["Ra"].toDouble(), p["Dec"].toDouble(), p["JulianDate"].toDouble() });
    }
    if (!m_telescopeState->isReady || !m_telescopeState->mount.track(points)) {
        reply(wsConn, "TrackMovingTarget", sequenceId, source, destination, -5, "Not ready");
        return;
    }
    qDebug() << "TrackMovingTarget:" << points.size() << "points";
    reply(wsConn, "TrackMovingTarget", sequenceId, source, destination);
}

void CommandHandler::handleSlew(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // relative moves at a rate, -9..9, with or without an initialisation; zero rates stop a manual move and leave a goto or track running
    m_telescopeState->mount.setRates(obj["AltRate"].toInt(), obj["AzmRate"].toInt());
    reply(wsConn, "Slew", sequenceId, source, destination);
}

void CommandHandler::handleAbortAxisMovement(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->mount.halt();
    
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
    // firmware 1.5.x answers "Unknown command" (2026-11-08): tracking is stopped with HaltTasks
    Q_UNUSED(obj);
    reply(wsConn, "StopTracking", sequenceId, source, destination, -2, "Unknown command");
}

void CommandHandler::handleRunImaging(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->isImaging = true;
    m_telescopeState->imagingTimeLeft = 30;
    
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
    // the real telescope's command is MoveTo with "Value"; MoveToPosition/"Position" is kept for older clients
    int targetPosition = obj.contains("Value") ? obj["Value"].toInt() : obj["Position"].toInt();
    m_telescopeState->position = targetPosition;
    
    QJsonObject response;
    response["Command"] = obj["Command"].toString("MoveToPosition");
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
    
    QJsonArray dirList;
    for (const QString &dir : m_telescopeState->astrophotographyDirs) {
        dirList.append(dir);
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
    if (obj.contains("BitDepth")) m_telescopeState->bitDepth = obj["BitDepth"].toInt();
    if (obj.contains("Debayer")) m_telescopeState->debayer = obj["Debayer"].toBool();
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
    response["Max"] = 37527;            // as the real telescope states its focuser travel (capture origin1.txt)
    response["Min"] = 1975;
    
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

void CommandHandler::sendJsonResponse(WebSocketConnection *wsConn, const QJsonObject &response) {
    QJsonDocument doc(response);
    QString message = doc.toJson();
    wsConn->sendTextMessage(message);
}

void CommandHandler::handleRunSampleCapture(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    // Extract exposure parameters
    double exposureTime = obj["ExposureTime"].toDouble();
    int iso = obj["ISO"].toInt();
    m_telescopeState->debayer = obj["Debayer"].toBool(false);   // current firmware (1.5.x) defaults to the raw Bayer frame when absent
    
    qDebug() << "RunSampleCapture: Exposure=" << exposureTime << "ISO=" << iso;
    
    // Update telescope state with camera parameters
    m_telescopeState->exposure = exposureTime;
    m_telescopeState->iso = iso;
    m_telescopeState->isImaging = true;
    m_telescopeState->imagingTimeLeft = static_cast<int>(exposureTime) + 2;  // Exposure + processing time
    
    // Set TaskController state for sample capture
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
    
    // Emit signal for task controller status change
    emit taskControllerStatusChanged();
    
    // Emit signal to start the imaging simulation
    emit imagingStarted();
}

void CommandHandler::completeSampleCapture() {
    m_telescopeState->state = "IDLE";
    emit taskControllerStatusChanged();
}

void CommandHandler::completeImaging() {
    m_telescopeState->isImaging = false;
    m_telescopeState->imagingTimeLeft = 0;
    
    // If we were in SAMPLE_CAPTURE state, return to IDLE
    if (m_telescopeState->state == "SAMPLE_CAPTURE") {
        completeSampleCapture();
    }
    
    emit imagingComplete();
}
