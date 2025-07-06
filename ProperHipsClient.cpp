// ProperHipsClient.cpp - Fixed version without QApplication include
#include "ProperHipsClient.h"
#include <QDebug>
#include <QNetworkRequest>
#include <QUrl>
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QTimer>
#include <cmath>

// In ProperHipsClient, add a method to find real neighbors
QList<long long> ProperHipsClient::getNeighboringPixels(long long centerPixel, int order) const {
    try {
        long long nside = 1LL << order;
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        
        // Get the actual neighbors
        fix_arr<int,8> neighbors;
        healpix.neighbors(centerPixel, neighbors);
        
        QList<long long> result;
        for (int i = 0; i < 8; i++) {
            if (neighbors[i] >= 0) {  // Valid neighbor
                result.append(neighbors[i]);
            }
        }
        return result;
    } catch (...) {
        return QList<long long>();
    }
}

// HEALPix neighbors are typically returned in this order:
// [0] = SW (Southwest)
// [1] = W  (West) 
// [2] = NW (Northwest)
// [3] = N  (North)
// [4] = NE (Northeast)
// [5] = E  (East)
// [6] = SE (Southeast)
// [7] = S  (South)

QMap<QString, long long> ProperHipsClient::getDirectionalNeighbors(long long centerPixel, int order) const {
    QMap<QString, long long> directionalNeighbors;
    
    try {
        long long nside = 1LL << order;
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        
        fix_arr<int,8> neighborArray;
        healpix.neighbors(centerPixel, neighborArray);
        
        // Standard HEALPix neighbor order (counter-clockwise from SW)
	// originally  QStringList directions = {"SW", "W", "NW", "N", "NE", "E", "SE", "S"};
        // manual based on M51 QStringList directions = {"N", "NE", "E", "SW", "SE", "S", "NW", "W"};
	QStringList directions = {"S", "SE", "E", "NE", "N", "NW", "W", "SW"};
        
        qDebug() << "Directional neighbors for pixel" << centerPixel << ":";
        for (int i = 0; i < 8; i++) {
            if (neighborArray[i] >= 0) {
                directionalNeighbors[directions[i]] = neighborArray[i];
                qDebug() << QString("  %1: %2").arg(directions[i]).arg(neighborArray[i]);
            } else {
                qDebug() << QString("  %1: NO NEIGHBOR").arg(directions[i]);
            }
        }
        
    } catch (const std::exception& e) {
        qDebug() << "HEALPix directional neighbors error:" << e.what();
    }
    
    return directionalNeighbors;
}

// Create proper 3x3 grid from directional neighbors
QList<QList<long long>> ProperHipsClient::createProper3x3Grid(long long centerPixel, int order) const {
    QMap<QString, long long> neighbors = getDirectionalNeighbors(centerPixel, order);
    
    // Map directions to 3x3 grid positions
    // Grid layout:
    // [NW] [N ] [NE]
    // [W ] [C ] [E ]  
    // [SW] [S ] [SE]
    
    QList<QList<long long>> grid = {
        // Bottom row: SW, S, SE
        {neighbors.value("SW", -1), neighbors.value("S", -1), neighbors.value("SE", -1)},
        // Middle row: W, Center, E  
        {neighbors.value("W", -1), centerPixel, neighbors.value("E", -1)},
        // Top row: NW, N, NE
        {neighbors.value("NW", -1), neighbors.value("N", -1), neighbors.value("NE", -1)}
    };
    
    return grid;
}

ProperHipsClient::ProperHipsClient(QObject *parent) 
    : QObject(parent), m_currentSurveyIndex(0), m_currentPositionIndex(0) {
    
    m_networkManager = new QNetworkAccessManager(this);
    m_testTimer = new QTimer(this);
    m_testTimer->setSingleShot(true);
    
    setupSurveys();
    setupTestPositions();
    
    qDebug() << "ProperHipsClient initialized with real HEALPix library";
    qDebug() << "Available surveys:" << m_surveys.keys();
}

