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
    } else if (command == "GetFilter" && destination == "Camera") {
        handleGetFilter(obj, wsConn, sequenceId, source, destination);
    } else if (command == "GetDirectConnectPassword" && destination == "Network") {
        handleGetDirectConnectPassword(obj, wsConn, sequenceId, source, destination);
    } else if (command == "Slew" && destination == "Mount") {
        handleSlew(obj, wsConn, sequenceId, source, destination);
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
    m_telescopeState->isInitializing = false;
    m_telescopeState->initializationProgress = 0;
    m_telescopeState->state = "INITIALIZED";
    m_telescopeState->stage = "FINISHED";
    m_telescopeState->isReady = true;
    
    // Reset initialization info
    m_telescopeState->initInfo.numPoints = 2;
    m_telescopeState->initInfo.positionOfFocus = -1;
    m_telescopeState->initInfo.numPointsRemaining = 0;
    m_telescopeState->initInfo.percentComplete = 100;
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
        if (true) qDebug() << "*** GOTO COMMAND RECEIVED ***";
        if (false) qDebug() << "Raw RA from JSON:" << received_ra << "radians";
        if (false) qDebug() << "Raw Dec from JSON:" << received_dec << "radians";
        if (true) qDebug() << "RA in hours:" << (received_ra * 12.0 / M_PI);
        if (true) qDebug() << "Dec in degrees:" << (received_dec * 180.0 / M_PI);
        
        m_telescopeState->targetRa = received_ra;
        m_telescopeState->targetDec = received_dec;
        
        if (false) qDebug() << "Stored targetRa:" << m_telescopeState->targetRa;
        if (false) qDebug() << "Stored targetDec:" << m_telescopeState->targetDec;	
        
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
    if (m_telescopeState->isAligned) {
        m_telescopeState->isGotoOver = false;
        m_telescopeState->isSlewing = true;

        if (false) qDebug() << "*** SLEW COMMAND RECEIVED ***";
        // Iterate with auto and structured bindings (C++17)
        if (false) for (auto it = obj.begin(); it != obj.end(); ++it) {
            QString key = it.key();
            QJsonValue value = it.value();
            // Process key and value
            if (true) qDebug() << key << ": " << value;
        }

        long alt_rate = obj["AltRate"].toInt();
        long az_rate = obj["AzmRate"].toInt();
        alt_rate = alt_rate < 0 ? -1 << -alt_rate : (1 << alt_rate) - 1;
        az_rate = az_rate < 0 ? -1 << -az_rate : (1 << az_rate) - 1;
        
        if (true) qDebug() << alt_rate << " " << az_rate;
        
        // Your RA/Dec coordinates
        struct ln_equ_posn equ_pos;
        equ_pos.ra = m_telescopeState->targetRa * 180.0 / M_PI;
        // Right Ascension in degrees
        equ_pos.dec = m_telescopeState->targetDec * 180.0 / M_PI;
        // Declination in degrees
        // Observer location
        struct ln_lnlat_posn observer;
        observer.lng = m_telescopeState->longitude;   // Longitude (negative for West)
        observer.lat = m_telescopeState->latitude;    // Latitude (positive for North)

        // Get current Julian Date
        double JD = ln_get_julian_from_sys();

        // Convert to Alt/Az
        struct ln_hrz_posn hrz_pos;
        ln_get_hrz_from_equ(&equ_pos, &observer, JD, &hrz_pos);

        hrz_pos.az += alt_rate / 3600.0;
        hrz_pos.alt += az_rate / 3600.0;
        // Convert Alt/Az to RA/Dec
 
        ln_get_equ_from_hrz(&hrz_pos, &observer, JD, &equ_pos);

        if (true) qDebug() << "Old Incremental targetRa/Dec:"
            << m_telescopeState->targetRa << " "
            << m_telescopeState->targetDec;

        m_telescopeState->targetRa = equ_pos.ra * M_PI / 180.0;
        m_telescopeState->targetDec = equ_pos.dec * M_PI / 180.0;

        if (true) qDebug() << "New Incremental targetRa/Dec:"
            << m_telescopeState->targetRa << " "
            << m_telescopeState->targetDec;
        
        emit slewStarted();
        
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
    } else {
        QJsonObject response;
        response["Command"] = "Slew";
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

void CommandHandler::sendJsonResponse(WebSocketConnection *wsConn, const QJsonObject &response) {
    QJsonDocument doc(response);
    QString message = doc.toJson();
    wsConn->sendTextMessage(message);
}
