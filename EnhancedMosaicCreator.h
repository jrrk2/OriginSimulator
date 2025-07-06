// EnhancedMosaicCreator
#include <QApplication>
#include <QDebug>
#include <QTimer>
#include <QDir>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <QNetworkRequest>
#include <QPixmap>
#include <QImage>
#include <QPainter>
#include <QFile>
#include <QComboBox>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QTextEdit>
#include <QCheckBox>
#include <QLineEdit>
#include <QTabWidget>
#include <QFormLayout>
#include <QRegularExpression>
#include <QMessageBox>
#include <QKeyEvent>
#include <QFocusEvent>
#include <QScrollArea>
#include <QSplitter>
#include <QTextStream>
#include <cmath>
#include <limits>
#include "ProperHipsClient.h"

// Coordinate parser (same as original)
struct SimpleCoordinateParser {
    static SkyPosition parseCoordinates(const QString& raText, const QString& decText, 
                                      const QString& name = "Custom Target") {
        SkyPosition pos;
        pos.name = name;
        pos.description = "User-defined coordinates";
        pos.ra_deg = parseRA(raText);
        pos.dec_deg = parseDec(decText);
        return pos;
    }
    
private:
    static double parseRA(const QString& text) {
        QString clean = text.trimmed();
        
        if (clean.contains(':')) {
            QStringList parts = clean.split(':');
            if (parts.size() >= 2) {
                double hours = parts[0].toDouble();
                double minutes = parts[1].toDouble();
                double seconds = parts.size() > 2 ? parts[2].toDouble() : 0.0;
                return (hours + minutes/60.0 + seconds/3600.0) * 15.0;
            }
        }
        
        if (clean.contains('h')) {
            QRegularExpression re("(\\d+(?:\\.\\d+)?)h(?:(\\d+(?:\\.\\d+)?)m)?(?:(\\d+(?:\\.\\d+)?)s)?");
            QRegularExpressionMatch match = re.match(clean);
            if (match.hasMatch()) {
                double hours = match.captured(1).toDouble();
                double minutes = match.captured(2).isEmpty() ? 0.0 : match.captured(2).toDouble();
                double seconds = match.captured(3).isEmpty() ? 0.0 : match.captured(3).toDouble();
                return (hours + minutes/60.0 + seconds/3600.0) * 15.0;
            }
        }
        
        double degrees = clean.toDouble();
        if (degrees <= 24.0) {
            return degrees * 15.0;
        }
        return degrees;
    }
    
    static double parseDec(const QString& text) {
        QString clean = text.trimmed();
        bool negative = clean.startsWith('-');
        if (negative) clean = clean.mid(1);
        if (clean.startsWith('+')) clean = clean.mid(1);
        
        if (clean.contains(':')) {
            QStringList parts = clean.split(':');
            if (parts.size() >= 2) {
                double degrees = parts[0].toDouble();
                double minutes = parts[1].toDouble();
                double seconds = parts.size() > 2 ? parts[2].toDouble() : 0.0;
                double result = degrees + minutes/60.0 + seconds/3600.0;
                return negative ? -result : result;
            }
        }
        
        if (clean.contains('d')) {
            QRegularExpression re("(\\d+(?:\\.\\d+)?)d(?:(\\d+(?:\\.\\d+)?)m)?(?:(\\d+(?:\\.\\d+)?)s)?");
            QRegularExpressionMatch match = re.match(clean);
            if (match.hasMatch()) {
                double degrees = match.captured(1).toDouble();
                double minutes = match.captured(2).isEmpty() ? 0.0 : match.captured(2).toDouble();
                double seconds = match.captured(3).isEmpty() ? 0.0 : match.captured(3).toDouble();
                double result = degrees + minutes/60.0 + seconds/3600.0;
                return negative ? -result : result;
            }
        }
        
        double result = clean.toDouble();
        return negative ? -result : result;
    }
};

class EnhancedMosaicCreator : public QObject {  // CHANGED: Inherit from QObject instead of QWidget
    Q_OBJECT

public:
    explicit EnhancedMosaicCreator(QObject *parent = nullptr);  // CHANGED: Constructor signature

    // NEW: Public interface for external control
    void setCustomCoordinates(const QString& raText, const QString& decText, const QString& name);
    void createCustomMosaic(const SkyPosition& target);
    QImage getLastGeneratedMosaic() const { return m_fullMosaic; }

signals:
    void mosaicComplete(const QImage& mosaic);  // NEW: Signal for completion

private slots:
    void onTileDownloaded();
    void processNextTile();

private:
    ProperHipsClient* m_hipsClient;
    QNetworkAccessManager* m_networkManager;
    
    // Target tracking
    SkyPosition m_customTarget;
    SkyPosition m_actualTarget;
    QImage m_fullMosaic;
    
    // Tile structure
    struct SimpleTile {
        int gridX, gridY;
        long long healpixPixel;
        QString filename;
        QString url;
        QImage image;
        bool downloaded;
        SkyPosition skyCoordinates;
    };
    
    QList<SimpleTile> m_tiles;
    int m_currentTileIndex;
    QString m_outputDir;
    QDateTime m_downloadStartTime;
    
    // Core algorithms
    void createTileGrid(const SkyPosition& position);
    void downloadTile(int tileIndex);
    
    // Enhanced mosaic assembly
    void assembleFinalMosaicCentered();
    QPoint calculateTargetPixelPosition();
    QImage cropMosaicToCenter(const QImage& rawMosaic, const QPoint& targetPixel);
    
    // Helper functions
    void saveProgressReport(const QString& targetName);
    bool checkExistingTile(const SimpleTile& tile);
    bool isValidJpeg(const QString& filename);
    SkyPosition healpixToSkyPosition(long long pixel, int order) const;
    double calculateAngularDistance(const SkyPosition& pos1, const SkyPosition& pos2) const;
};
