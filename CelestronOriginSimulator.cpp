#include "CelestronOriginSimulator.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkInterface>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <cmath>
#include "CelestronOriginSimulator.h"
#include "TiffImageGenerator.h"
#include "FitsImageGenerator.h"
#include "GaiaSky.h"
#include <QUrlQuery>
#include <QRegularExpression>
#include <QJsonArray>
#include <vector>
#include <algorithm>
#include <cmath>
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QNetworkInterface>
#include <QDir>
#include <QFile>
#include <QBuffer>
#include <QImage>
#include <QDebug>
#include <QPainter>
#include <cmath>

CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
    // Initialize core components
    m_telescopeState = new TelescopeState();
    m_commandHandler = new CommandHandler(m_telescopeState, this);
    m_statusSender = new StatusSender(m_telescopeState, this);
    
    // Initialize the dual protocol server
    m_tcpServer = new QTcpServer(this);
    m_udpSocket = new QUdpSocket(this);

    // Initialize headless mosaic creator
    m_mosaicCreator = new EnhancedMosaicCreator(this); // Headless mode
    m_mosaicInProgress = false;
    m_surveyFits = qEnvironmentVariable("ORIGIN_SIM_SURVEY", "0") != "0";
    
    // Connect mosaic completion signal
    connect(m_mosaicCreator, &EnhancedMosaicCreator::mosaicComplete, 
            this, &CelestronOriginSimulator::onMosaicComplete);
    
    if (false) qDebug() << "Headless Enhanced Mosaic Creator initialized";
    
    setupHipsIntegration();  // Changed from setupRubinIntegration
    
    const int port = qEnvironmentVariable("ORIGIN_SIM_PORT", QString::number(SERVER_PORT)).toInt();
    GaiaSky::instance();                // load the catalogue before the first frame is asked for
    if (m_tcpServer->listen(QHostAddress::Any, port)) {
        qDebug() << "Origin simulator on port" << port << (GaiaSky::instance().available() ? "with Gaia stars" : "with synthetic stars");
        setupConnections();
        setupTimers();
        
        // First broadcast immediately
        QTimer::singleShot(100, this, &CelestronOriginSimulator::sendBroadcast);
    } else {
        qWarning() << "Failed to start Origin simulator on port" << port << ":" << m_tcpServer->errorString();
    }
}

CelestronOriginSimulator::~CelestronOriginSimulator() {
    if (m_tcpServer) {
        m_tcpServer->close();
    }
    qDeleteAll(m_webSocketClients);
}

void CelestronOriginSimulator::setupHipsIntegration() {
    m_hipsClient = new ProperHipsClient(this);
    
    // Set up HiPS image directory to integrate with existing simulator structure
    QString homeDir = QDir::homePath();
    QString appSupportDir = QDir(homeDir).absoluteFilePath("Library/Application Support/OriginSimulator");
    QString hipsDir = QDir(appSupportDir).absoluteFilePath("Images/HiPS");
    
    // Create the HiPS directory
    QDir().mkpath(hipsDir);
    
    // Connect ProperHipsClient signals to our slots
    connect(m_hipsClient, &ProperHipsClient::testingComplete, 
            this, &CelestronOriginSimulator::onHipsTestingComplete);
    
    if (false) qDebug() << "ProperHips integration initialized";
    if (false) qDebug() << "HiPS images will be saved to:" << hipsDir;

    // Set initial coordinates: where the mount model points
    m_telescopeState->mount.trueJ2000(TelescopeState::julianDateNow(), m_telescopeState->ra, m_telescopeState->dec);

    // Start with initial position fetch
    SkyPosition initialPos = {
        m_telescopeState->ra * 180.0 / M_PI,
        m_telescopeState->dec * 180.0 / M_PI,
        "Initial_Position",
        "Telescope starting position"
    };
    
    fetchHipsImagesForPosition(initialPos);
}

void CelestronOriginSimulator::fetchHipsImagesForPosition(const SkyPosition& position) {
    if (false) qDebug() << QString("Fetching HiPS images for position: RA=%1°, Dec=%2°")
                .arg(position.ra_deg, 0, 'f', 6)
                .arg(position.dec_deg, 0, 'f', 6);
    
    // Use the best available survey
    QString bestSurvey = getBestAvailableSurvey();
    if (bestSurvey.isEmpty()) {
        if (false) qDebug() << "No working surveys available";
        return;
    }
    
    // Test the survey at this position
    m_hipsClient->testSurveyAtPosition(bestSurvey, position);
    
    generateCurrentSkyImage();
}

QString CelestronOriginSimulator::getBestAvailableSurvey() const {
    // Get working surveys from previous tests, or use defaults
    QStringList workingSurveys = m_hipsClient->getWorkingSurveys();
    
    if (workingSurveys.isEmpty()) {
        // Default to known working surveys
        QStringList defaultSurveys = {"DSS2_Color", "2MASS_Color", "2MASS_J"};
        return defaultSurveys.first();
    }
    
    // Prefer color surveys over single-band
    QStringList preferredOrder = {"DSS2_Color", "2MASS_Color", "Mellinger_Color", 
                                  "2MASS_J", "DSS2_Red", "Gaia_DR3"};
    
    for (const QString& preferred : preferredOrder) {
        if (workingSurveys.contains(preferred)) {
            return preferred;
        }
    }
    
    return workingSurveys.first();
}

// Slot implementations for HiPS integration
void CelestronOriginSimulator::onHipsImageReady(const QString& filename) {
    if (false) qDebug() << "HiPS Observatory image ready:" << filename;
    
    // Extract just the filename for the telescope's image system
    QFileInfo fileInfo(filename);
    QString relativePath = QString("/SmartScope-1.0/dev2/Images/Astrophotography/%1").arg(fileInfo.fileName());
    
    // Update telescope state with new image location
    m_telescopeState->fileLocation = relativePath;
    m_telescopeState->imageType = "HIPS_IMAGE";
    m_telescopeState->sequenceNumber++;

    // Notify all connected clients about the new image
    m_statusSender->sendNewImageReadyToAll();
    
    if (true) qDebug() << "Telescope updated with HiPS image:" << relativePath;
}

void CelestronOriginSimulator::onHipsTilesAvailable(const QStringList& filenames) {
    if (false) qDebug() << "HiPS Observatory tiles available:" << filenames.size();
    
    if (!filenames.isEmpty()) {
        // Use the first available tile
        onHipsImageReady(filenames.first());
        
        // Log all available files
        for (const QString& file : filenames) {
            QFileInfo info(file);
            if (false) qDebug() << "  Available:" << info.fileName() << "(" << info.size() << "bytes)";
        }
    }
}