void ProperHipsClient::setupSurveys() {
    // Working surveys based on your test results
    m_surveys["DSS2_Color"] = {
        "DSS2 Color",
        "http://alasky.u-strasbg.fr/DSS/DSSColor",
        "jpg",
        "Digital Sky Survey 2 Color - proven 100% success",
        true, 11, {"full_sky"}
    };
    
    m_surveys["2MASS_Color"] = {
        "2MASS Color",
        "http://alasky.u-strasbg.fr/2MASS/Color", 
        "jpg",
        "2MASS near-infrared color - proven 100% success",
        true, 9, {"full_sky"}
    };
    
    m_surveys["2MASS_J"] = {
        "2MASS J-band",
        "http://alasky.u-strasbg.fr/2MASS/J",
        "jpg", 
        "2MASS J-band (1.25 micron) - proven 100% success",
        true, 9, {"full_sky"}
    };
    
    // Test additional surveys with proper HEALPix
    m_surveys["DSS2_Red"] = {
        "DSS2 Red",
        "http://alasky.u-strasbg.fr/DSS/DSS2-red",
        "jpg",
        "DSS2 red band",
        true, 11, {"full_sky"}
    };
    
    m_surveys["Gaia_DR3"] = {
        "Gaia DR3", 
        "http://alasky.u-strasbg.fr/Gaia/Gaia-DR3",
        "png",
        "Gaia Data Release 3",
        true, 13, {"full_sky"}
    };
    
    m_surveys["SDSS_DR12"] = {
        "SDSS DR12",
        "http://alasky.u-strasbg.fr/SDSS/DR12/color", 
        "jpg",
        "Sloan Digital Sky Survey DR12",
        true, 12, {"northern_sky"}
    };
    
    m_surveys["Mellinger_Color"] = {
        "Mellinger Color",
        "http://alasky.u-strasbg.fr/Mellinger/Mellinger_color",
        "jpg",
        "Mellinger all-sky optical mosaic", 
        true, 8, {"full_sky"}
    };
    
    // Rubin Observatory (may need different URL patterns)
    m_surveys["Rubin_Virgo_Color"] = {
        "Rubin Virgo Color",
        "https://images.rubinobservatory.org/hips/SVImages_v2/color_ugri",
        "webp",
        "Rubin Observatory Virgo Cluster",
        true, 12, {"virgo_cluster"}
    };
}

void ProperHipsClient::setupTestPositions() {
    m_testPositions = {
        // High-priority test positions
        {83.0, -5.4, "Orion", "Orion Nebula region - should have data everywhere"},
        {266.4, -29.0, "Galactic_Center", "Sagittarius A* region"},
        {186.25, 12.95, "Virgo_Center", "Center of Virgo galaxy cluster"},
        {210.0, 54.0, "Ursa_Major", "Big Dipper region"},
        
        // Edge cases
        {0.0, 0.0, "Equator_0h", "Celestial equator"},
        {180.0, 0.0, "Equator_12h", "Opposite side of sky"},
        
        // Additional test points
        {23.46, 30.66, "Andromeda", "M31 galaxy region"},
        {201.0, -43.0, "Centaurus", "Centaurus constellation"}
    };
}

void ProperHipsClient::testPixelCalculation() {
    qDebug() << "=== Testing Real HEALPix Pixel Calculation ===";
    
    SkyPosition orion = {83.0, -5.4, "Orion", "Test position"};
    
    // Test different orders
    for (int order = 3; order <= 10; order++) {
        long long realPixel = calculateHealPixel(orion, order);
        long long simplePixel = calculateSimplePixel(orion.ra_deg, orion.dec_deg, order);
        long long nside = 1LL << order;
        
        qDebug() << QString("Order %1: nside=%2, real_pixel=%3, simple_pixel=%4, diff=%5")
                    .arg(order).arg(nside).arg(realPixel).arg(simplePixel).arg(realPixel - simplePixel);
        
        // Build test URLs for DSS (known working survey)
        QString realUrl = buildDSSUrl(orion, order, "DSS2_Color");
        qDebug() << "  Real HEALPix URL:" << realUrl;
    }
    
    qDebug() << "\nThis shows the difference between simple and real HEALPix calculations!";
}

long long ProperHipsClient::calculateHealPixel(const SkyPosition& position, int order) const {
    try {
        long long nside = 1LL << order;
        Healpix_Base healpix(nside, NEST, SET_NSIDE);
        
        pointing pt = position.toPointing();
        long long pixel = healpix.ang2pix(pt);
        
        return pixel;
    } catch (const std::exception& e) {
        qDebug() << "HEALPix error:" << e.what();
        return -1;
    }
}

