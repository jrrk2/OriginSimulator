// ProperHipsClient.h - Using real HEALPix library for accurate pixel calculations
#ifndef PROPERHIPSCLIENT_H
#define PROPERHIPSCLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>
#include <QDateTime>
#include <QList>
#include <QStringList>
#include <QMap>
#include <QMutex>

// Real HEALPix includes
#include "healpix_base.h"
#include "pointing.h"

struct HipsSurveyInfo {
    QString name;
    QString baseUrl;
    QString format;
    QString description;
    bool available;
    int maxOrder;
    QStringList regions;
};

struct SkyPosition {
    double ra_deg;
    double dec_deg;
    QString name;
    QString description;
    
    // Convert to HEALPix pointing
    pointing toPointing() const {
        double theta = (90.0 - dec_deg) * M_PI / 180.0;  // colatitude
        double phi = ra_deg * M_PI / 180.0;              // longitude
        return pointing(theta, phi);
    }
};

struct TileResult {
    QString survey;
    QString position;
    bool success;
    int httpStatus;
    qint64 downloadTime;
    qint64 fileSize;
    QString url;
    long long healpixPixel;
    int order;
    QDateTime timestamp;
};

class ProperHipsClient : public QObject {
    Q_OBJECT

public:
    explicit ProperHipsClient(QObject *parent = nullptr);
    
    // Main testing interface
    void testAllSurveys();
    void testSurveyAtPosition(const QString& surveyName, const SkyPosition& position);
    void testPixelCalculation();
    
    // Production interface for telescope simulator
    QStringList getWorkingSurveys() const;
    QString getBestSurveyForPosition(const SkyPosition& position) const;
    QString buildTileUrl(const QString& surveyName, const SkyPosition& position, int order = 6) const;
    
    // Results access
    QList<TileResult> getResults() const { return m_results; }
    void saveResults(const QString& filename) const;
    void printSummary() const;

    long long calculateHealPixel(const SkyPosition& position, int order) const;
    QList<long long> getNeighboringPixels(long long centerPixel, int order) const;
    QMap<QString, long long> getDirectionalNeighbors(long long centerPixel, int order) const;
    QList<QList<long long>> createProper3x3Grid(long long centerPixel, int order) const;
										 
private slots:
    void onReplyFinished();

signals:
    void testingComplete();

private:
    QNetworkAccessManager* m_networkManager;
    QMap<QString, HipsSurveyInfo> m_surveys;
    QList<SkyPosition> m_testPositions;
    QList<TileResult> m_results;
    QTimer* m_testTimer;
    
    // Test state
    int m_currentSurveyIndex;
    int m_currentPositionIndex;
    QDateTime m_requestStartTime;
    
    void setupSurveys();
    void setupTestPositions();
    void startNextTest();
    void moveToNextTest();
    void finishTesting();
    
    // HEALPix utilities
    long long calculateSimplePixel(double ra_deg, double dec_deg, int order) const; // For comparison
    QList<long long> calculateTileGrid(const SkyPosition& center, int order, int gridSize = 4) const;
    
    // URL building for different survey types
    QString buildDSSUrl(const SkyPosition& position, int order, const QString& survey) const;
    QString build2MASSUrl(const SkyPosition& position, int order, const QString& survey) const;
    QString buildRubinUrl(const SkyPosition& position, int order, const QString& survey) const;
    QString buildGenericHipsUrl(const QString& baseUrl, const QString& format, 
                               const SkyPosition& position, int order) const;
};

#endif // PROPERHIPSCLIENT_H