void CelestronOriginSimulator::onHipsFetchError(const QString& error_message) {
    if (false) qDebug() << "HiPS Observatory fetch error:" << error_message;
    
    // Send error notification to clients
    QJsonObject errorNotification;
    errorNotification["Command"] = "Warning";
    errorNotification["Destination"] = "All";
    errorNotification["Source"] = "HipsImageServer";
    errorNotification["Type"] = "Notification";
    errorNotification["Message"] = "HiPS Observatory data unavailable: " + error_message;
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["SequenceID"] = m_telescopeState->getNextSequenceId();
    
    m_statusSender->sendJsonMessageToAll(errorNotification);
}

void CelestronOriginSimulator::onHipsTestingComplete() {
    if (false) qDebug() << "HiPS testing completed";
    
    // Print summary of results
    m_hipsClient->printSummary();
    
    // Save results for debugging
    m_hipsClient->saveResults("hips_test_results.csv");
}

void CelestronOriginSimulator::setupConnections() {
    connect(m_tcpServer, &QTcpServer::newConnection, this, &CelestronOriginSimulator::handleNewConnection);
    
    // Connect command handler signals
    connect(m_commandHandler, &CommandHandler::imagingStarted, this, [this]() {
        m_imagingTimer->start(1000);
        // record the mount through the exposure (stepMount, every 0.1 s): that is what the frame shows
        m_exposureSamples.clear();
        m_exposureSampling = true;
        QTimer::singleShot(std::max(1, int(m_telescopeState->exposure * 1000)), this, [this]() { m_exposureSampling = false; });
    });

    // Connect imaging completion signal
    connect(m_commandHandler, &CommandHandler::imagingComplete, this, [this]() {
        m_imagingTimer->stop();
    });

    // Connect initialization signal
    connect(m_commandHandler, &CommandHandler::initializationStarted, this, [this](bool fakeInit) {
        if (fakeInit) {
            // Complete immediately for fake initialization
            QTimer::singleShot(1000, this, &CelestronOriginSimulator::completeInitialization);
        } else {
            // Start the initialization process
            m_initUpdateCount = 0;
            m_initTimer->start(qEnvironmentVariable("ORIGIN_SIM_INIT_TICK_MS", "3000").toInt());
        }
    });

    // Add these connections to your existing setupConnections method
    connect(m_commandHandler, &CommandHandler::taskControllerStatusChanged, this, [this]() {
	m_statusSender->sendTaskControllerStatusToAll();
    });

    connect(m_commandHandler, &CommandHandler::imagingComplete, this, [this]() {
	m_imagingTimer->stop();
    });
 
}

void CelestronOriginSimulator::setupTimers() {
    // Create broadcast timer
    m_broadcastTimer = new QTimer(this);
    connect(m_broadcastTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendBroadcast);
    m_broadcastTimer->start(BROADCAST_INTERVAL);
    
    // Create update timer for regular status updates
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &CelestronOriginSimulator::sendStatusUpdates);
    m_updateTimer->start(1000);
    
    // Create imaging timer
    m_imagingTimer = new QTimer(this);
    connect(m_imagingTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateImaging);

    m_mountTimer = new QTimer(this);
    connect(m_mountTimer, &QTimer::timeout, this, &CelestronOriginSimulator::stepMount);
    m_mountClock.start();
    m_mountTimer->start(100);

    // Create initialization timer
    m_initTimer = new QTimer(this);
    m_initTimer->setSingleShot(false);
    connect(m_initTimer, &QTimer::timeout, this, &CelestronOriginSimulator::updateInitialization);
}

// =============================================================================
// CORRECTED updateImaging() function for CelestronOriginSimulator
// =============================================================================
// 
// This fixes the issue where NewImageReady notifications were being sent
// continuously during sample capture instead of once when complete.
//
// Replace the existing updateImaging() function (around line 2917 in sim.txt)
// with this corrected version.
// =============================================================================