// Simplified tile grid - just return center pixel for now
QList<long long> ProperHipsClient::calculateTileGrid(const SkyPosition& center, int order, int) const {
    QList<long long> pixels;
    
    // For now, just return the center pixel
    // This avoids the neighbors() API issues
    long long centerPixel = calculateHealPixel(center, order);
    if (centerPixel >= 0) {
        pixels.append(centerPixel);
    }
    
    return pixels;
}

QString ProperHipsClient::buildDSSUrl(const SkyPosition& position, int order, const QString& survey) const {
    const HipsSurveyInfo& info = m_surveys[survey];
    long long pixel = calculateHealPixel(position, order);
    
    if (pixel < 0) return QString();
    
    // Standard HiPS URL format for DSS
    int dir = (pixel / 10000) * 10000;
    return QString("%1/Norder%2/Dir%3/Npix%4.%5")
           .arg(info.baseUrl)
           .arg(order)
           .arg(dir) 
           .arg(pixel)
           .arg(info.format);
}

QString ProperHipsClient::build2MASSUrl(const SkyPosition& position, int order, const QString& survey) const {
    // 2MASS uses same format as DSS
    return buildDSSUrl(position, order, survey);
}

QString ProperHipsClient::buildRubinUrl(const SkyPosition& position, int order, const QString& survey) const {
    const HipsSurveyInfo& info = m_surveys[survey];
    long long pixel = calculateHealPixel(position, order);
    
    if (pixel < 0) return QString();
    
    // Rubin may use different directory structure
    int dir = (pixel / 10000) * 10000;
    return QString("%1/Norder%2/Dir%3/Npix%4.%5")
           .arg(info.baseUrl)
           .arg(order)
           .arg(dir)
           .arg(pixel) 
           .arg(info.format);
}

QString ProperHipsClient::buildGenericHipsUrl(const QString& baseUrl, const QString& format,
                                              const SkyPosition& position, int order) const {
    long long pixel = calculateHealPixel(position, order);
    
    if (pixel < 0) return QString();
    
    int dir = (pixel / 10000) * 10000;
    return QString("%1/Norder%2/Dir%3/Npix%4.%5")
           .arg(baseUrl)
           .arg(order)
           .arg(dir)
           .arg(pixel)
           .arg(format);
}

QString ProperHipsClient::buildTileUrl(const QString& surveyName, const SkyPosition& position, int order) const {
    if (!m_surveys.contains(surveyName)) {
        return QString();
    }
    
    const HipsSurveyInfo& survey = m_surveys[surveyName];
    
    // Use appropriate URL builder based on survey type
    if (surveyName.startsWith("DSS") || surveyName.contains("Mellinger")) {
        return buildDSSUrl(position, order, surveyName);
    } else if (surveyName.startsWith("2MASS")) {
        return build2MASSUrl(position, order, surveyName); 
    } else if (surveyName.startsWith("Rubin")) {
        return buildRubinUrl(position, order, surveyName);
    } else {
        return buildGenericHipsUrl(survey.baseUrl, survey.format, position, order);
    }
}

void ProperHipsClient::testAllSurveys() {
    qDebug() << "=== Testing All Surveys with Real HEALPix ===";
    qDebug() << "Surveys:" << m_surveys.keys();
    qDebug() << "Positions:" << m_testPositions.size();
    
    m_results.clear();
    m_currentSurveyIndex = 0;
    m_currentPositionIndex = 0;
    
    startNextTest();
}

void ProperHipsClient::startNextTest() {
    if (m_currentSurveyIndex >= m_surveys.keys().size()) {
        finishTesting();
        return;
    }
    
    QString surveyName = m_surveys.keys()[m_currentSurveyIndex];
    const SkyPosition& position = m_testPositions[m_currentPositionIndex];
    
    // Build URL with real HEALPix
    QString url = buildTileUrl(surveyName, position, 6);  // Test with order 6
    
    if (url.isEmpty()) {
        qDebug() << "✗ Failed to build URL for" << surveyName << "@" << position.name;
        
        // Record failure
        TileResult result;
        result.survey = surveyName;
        result.position = position.name;
        result.success = false;
        result.httpStatus = 0;
        result.url = "URL_BUILD_FAILED";
        result.healpixPixel = -1;
        result.order = 6;
        result.timestamp = QDateTime::currentDateTime();
        m_results.append(result);
        
        moveToNextTest();
        return;
    }
    
    qDebug() << QString("Testing %1 @ %2").arg(surveyName).arg(position.name);
    qDebug() << "  URL:" << url;
    
    // Start download test
    QUrl targetUrl(url);
    QNetworkRequest request(targetUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "ProperHipsClient/1.0");
    request.setRawHeader("Accept", "image/*");
    
    m_requestStartTime = QDateTime::currentDateTime();
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Store test info in reply properties
    reply->setProperty("survey", surveyName);
    reply->setProperty("position", position.name);
    reply->setProperty("url", url);
    reply->setProperty("pixel", calculateHealPixel(position, 6));
    
    connect(reply, &QNetworkReply::finished, this, &ProperHipsClient::onReplyFinished);
    
    // Set timeout
    QTimer::singleShot(15000, reply, &QNetworkReply::abort);
}

