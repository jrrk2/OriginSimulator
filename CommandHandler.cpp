#include "CommandHandler.h"
#include <QJsonDocument>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>

CommandHandler::CommandHandler(TelescopeState *state, QObject *parent) 
    : QObject(parent), m_telescopeState(state) {
}

void CommandHandler::processCommand(const QJsonObject &obj, WebSocketConnection *wsConn) {
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    int sequenceId = obj["SequenceID"].toInt();
    QString source = obj["Source"].toString();
    QString type = obj["Type"].toString();
    
    qDebug() << "Processing command:" << command << "to" << destination << "from" << source;
    
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

void CommandHandler::handleRunInitialize(const QJsonObject &obj, WebSocketConnection *wsConn, int sequenceId, const QString &source, const QString &destination) {
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    m_telescopeState->isAligned = false;
    m_telescopeState->isGotoOver = true;
    m_telescopeState->isTracking = false;
    
    if (obj.contains("Date")) m_telescopeState->dateTime.setDate(QDate::fromString(obj["Date"].toString(), "dd MM yyyy"));
    if (obj.contains("Time")) m_telescopeState->dateTime.setTime(QTime::fromString(obj["Time"].toString(), "hh:mm:ss"));
    if (obj.contains("Latitude")) m_telescopeState->latitude = obj["Latitude"].toDouble();
    if (obj.contains("Longitude")) m_telescopeState->longitude = obj["Longitude"].toDouble();
    if (obj.contains("TimeZone")) m_telescopeState->timeZone = obj["TimeZone"].toString();
    
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
        m_telescopeState->targetRa = obj["Ra"].toDouble();
        m_telescopeState->targetDec = obj["Dec"].toDouble();
        
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

void CommandHandler::sendJsonResponse(WebSocketConnection *wsConn, const QJsonObject &response) {
    QJsonDocument doc(response);
    QString message = doc.toJson();
    wsConn->sendTextMessage(message);
}