// Survey luminance at the sensor plate scale for a FITS frame: linear, sky-subtracted, placed where the survey tiles fall for the current pointing.
// Returns false when the mosaic is missing or does not overlap the frame.
static bool buildSurveyPlane(const QImage& mosaic, double mosaicRaDeg, double mosaicDecDeg, double raDeg, double decDeg,
                             std::vector<float>& plane, int footprint[4])
{
    const int W = FitsImageGenerator::WIDTH, H = FitsImageGenerator::HEIGHT;
    if (mosaic.isNull()) return false;
    const double scale = 1.61 / 1.478;                                            // mosaic arcsec/px over sensor arcsec/px
    const QImage g = mosaic.convertToFormat(QImage::Format_Grayscale8).scaled(int(mosaic.width() * scale), int(mosaic.height() * scale), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const double pxPerDeg = 3600.0 / 1.478;
    // pointing east/north of where the mosaic was made moves the sky west (+x; east is left) / south (+y)
    const int ox = int(std::lround((W - g.width()) / 2.0 + (raDeg - mosaicRaDeg) * std::cos(decDeg * M_PI / 180.0) * pxPerDeg));
    const int oy = int(std::lround((H - g.height()) / 2.0 + (decDeg - mosaicDecDeg) * pxPerDeg));
    const int x0 = std::max(0, ox), y0 = std::max(0, oy), x1 = std::min(W, ox + g.width()), y1 = std::min(H, oy + g.height());
    if (x1 - x0 < 64 || y1 - y0 < 64) return false;
    std::vector<float> lin; lin.reserve(size_t(x1 - x0) * (y1 - y0));
    plane.assign(size_t(W) * H, 0.f);
    for (int y = y0; y < y1; ++y) {
        const uchar* line = g.constScanLine(y - oy);
        for (int x = x0; x < x1; ++x) { const float v = std::pow(line[x - ox] / 255.0f, 2.2f); plane[size_t(y) * W + x] = v; lin.push_back(v); }
    }
    std::vector<float> sub; for (size_t i = 0; i < lin.size(); i += 7) sub.push_back(lin[i]);
    std::nth_element(sub.begin(), sub.begin() + sub.size() / 2, sub.end());
    const float bg = sub[sub.size() / 2];                                          // survey sky level: the simulator supplies its own sky
    for (int y = y0; y < y1; ++y) for (int x = x0; x < x1; ++x) plane[size_t(y) * W + x] = std::max(0.f, plane[size_t(y) * W + x] - bg);
    footprint[0] = x0; footprint[1] = y0; footprint[2] = x1; footprint[3] = y1;
    return true;
}

// Gaia stars in the frame, star by star as they appear: J2000 to the horizon, raised by the refraction at each star's own altitude, then the gnomonic
// (TAN) projection onto the plane square to the tube's axis, with the image's up and right as the mount carries the camera. Exact rather than a flat
// offset (several pixels out at the corners of a 75 x 50 arcminute field), and the differential refraction squashes a low field vertically.
static void projectStars(const MountModel& mount, double jd, double ra0, double dec0, std::vector<FitsImageGenerator::Params::PixelStar>& out, double weight = 1.0)
{
    const int W = FitsImageGenerator::WIDTH, H = FitsImageGenerator::HEIGHT;
    const double pxPerRad = 206264.806 / 1.478;
    const double radius = std::hypot(W / 2.0, H / 2.0) / pxPerRad * 180.0 / M_PI + 0.1;
    double b[3], up[3], right[3];
    mount.imageBasis(b, up, right);
    for (const GaiaSky::Star& s : GaiaSky::instance().cone(ra0 * 180.0 / M_PI, dec0 * 180.0 / M_PI, radius)) {
        double v[3];
        mount.apparentDirection(s.raDeg * M_PI / 180.0, s.decDeg * M_PI / 180.0, jd, v);
        const double d = v[0] * b[0] + v[1] * b[1] + v[2] * b[2];
        if (d <= 0) continue;
        const double x = (v[0] * right[0] + v[1] * right[1] + v[2] * right[2]) / d, y = (v[0] * up[0] + v[1] * up[1] + v[2] * up[2]) / d;
        out.push_back({ W / 2.0 + x * pxPerRad, H / 2.0 - y * pxPerRad, double(s.g), weight });
    }
}

void CelestronOriginSimulator::updateImaging() {
    if (m_telescopeState->isImaging) {
        m_telescopeState->imagingTimeLeft--;
        
        if (m_telescopeState->imagingTimeLeft <= 0 && m_surveyFits && m_telescopeState->bitDepth == 16 && !m_telescopeState->debayer && m_surveyWaitTicks < 90) {
            // survey mode: hold the frame until the HiPS mosaic for the current pointing has arrived (up to 90 s), so the image matches the sky
            const double cosd = std::cos(m_telescopeState->dec);
            const double dRa = (m_telescopeState->ra * 180.0 / M_PI - m_surveyRaDeg) * cosd, dDec = m_telescopeState->dec * 180.0 / M_PI - m_surveyDecDeg;
            if (m_mosaicInProgress || m_surveyMosaic.isNull() || std::hypot(dRa, dDec) > 0.1) { m_telescopeState->imagingTimeLeft = 1; ++m_surveyWaitTicks; return; }
        }
        if (m_telescopeState->imagingTimeLeft <= 0) {
            m_surveyWaitTicks = 0;
            m_telescopeState->isImaging = false;
            m_imagingTimer->stop();
            
            if (m_telescopeState->bitDepth == 16 && !m_telescopeState->debayer) {
                // raw 16-bit Bayer FITS snapshot, as the current firmware delivers
                const QString imagePath = m_telescopeState->getNextFITSFile();
                m_telescopeState->fileLocation = imagePath;
                m_telescopeState->imageType = "SAMPLE_CAPTURE";
                const QString fullPath = m_absoluteTempDir + "/" + imagePath;
                QDir().mkpath(QFileInfo(fullPath).absolutePath());
                FitsImageGenerator::Params fp;
                // the mount through the exposure, sampled every 0.1 s: the frame centre and star shape from mid-exposure, the stars summed over all of it
                if (m_exposureSamples.empty()) m_exposureSamples.push_back({ TelescopeState::julianDateNow(), m_telescopeState->mount });
                const auto& mid = m_exposureSamples[m_exposureSamples.size() / 2];
                const double jd = mid.first;
                const MountModel& mount = mid.second;
                mount.trueJ2000(jd, fp.raRad, fp.decRad);                                     // where the tube really points
                if (mount.isInitialised()) mount.firmwareJ2000(jd, fp.reportedRaRad, fp.reportedDecRad);   // what the firmware believes, for the header
                std::vector<FitsImageGenerator::Params::PixelStar> catalogue;
                if (!m_surveyFits && GaiaSky::instance().available()) {
                    // field rotation and drift: each star at up to 40 moments through the exposure, each with its share of the light
                    const size_t n = std::min<size_t>(40, m_exposureSamples.size());
                    for (size_t k = 0; k < n; ++k) {
                        const auto& sample = m_exposureSamples[(k * m_exposureSamples.size()) / n];
                        projectStars(sample.second, sample.first, fp.raRad, fp.decRad, catalogue, 1.0 / double(n));
                    }
                    fp.catalogue = &catalogue;
                }
                mount.starShape(fp.elongation, fp.elongationAngleDeg);
                m_exposureSamples.clear();
                fp.exposureS = m_telescopeState->exposure; fp.iso = m_telescopeState->iso;
                fp.focuserPosition = m_telescopeState->position; fp.frameNumber = ++m_telescopeState->frameCounter; fp.cloud = qEnvironmentVariable("ORIGIN_SIM_CLOUD", QString::number(m_telescopeState->cloud)).toDouble();   // 0..1 test hook
                std::vector<float> surveyPlane;
                if (m_surveyFits && buildSurveyPlane(m_surveyMosaic, m_surveyRaDeg, m_surveyDecDeg, m_telescopeState->ra * 180.0 / M_PI, m_telescopeState->dec * 180.0 / M_PI, surveyPlane, fp.surveyFootprint))
                { fp.survey = &surveyPlane; fp.stars = 1500; }       // denser synthetic fill outside the survey tiles, closer to the survey's own star density
                else if (m_surveyFits) qDebug() << "survey mosaic unavailable for this pointing: synthetic stars only";
                QFile f(fullPath);
                if (f.open(QIODevice::WriteOnly)) { f.write(FitsImageGenerator::generate(fp)); f.close(); }
                qDebug() << "=== SAMPLE CAPTURE COMPLETE === FITS" << fullPath << "focuser" << fp.focuserPosition << (fp.catalogue ? QString("%1 Gaia stars").arg(catalogue.size()) : QString("synthetic stars")) << "elongation" << fp.elongation
;
            } else {
                QString imagePath = m_telescopeState->getNextTIFFFile();
                m_telescopeState->fileLocation = imagePath;
                QString fullPath = m_absoluteTempDir + "/" + imagePath;

                // No cached image data
                TiffImageGenerator::generateSyntheticStarField(fullPath, 150);
                qDebug() << "=== SAMPLE CAPTURE COMPLETE ===";
                qDebug() << "Generated synthetic star field (no cached image)";
            }

            m_statusSender->sendNewImageReadyToAll();
            m_telescopeState->imageType = "LIVE";       // only this notification is a snapshot; the live-view stream that follows is not
            m_commandHandler->completeImaging();
        }
    }
}

// =============================================================================
// EXPLANATION OF THE FIX
// =============================================================================
//
// BEFORE (INCORRECT):
// - Sent NewImageReady notification on EVERY timer tick during SAMPLE_CAPTURE
// - Changed image path continuously
// - INDI driver received multiple notifications for same capture
// - Confusing behavior and potential race conditions
//
// AFTER (CORRECT):
// - Only sends NewImageReady notification ONCE when imaging completes
// - Sets image path once when ready
// - Matches real telescope behavior
// - INDI driver receives exactly one notification per capture
//
// FLOW:
// 1. INDI sends RunSampleCapture with ExposureTime=1.0, ISO=800
// 2. Simulator responds immediately with success
// 3. imagingStarted signal starts m_imagingTimer (1 second ticks)
// 4. imagingTimeLeft counts down from (exposureTime + 2 seconds)
// 5. When imagingTimeLeft reaches 0:
//    a. Stop timer
//    b. Set fileLocation to next available image
//    c. Send NewImageReady notification
//    d. Complete imaging
// 6. INDI driver receives notification and downloads image via HTTP
//
// =============================================================================

void CelestronOriginSimulator::updateInitialization() {
    // Simulate real telescope initialization progression
    m_initUpdateCount++;
    
    // Send status update with progress
    m_statusSender->sendTaskControllerStatusToAll();
    
    // After several updates, set focus position (matches real telescope behavior)
    if (m_initUpdateCount == 5) {
        m_telescopeState->initInfo.positionOfFocus = 18617; // Use value from real trace
    }
    
    // After more updates, find first alignment point
    if (m_initUpdateCount == 10) {
        m_telescopeState->initInfo.numPoints = 1;
        m_telescopeState->initInfo.numPointsRemaining = 1;
        m_telescopeState->initInfo.percentComplete = 50;
    }
    
    // the references need the live stream (2026-11-08: -53 "Can't add alignment reference" with it disabled)
    if (m_initUpdateCount == 10 && m_telescopeState->liveStreamDisabled) {
        failInitialization(-53, "Can't add alignment reference");
        return;
    }
    // under a poor sky it can fail anyway: ORIGIN_SIM_INIT_FAIL percent a step (0 by default)
    if (m_initUpdateCount < 10 && QRandomGenerator::global()->bounded(100) < qEnvironmentVariable("ORIGIN_SIM_INIT_FAIL", "0").toInt()) {
        failInitialization(-78, "Initialization failed. Please point the scope away from any bright lights; buildings; trees and try again.");
        return;
    }
    
    // Complete the initialization
    if (m_initUpdateCount >= 15) {
        m_telescopeState->initInfo.numPoints = 2;
        m_telescopeState->initInfo.numPointsRemaining = 0;
        m_telescopeState->initInfo.percentComplete = 100;
        completeInitialization();
    }
}

void CelestronOriginSimulator::completeInitialization() {
    // Stop the timer
    m_initTimer->stop();
    m_initUpdateCount = 0;
    
    // Set final status
    if (m_telescopeState->isInitializing) {
        m_telescopeState->mount.initialise(false, TelescopeState::julianDateNow());   // a real alignment: the true pointing here, a level base assumed
        m_telescopeState->isAligned = true;
        m_telescopeState->numAlignRefs = 2;
    }
    m_telescopeState->isInitializing = false;
    m_telescopeState->stage = "COMPLETE";
    m_telescopeState->isReady = true;
    
    // Send completion status
    m_statusSender->sendTaskControllerStatusToAll();
    
    // Wait a moment and transition to IDLE state
    QTimer::singleShot(1000, this, [this]() {
        m_telescopeState->state = "IDLE";
        m_statusSender->sendTaskControllerStatusToAll();
    });
}

void CelestronOriginSimulator::failInitialization(int code, const QString &message) {
    // Stop the timer
    m_initTimer->stop();
    m_initUpdateCount = 0;
    
    // Set failure status
    m_telescopeState->isInitializing = false;
    m_telescopeState->stage = "STOPPED";
    m_telescopeState->state = "IDLE";
    m_telescopeState->isReady = false;
    
    // Send error notification
    QJsonObject errorNotification;
    errorNotification["Command"] = "Error";
    errorNotification["Destination"] = "All";
    errorNotification["ErrorCode"] = code;
    errorNotification["ErrorMessage"] = message;
    errorNotification["ExpiredAt"] = m_telescopeState->getExpiredAt();
    errorNotification["Type"] = "Notification";
    m_statusSender->sendJsonMessageToAll(errorNotification);
    
    // Send failure status
    m_statusSender->sendTaskControllerStatusToAll();
}

// Rest of the existing methods remain the same...
void CelestronOriginSimulator::handleNewConnection() {
    QTcpSocket *socket = m_tcpServer->nextPendingConnection();
    
    // Store pending request data
    m_pendingRequests[socket] = QByteArray();
    
    // CRITICAL: Use QueuedConnection to prevent immediate processing conflicts
    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        handleIncomingData(socket);
    }, Qt::QueuedConnection);
    
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        m_pendingRequests.remove(socket);
        socket->deleteLater();
    });
}