void ProperHipsClient::testSurveyAtPosition(const QString& surveyName, const SkyPosition& position) {
    QString url = buildTileUrl(surveyName, position, 6);
    if (url.isEmpty()) {
        qDebug() << "Failed to build URL for" << surveyName << "at" << position.name;
        return;
    }
    
    qDebug() << "Testing" << surveyName << "at" << position.name;
    qDebug() << "URL:" << url;
    
    // Start download test
    QUrl targetUrl(url);
    QNetworkRequest request(targetUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader, "ProperHipsClient/1.0");
    request.setRawHeader("Accept", "image/*");
    
    m_requestStartTime = QDateTime::currentDateTime();
    QNetworkReply* reply = m_networkManager->get(request);
    
    // Store test info
    reply->setProperty("survey", surveyName);
    reply->setProperty("position", position.name);
    reply->setProperty("url", url);
    reply->setProperty("pixel", calculateHealPixel(position, 6));
    
    connect(reply, &QNetworkReply::finished, this, &ProperHipsClient::onReplyFinished);
    
    // Set timeout
    QTimer::singleShot(15000, reply, &QNetworkReply::abort);
}

void ProperHipsClient::onReplyFinished() {
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;
    
    // Extract test info
    QString surveyName = reply->property("survey").toString();
    QString positionName = reply->property("position").toString();
    QString url = reply->property("url").toString();
    long long pixel = reply->property("pixel").toLongLong();
    
    // Calculate results
    TileResult result;
    result.survey = surveyName;
    result.position = positionName;
    result.success = (reply->error() == QNetworkReply::NoError);
    result.httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    result.downloadTime = m_requestStartTime.msecsTo(QDateTime::currentDateTime());
    result.fileSize = reply->readAll().size();
    result.url = url;
    result.healpixPixel = pixel;
    result.order = 6;
    result.timestamp = QDateTime::currentDateTime();
    
    m_results.append(result);
    
    // Print immediate result
    QString status = result.success ? "✓" : "✗";
    qDebug() << QString("  %1 %2ms, %3 bytes, HTTP %4, pixel %5")
                .arg(status)
                .arg(result.downloadTime)
                .arg(result.fileSize)
                .arg(result.httpStatus)
                .arg(result.healpixPixel);
    
    // Signal removed - not needed for basic functionality
    // M51MosaicClient will handle its own progress tracking
    
    reply->deleteLater();
    moveToNextTest();
}

void ProperHipsClient::moveToNextTest() {
    m_currentPositionIndex++;
    if (m_currentPositionIndex >= m_testPositions.size()) {
        m_currentPositionIndex = 0;
        m_currentSurveyIndex++;
    }
    
    // Small delay between tests
    QTimer::singleShot(200, this, &ProperHipsClient::startNextTest);
}

void ProperHipsClient::finishTesting() {
    qDebug() << "\n=== Testing Complete ===";
    printSummary();
    saveResults("proper_hips_results.csv");
    
    // Don't quit the application here - let the caller decide
    qDebug() << "Testing finished. Results saved.";
    emit testingComplete();
}