// Include the rest of your existing methods here...
// (handleIncomingData, handleWebSocketUpgrade, etc.)
// They remain unchanged from your original implementation

void CelestronOriginSimulator::sendBroadcast() {
    if (qEnvironmentVariable("ORIGIN_SIM_BROADCAST", "1") == "0") return;
    // Prepare the broadcast message
    QString message = QString("Identity:Origin-") + QLatin1String(std::to_string(broadcast_id)) + QString("Z Origin IP Address = %1");
    
    // Get our IP addresses
    QList<QHostAddress> ipAddresses = QNetworkInterface::allAddresses();
    
    for (const QHostAddress &address : ipAddresses) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost) {
            QString broadcastMessage = message.arg(address.toString());
            if (true) qDebug() << broadcastMessage;
            
            // Send broadcast on all network interfaces
            m_udpSocket->writeDatagram(
                broadcastMessage.toUtf8(),
                QHostAddress::Broadcast,
                BROADCAST_PORT
            );
        }
    }
}


void CelestronOriginSimulator::handleIncomingData(QTcpSocket *socket) {
    QByteArray newData = socket->readAll();
    m_pendingRequests[socket].append(newData);
    
    QByteArray &requestData = m_pendingRequests[socket];
    
    // Look for complete HTTP headers
    int headerEndPos = requestData.indexOf("\r\n\r\n");
    if (headerEndPos == -1) {
        // Headers not complete yet, wait for more data
        if (requestData.size() > 8192) {
            // Too much data without finding headers, disconnect
            socket->disconnectFromHost();
            return;
        }
        return;
    }
    
    // We have complete headers, determine the protocol
    QString request = QString::fromUtf8(requestData.left(headerEndPos));
    QStringList lines = request.split("\r\n");
    
    if (lines.isEmpty()) {
        socket->disconnectFromHost();
        return;
    }
    
    QString requestLine = lines[0];
    QStringList requestParts = requestLine.split(" ");
    
    if (requestParts.size() < 3) {
        socket->disconnectFromHost();
        return;
    }
    
    QString method = requestParts[0];
    QString path = requestParts[1];
    
    if (true) qDebug() << "Origin Protocol Request:" << method << path;
    
    // Check if this is a WebSocket upgrade request
    bool isWebSocketUpgrade = false;
    for (const QString &line : lines) {
        if (line.toLower().contains("upgrade: websocket")) {
            isWebSocketUpgrade = true;
            break;
        }
    }
    
    if (method == "GET" && path.startsWith("/sim/")) {
        handleSimRequest(socket, path);
    } else if (isWebSocketUpgrade && path == "/SmartScope-1.0/mountControlEndpoint") {
        // Handle WebSocket upgrade for telescope control
        handleWebSocketUpgrade(socket, requestData);
    } else if (method == "GET" && path.startsWith("/SmartScope-1.0/dev2/Images/Temp/")) {
        // Handle HTTP image request
        handleHttpImageRequest(socket, path);
    } else if (method == "GET" && path.contains("/SmartScope-1.0/dev2//tmp")) {
        // Handle HTTP astrophotography image request
        handleHttpAstroImageRequest(socket, path);
    } else {
        // Unknown request
        sendHttpResponse(socket, 404, "text/plain", "Not Found");
    }
    
    // Clear the pending request data
    m_pendingRequests.remove(socket);
}
// CORRECTED FIX: CelestronOriginSimulator.cpp - Proper handshake sequence
// Perform handshake FIRST, then transfer socket ownership

void CelestronOriginSimulator::handleWebSocketUpgrade(QTcpSocket *socket, const QByteArray &requestData) {
//     // if (false) qDebug() << "*** STARTING WEBSOCKET UPGRADE PROCESS ***";
//     if (false) qDebug() << "Request data size:" << requestData.size();
    
    // Create WebSocketConnection but DON'T disconnect protocol detector yet
    WebSocketConnection *wsConn = new WebSocketConnection(socket, this, false); // false = don't take ownership yet
    
    // FIRST: Perform the handshake using the request data
    if (wsConn->performHandshake(requestData)) {
//         // if (false) qDebug() << "*** HANDSHAKE SUCCESSFUL - TRANSFERRING SOCKET OWNERSHIP ***";
        
        // CRITICAL: NOW disconnect the protocol detector since handshake worked
        disconnect(socket, &QTcpSocket::readyRead, this, nullptr);
//         // if (false) qDebug() << "*** PROTOCOL DETECTOR DISCONNECTED ***";
        
        // Clear any pending data since we're switching protocols  
        m_pendingRequests.remove(socket);
        
        // NOW let WebSocketConnection take full ownership
        wsConn->takeSocketOwnership();
        
        // Add to our client list
        m_webSocketClients.append(wsConn);
        m_statusSender->addWebSocketClient(wsConn);
        
        // Set up all signal connections for WebSocket handling
        connect(wsConn, &WebSocketConnection::textMessageReceived, 
                this, &CelestronOriginSimulator::processWebSocketCommand);
        
        connect(wsConn, &WebSocketConnection::pingReceived,
                this, &CelestronOriginSimulator::handleWebSocketPing);
        
        connect(wsConn, &WebSocketConnection::pongReceived,
                this, &CelestronOriginSimulator::handleWebSocketPong);
        
        connect(wsConn, &WebSocketConnection::pingTimeout,
                this, &CelestronOriginSimulator::handleWebSocketTimeout);
        
        connect(wsConn, &WebSocketConnection::disconnected, 
                this, &CelestronOriginSimulator::onWebSocketDisconnected);
        
//         if (false) qDebug() << "WebSocket connection established for telescope control";
        
        // Send initial status updates after a brief delay
        QTimer::singleShot(1000, this, [this, wsConn]() {
            if (m_webSocketClients.contains(wsConn)) {
                m_statusSender->sendMountStatus(wsConn);
                m_statusSender->sendFocuserStatus(wsConn);
                m_statusSender->sendCameraParams(wsConn);
                m_statusSender->sendDiskStatus(wsConn);
                m_statusSender->sendTaskControllerStatus(wsConn);
                m_statusSender->sendEnvironmentStatus(wsConn);
                m_statusSender->sendDewHeaterStatus(wsConn);
                m_statusSender->sendOrientationStatus(wsConn);
            }
        });
    } else {
//         // if (false) qDebug() << "*** HANDSHAKE FAILED - KEEPING PROTOCOL DETECTOR ***";
        
        // Handshake failed, so keep the original protocol detector connected
        // Don't disconnect anything - let it continue as HTTP
        sendHttpResponse(socket, 400, "text/plain", "Bad WebSocket Request");
        
        // Clean up the failed WebSocket connection
        delete wsConn;
    }
}

void CelestronOriginSimulator::handleHttpImageRequest(QTcpSocket *socket, const QString &path) {
    if (false) qDebug() << "Handling HTTP image request for path:" << path;

    sendHttpResponse(socket, 200, "image/jpeg", m_imageData);
}