void ProperHipsClient::printSummary() const {
    qDebug() << "\n=== PROPER HiPS RESULTS SUMMARY ===";
    
    // Group results by survey
    QMap<QString, QList<TileResult>> surveyResults;
    for (const TileResult& result : m_results) {
        surveyResults[result.survey].append(result);
    }
    
    qDebug() << QString("%-20s %8s %8s %8s %10s").arg("Survey").arg("Success").arg("Avg Time").arg("Avg Size").arg("Coverage");
    qDebug() << QString("%-20s %8s %8s %8s %10s").arg("--------").arg("-------").arg("--------").arg("--------").arg("--------");
    
    QStringList bestSurveys;
    
    for (auto it = surveyResults.begin(); it != surveyResults.end(); ++it) {
        QString survey = it.key();
        QList<TileResult> results = it.value();
        
        int successful = 0;
        qint64 totalTime = 0;
        qint64 totalSize = 0;
        
        for (const TileResult& result : results) {
            if (result.success) {
                successful++;
                totalTime += result.downloadTime;
                totalSize += result.fileSize;
            }
        }
        
        double successRate = double(successful) / double(results.size()) * 100.0;
        double avgTime = successful > 0 ? double(totalTime) / successful : 0;
        double avgSize = successful > 0 ? double(totalSize) / successful : 0;
        
        qDebug() << QString("%-20s %7.1f%% %7.0fms %7.0fkB %9.1f%%")
                    .arg(survey)
                    .arg(successRate)
                    .arg(avgTime)
                    .arg(avgSize / 1024.0)
                    .arg(successRate);
        
        if (successRate >= 90.0) {
            bestSurveys.append(survey);
        }
    }
    
    qDebug() << "\n=== RECOMMENDATIONS ===";
    if (!bestSurveys.isEmpty()) {
        qDebug() << "Best surveys (≥90% success):" << bestSurveys;
    } else {
        qDebug() << "No surveys achieved ≥90% success rate";
    }
    
    // Show pixel calculation comparison
    qDebug() << "\n=== HEALPix Pixel Comparison ===";
    if (!m_results.isEmpty()) {
        SkyPosition samplePos = {83.0, -5.4, "Orion", "Sample"};
        
        long long realPixel = calculateHealPixel(samplePos, 6);
        long long simplePixel = calculateSimplePixel(samplePos.ra_deg, samplePos.dec_deg, 6);
        
        qDebug() << "Real HEALPix pixel for Orion (order 6):" << realPixel;
        qDebug() << "Simple calculation pixel for Orion (order 6):" << simplePixel;
        qDebug() << "Difference:" << (realPixel - simplePixel);
        qDebug() << "This difference explains why some surveys failed before!";
    }
}

void ProperHipsClient::saveResults(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Failed to save results to" << filename;
        return;
    }
    
    QTextStream out(&file);
    out << "Survey,Position,Success,HTTP_Status,Time_ms,Size_bytes,HealPix_Pixel,Order,URL,Timestamp\n";
    
    for (const TileResult& result : m_results) {
        out << QString("%1,%2,%3,%4,%5,%6,%7,%8,\"%9\",%10\n")
               .arg(result.survey)
               .arg(result.position)
               .arg(result.success ? "TRUE" : "FALSE")
               .arg(result.httpStatus)
               .arg(result.downloadTime)
               .arg(result.fileSize)
               .arg(result.healpixPixel)
               .arg(result.order)
               .arg(result.url)
               .arg(result.timestamp.toString(Qt::ISODate));
    }
    
    file.close();
    qDebug() << "Results saved to:" << filename;
}

QStringList ProperHipsClient::getWorkingSurveys() const {
    QStringList working;
    QMap<QString, int> successCount;
    QMap<QString, int> totalCount;
    
    // Count successes per survey
    for (const TileResult& result : m_results) {
        totalCount[result.survey]++;
        if (result.success) {
            successCount[result.survey]++;
        }
    }
    
    // Return surveys with >80% success rate
    for (auto it = totalCount.begin(); it != totalCount.end(); ++it) {
        QString survey = it.key();
        int total = it.value();
        int success = successCount.value(survey, 0);
        
        if (total > 0 && (double(success) / total) > 0.8) {
            working.append(survey);
        }
    }
    
    return working;
}

QString ProperHipsClient::getBestSurveyForPosition(const SkyPosition&) const {
    // Simple strategy: return the first working survey
    // In a real implementation, you'd consider position-specific factors
    QStringList working = getWorkingSurveys();
    return working.isEmpty() ? QString() : working.first();
}

// Add the old simple calculation for comparison
long long ProperHipsClient::calculateSimplePixel(double ra_deg, double dec_deg, int order) const {
    long long nside = 1LL << order;
    int ra_bucket = static_cast<int>((ra_deg / 360.0) * nside) % nside;
    int dec_bucket = static_cast<int>(((dec_deg + 90.0) / 180.0) * nside) % nside;
    long long pixel = dec_bucket * nside + ra_bucket;
    long long max_pixels = 12LL * nside * nside;
    return pixel % max_pixels;
}