void CelestronOriginSimulator::handleHttpAstroImageRequest(QTcpSocket *socket, const QString &path) {
    // Normalize path by replacing double slashes
    QString normalizedPath = path;
    normalizedPath.replace("//", "/");
    
    // Extract just the requested filename
    QString fileName = QFileInfo(normalizedPath).fileName();

    // FITS snapshots are served as generated; everything else keeps the pre-generated 16-bit TIFF
    if (fileName.endsWith(".fits", Qt::CaseInsensitive)) {
        // any of the snapshot files, by name: the live view moves fileLocation on every few seconds, and a slow render can leave the download behind it
        static const QRegularExpression snapshot("^Images_\\d\\.fits$");
        QFile fits(m_absoluteTempDir + "/" + (snapshot.match(fileName).hasMatch() ? "/tmp/" + fileName : m_telescopeState->fileLocation));
        if ((snapshot.match(fileName).hasMatch() || m_telescopeState->fileLocation.endsWith(fileName)) && fits.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = fits.readAll();
            qDebug() << "Serving image/fits from" << fits.fileName() << bytes.size() << "bytes";
            sendHttpResponse(socket, 200, "image/fits", bytes);
            return;
        }
        sendHttpResponse(socket, 404, "text/plain", "Image not found");
        return;
    }
    QString fullPath = "/tmp/temp_hips_image.tiff";

    // If the TIFF doesn't exist, fall back to regular search
    if (!QFile::exists(fullPath)) {
        if (!m_telescopeState->fileLocation.isEmpty() && QFile::exists(m_telescopeState->fileLocation)) {
            fullPath = m_telescopeState->fileLocation;
        } else if (QFile::exists(m_absoluteTempDir + "/Images_1.tiff")) {
            fullPath = m_absoluteTempDir + "/Images_1.tiff";
        } else {
            fullPath = QString("%1/%2").arg(m_absoluteTempDir, fileName);
        }
    }

    if (!QFile::exists(fullPath)) {
        qWarning() << "AstroImage request failed - file not found:" << normalizedPath << "->" << fullPath;
        sendHttpResponse(socket, 404, "text/plain", "Image not found");
        return;
    }

    QFile imageFile(fullPath);
    if (!imageFile.open(QIODevice::ReadOnly)) {
        sendHttpResponse(socket, 500, "text/plain", "Failed to open image");
        return;
    }

    QByteArray imageData = imageFile.readAll();
    imageFile.close();

    qDebug() << "Serving image/tiff from" << fullPath << "for" << normalizedPath;
    sendHttpResponse(socket, 200, "image/tiff", imageData);
}

void CelestronOriginSimulator::sendHttpResponse(QTcpSocket *socket, int statusCode, 
                     const QString &contentType, const QByteArray &data) {
    QString statusText;
    switch (statusCode) {
        case 200: statusText = "OK"; break;
        case 404: statusText = "Not Found"; break;
        case 400: statusText = "Bad Request"; break;
        case 500: statusText = "Internal Server Error"; break;
        default: statusText = "Unknown"; break;
    }
    
    QString response = QString("HTTP/1.1 %1 %2\r\n").arg(statusCode).arg(statusText);
    response += QString("Content-Type: %1\r\n").arg(contentType);
    response += QString("Content-Length: %1\r\n").arg(data.size());
    response += "Cache-Control: no-cache\r\n";
    response += "Access-Control-Allow-Origin: *\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";
    
    socket->write(response.toUtf8());
    if (!data.isEmpty()) {
        socket->write(data);
    }
    socket->disconnectFromHost();
}

void CelestronOriginSimulator::processWebSocketCommand(const QString &message) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
    if (!wsConn) return;
    
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) {
//         if (false) qDebug() << "Received invalid JSON:" << message;
        return;
    }
    
    QJsonObject obj = doc.object();
    
    // Extract common fields
    QString command = obj["Command"].toString();
    QString destination = obj["Destination"].toString();
    QString source = obj["Source"].toString();
    
//     if (false) qDebug() << "Received WebSocket command:" << command << "to" << destination << "from" << source;
    
    // Handle status requests directly
    if (command == "GetStatus") {
        int sequenceId = obj["SequenceID"].toInt();
        
        if (destination == "System") {
            m_statusSender->sendSystemVersion(wsConn, sequenceId, source);
        } else if (destination == "Mount") {
            m_statusSender->sendMountStatus(wsConn, sequenceId, source);
        } else if (destination == "Focuser") {
            m_statusSender->sendFocuserStatus(wsConn, sequenceId, source);
        } else if (destination == "TaskController") {
            m_statusSender->sendTaskControllerStatus(wsConn, sequenceId, source);
        } else if (destination == "DewHeater") {
            m_statusSender->sendDewHeaterStatus(wsConn, sequenceId, source);
        } else if (destination == "Environment") {
            m_statusSender->sendEnvironmentStatus(wsConn, sequenceId, source);
        } else if (destination == "OrientationSensor") {
            m_statusSender->sendOrientationStatus(wsConn, sequenceId, source);
        } else if (destination == "Disk") {
            m_statusSender->sendDiskStatus(wsConn, sequenceId, source);
        } else if (destination == "FactoryCalibrationController") {
            m_statusSender->sendCalibrationStatus(wsConn, sequenceId, source);
        }
    } else if (command == "GetVersion") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendSystemVersion(wsConn, sequenceId, source);
    } else if (command == "GetCaptureParameters") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendCameraParams(wsConn, sequenceId, source);
    } else if (command == "GetFilter") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendCameraFilter(wsConn, sequenceId, source);
    } else if (command == "GetModel") {
        int sequenceId = obj["SequenceID"].toInt();
        m_statusSender->sendSystemModel(wsConn, sequenceId, source);
    } else {
        // Process the command through the command handler
        m_commandHandler->processCommand(obj, wsConn);
    }
}

void CelestronOriginSimulator::onWebSocketDisconnected() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
    if (wsConn) {
        m_webSocketClients.removeAll(wsConn);
        m_statusSender->removeWebSocketClient(wsConn);
        wsConn->deleteLater();
    }
}

// CRITICAL: Fix the status update timing to avoid blocking ping/pong
// Replace sendStatusUpdates method with this non-blocking version:

void CelestronOriginSimulator::sendStatusUpdates() {
    // Update time
    m_telescopeState->dateTime = QDateTime::currentDateTime();
    
    // CRITICAL: Use QueuedConnection to avoid blocking the event loop
    static int updateCounter = 0;
    updateCounter++;
    
    // Send updates in smaller batches to prevent blocking
    if (updateCounter % 1 == 0) {
        // Every second - critical updates only
        QTimer::singleShot(0, this, [this]() {
            m_statusSender->sendMountStatusToAll();
        });
    }
    
    if (updateCounter % 2 == 0) {
        QTimer::singleShot(5, this, [this]() {
            m_statusSender->sendFocuserStatusToAll();
        });
    }
    
    if (updateCounter % 3 == 0) {
        QTimer::singleShot(10, this, [this]() {
	  if (!m_telescopeState->isImaging)
	    {
            m_statusSender->sendCameraParamsToAll();
            m_telescopeState->sequenceNumber++;
            m_telescopeState->fileLocation = m_telescopeState->getNextImageFile();
            m_statusSender->sendNewImageReadyToAll();
	    }
        });
    }
    
    // Less frequent updates with longer delays
    if (updateCounter % 10 == 0) {
        QTimer::singleShot(15, this, [this]() {
            m_statusSender->sendEnvironmentStatusToAll();
            m_statusSender->sendDiskStatusToAll();
        });
    }
    
    if (updateCounter % 15 == 0) {
        QTimer::singleShot(20, this, [this]() {
            m_statusSender->sendDewHeaterStatusToAll();
        });
    }
    
    if (updateCounter % 30 == 0) {
        QTimer::singleShot(25, this, [this]() {
            m_statusSender->sendOrientationStatusToAll();
        });
    }
    
    if (updateCounter % 5 == 0) {
        QTimer::singleShot(30, this, [this]() {
            m_statusSender->sendTaskControllerStatusToAll();
        });
    }
    
    // Reset counter to prevent overflow
    if (updateCounter > 1000) {
        updateCounter = 0;
    }
}

// CRITICAL: Add connection monitoring
// Add this method to monitor connection health:

void CelestronOriginSimulator::checkConnectionHealth() {
//     if (false) qDebug() << "Active WebSocket connections:" << m_webSocketClients.size();
    
    for (WebSocketConnection *wsConn : m_webSocketClients) {
        // Check if connection is still alive
        // We don't need to do anything here - the ping/pong mechanism handles it
    }
}

// Add these new slot methods to CelestronOriginSimulator class:

void CelestronOriginSimulator::handleWebSocketPing(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     if (false) qDebug() << "WebSocket ping received from client, payload size:" << payload.size();
    // The WebSocketConnection automatically sends pong, we just log it here
}

void CelestronOriginSimulator::handleWebSocketPong(const QByteArray &payload) {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     if (false) qDebug() << "WebSocket pong received from client, payload size:" << payload.size();
    // Client responded to our ping successfully
}

void CelestronOriginSimulator::handleWebSocketTimeout() {
    WebSocketConnection *wsConn = qobject_cast<WebSocketConnection*>(sender());
//     if (false) qDebug() << "WebSocket ping timeout occurred - client not responding";
    
    if (wsConn && m_webSocketClients.contains(wsConn)) {
        // Remove from active clients but don't delete yet - let disconnected signal handle cleanup
        m_statusSender->removeWebSocketClient(wsConn);
    }
}

void CelestronOriginSimulator::generateCurrentSkyImage() {
    // Skip if already generating
    if (m_mosaicInProgress) {
        if (false) qDebug() << "Mosaic generation already in progress, skipping";
        return;
    }
    
    // Get current telescope pointing
    double ra_rad = m_telescopeState->ra;
    double dec_rad = m_telescopeState->dec;
    double ra_deg = ra_rad * 180.0 / M_PI;
    double dec_deg = dec_rad * 180.0 / M_PI;
    m_pendingRaDeg = ra_deg; m_pendingDecDeg = dec_deg;
    
    if (false) qDebug() << QString("Generating enhanced mosaic for RA=%1°, Dec=%2°")
                .arg(ra_deg, 0, 'f', 6)
                .arg(dec_deg, 0, 'f', 6);
    
    // Create SkyPosition using the existing structure
    SkyPosition currentPosition = {
        ra_deg,
        dec_deg,
        QString("Live_Image_%1").arg(m_telescopeState->imageCounter),
        QString("Real-time telescope position at %1").arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
    };
    
    // Convert coordinates to string format for the mosaic creator
    double ra_hours = ra_deg / 15.0;
    int ra_h = static_cast<int>(ra_hours);
    int ra_m = static_cast<int>((ra_hours - ra_h) * 60);
    double ra_s = ((ra_hours - ra_h) * 60 - ra_m) * 60;
    
    bool dec_negative = dec_deg < 0;
    double abs_dec = std::abs(dec_deg);
    int dec_d = static_cast<int>(abs_dec);
    int dec_m = static_cast<int>((abs_dec - dec_d) * 60);
    double dec_s = ((abs_dec - dec_d) * 60 - dec_m) * 60;
    
    QString raText = QString("%1h%2m%3s")
                     .arg(ra_h).arg(ra_m, 2, 10, QChar('0')).arg(ra_s, 0, 'f', 1);
    
    QString decText = QString("%1%2d%3m%4s")
                      .arg(dec_negative ? "-" : "+")
                      .arg(dec_d).arg(dec_m, 2, 10, QChar('0')).arg(dec_s, 0, 'f', 1);
    
    if (false) qDebug() << QString("Converted coordinates: %1, %2").arg(raText).arg(decText);
    
    // Set coordinates in the headless mosaic creator
    m_mosaicCreator->setCustomCoordinates(raText, decText, currentPosition.name);
    
    // Start mosaic creation (this will use the existing 3x3 tile logic)
    m_mosaicInProgress = true;
    m_mosaicCreator->createCustomMosaic(currentPosition);
    
    if (true) qDebug() << "Enhanced mosaic generation started...";
}

// Method 1: Save to QByteArray as JPEG/PNG (most common)
QByteArray saveImageToByteArray(const QImage& image, const QString& format = "JPEG", int quality = 95) {
    QByteArray byteArray;
    QBuffer buffer(&byteArray);
    buffer.open(QIODevice::WriteOnly);
    
    // Save image to buffer in specified format
    bool success = image.save(&buffer, format.toUtf8().data(), quality);
    
    if (success) {
        if (false) qDebug() << QString("Image saved to buffer: %1 bytes, format: %2")
                    .arg(byteArray.size()).arg(format);
    } else {
        if (false) qDebug() << "Failed to save image to buffer";
    }
    
    return byteArray;
}

void CelestronOriginSimulator::onMosaicComplete(const QImage& mosaic) {
    if (false) qDebug() << "Enhanced mosaic complete, processing for telescope image";
    
    if (mosaic.isNull()) {
        if (false) qDebug() << "Received null mosaic image";
        m_mosaicInProgress = false;
        return;
    }
    
    if (true) qDebug() << QString("Received mosaic: %1x%2 pixels").arg(mosaic.width()).arg(mosaic.height());
    m_surveyMosaic = m_mosaicCreator->cleanMosaic().isNull() ? mosaic : m_mosaicCreator->cleanMosaic(); m_surveyRaDeg = m_pendingRaDeg; m_surveyDecDeg = m_pendingDecDeg;
    QImage fullImage = mosaic.scaled(3056, 2048, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    QImage paddedImage(3056, 2048, QImage::Format_RGB888);
    paddedImage.fill(Qt::black);
    QPainter tiffpainter(&paddedImage);
    int x = (3056 - fullImage.width()) / 2;
    int y = (2048 - fullImage.height()) / 2;
    tiffpainter.drawImage(x, y, fullImage);
    tiffpainter.end();
    QByteArray m_fulltiff = saveImageToByteArray(paddedImage, "TIFF", 100);
    QString tempPath = "/tmp/temp_hips_image.tiff";
    bool success = TiffImageGenerator::generateOriginFormatTiff(tempPath, paddedImage);
    qDebug() << "Saved resized image: " << tempPath;               
    
    // Resize to telescope camera resolution (800x600) - Origin camera specs
    QImage telescopeImage = mosaic.scaled(800, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    
    // Fill any letterbox areas with black (if aspect ratios don't match)
    if (telescopeImage.width() != 800 || telescopeImage.height() != 600) {
        QImage finalImage(800, 600, QImage::Format_RGB888);
        finalImage.fill(Qt::black);
        
        QPainter painter(&finalImage);
        int x = (800 - telescopeImage.width()) / 2;
        int y = (600 - telescopeImage.height()) / 2;
        painter.drawImage(x, y, telescopeImage);
        painter.end();
        
        telescopeImage = finalImage;
    }
    
    // Add telescope-specific overlay
    QPainter painter(&telescopeImage);
    addTelescopeOverlay(painter, telescopeImage);
    painter.end();
    
    m_imageData = saveImageToByteArray(telescopeImage, "JPEG", 95);
  
    if (true) qDebug() << QString("Updated mosaic: %1x%2 pixels").arg(telescopeImage.width()).arg(telescopeImage.height());

    m_mosaicInProgress = false;
}

void CelestronOriginSimulator::addTelescopeOverlay(QPainter& painter, const QImage& image) {
    // Add crosshairs at center (where the exact coordinates are)
    painter.setPen(QPen(Qt::yellow, 2));
    int centerX = image.width() / 2;
    int centerY = image.height() / 2;
    
    painter.drawLine(centerX - 30, centerY, centerX + 30, centerY);
    painter.drawLine(centerX, centerY - 30, centerX, centerY + 30);
    
    // Add coordinate info (top left)
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 10, QFont::Bold));
    
    double ra_deg = m_telescopeState->ra * 180.0 / M_PI;
    double dec_deg = m_telescopeState->dec * 180.0 / M_PI;
    
    QString coordText = QString("RA: %1° Dec: %2°")
                       .arg(ra_deg, 0, 'f', 3)
                       .arg(dec_deg, 0, 'f', 3);
    
    painter.drawText(10, 20, coordText);
    
    // Add timestamp (bottom left)
    QString timeText = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss UTC");
    painter.drawText(10, image.height() - 25, timeText);
    
    // Add exposure info (bottom left, second line)
    QString exposureText = QString("ISO:%1 EXP:%2s BIN:%3x%3")
                          .arg(m_telescopeState->iso)
                          .arg(m_telescopeState->exposure, 0, 'f', 1)
                          .arg(m_telescopeState->binning);
    painter.drawText(10, image.height() - 10, exposureText);
    
    // Add "REAL HiPS DATA" label (top right)
    painter.setPen(Qt::green);
    painter.setFont(QFont("Arial", 8, QFont::Bold));
    painter.drawText(image.width() - 120, 20, "REAL HiPS DATA");
    
    // Add precision indicator (top right, second line)
    painter.setPen(Qt::cyan);
    painter.setFont(QFont("Arial", 7));
    painter.drawText(image.width() - 120, 35, "1.61\"/pixel precision");
    
    // Add frame number (bottom right)
    painter.setPen(Qt::white);
    painter.setFont(QFont("Arial", 8));
    QString frameText = QString("Frame %1").arg(m_telescopeState->imageCounter % 10);
    painter.drawText(image.width() - 80, image.height() - 10, frameText);
    
    // Add center marker with coordinate precision
    painter.setPen(QPen(Qt::red, 1));
    painter.drawEllipse(centerX - 2, centerY - 2, 4, 4);
    
    // Add FOV indicator (corner markers)
    painter.setPen(QPen(Qt::gray, 1));
    painter.drawLine(5, 5, 25, 5);      // Top left
    painter.drawLine(5, 5, 5, 25);
    painter.drawLine(image.width()-25, 5, image.width()-5, 5);  // Top right
    painter.drawLine(image.width()-5, 5, image.width()-5, 25);
    painter.drawLine(5, image.height()-25, 5, image.height()-5);  // Bottom left
    painter.drawLine(5, image.height()-5, 25, image.height()-5);
    painter.drawLine(image.width()-5, image.height()-25, image.width()-5, image.height()-5);  // Bottom right
    painter.drawLine(image.width()-25, image.height()-5, image.width()-5, image.height()-5);
}

// And call it in the constructor:
// CelestronOriginSimulator::CelestronOriginSimulator(QObject *parent) : QObject(parent) {
//     // ... existing initialization ...
//     setupHipsIntegration();
//     setupMosaicCreator();  // Add this line
//     // ... rest of initialization ...
// }

// ---- the mount model ----

void CelestronOriginSimulator::stepMount() {
    const double seconds = m_mountClock.restart() / 1000.0;
    MountModel& m = m_telescopeState->mount;
    const bool wasGoing = !m.isGotoOver();
    const double jd = TelescopeState::julianDateNow();
    m.step(std::min(seconds, 1.0), jd);
    for (const QString& e : m.takeEvents()) qWarning().noquote() << "MOUNT:" << e;
    if (m_exposureSampling) m_exposureSamples.push_back({ jd, m });
    // the true pointing, for the frames and the survey mosaic
    double ra, dec;
    m.trueJ2000(jd, ra, dec);
    m_telescopeState->ra = ra; m_telescopeState->dec = dec;
    m_telescopeState->isGotoOver = m.isGotoOver();
    m_telescopeState->isTracking = m.isTracking();
    if (wasGoing && m.isGotoOver()) {
        m_statusSender->sendMountStatusToAll();
        if (m_surveyFits && m_hipsClient) fetchHipsImagesForPosition({ ra * 180.0 / M_PI, dec * 180.0 / M_PI, "Goto_Target", "Position after a goto" });
    }
}

void CelestronOriginSimulator::handleSimRequest(QTcpSocket *socket, const QString &path) {
    const QUrl url(path);
    const double jd = TelescopeState::julianDateNow();
    if (url.path() == "/sim/slip") {
        // a knock, or the clutch slipping: the sky turns, the encoder does not
        m_telescopeState->mount.slipAzimuth(QUrlQuery(url).queryItemValue("degrees").toDouble());
    } else if (url.path() == "/sim/truth") {
        sendHttpResponse(socket, 200, "application/json", QJsonDocument(m_telescopeState->mount.truth(jd)).toJson(QJsonDocument::Compact));
        return;
    } else if (url.path() != "/sim/state") {
        sendHttpResponse(socket, 404, "text/plain", "Not Found");
        return;
    }
    QJsonObject state = m_telescopeState->mount.state(jd);
    state["ready"] = m_telescopeState->isReady;
    state["live_stream_disabled"] = m_telescopeState->liveStreamDisabled;
    state["gaia_stars"] = double(GaiaSky::instance().count());
    sendHttpResponse(socket, 200, "application/json", QJsonDocument(state).toJson(QJsonDocument::Compact));
}
