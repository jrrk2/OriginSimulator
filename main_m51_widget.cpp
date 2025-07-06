// main_m51_widget.cpp - GUI version using M51MosaicClient widget
#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>

// Include our mosaic client
#include "M51MosaicClient.h"

class M51ControlPanel : public QWidget {
    Q_OBJECT

public:
    explicit M51ControlPanel(QWidget *parent = nullptr);

private slots:
    void onCreateMosaicClicked();
    void onSaveMosaicClicked();
    void onMosaicProgress(int completed, int total);
    void onMosaicComplete(const QImage& mosaic);
    void onTileDownloaded(int x, int y, const QString& survey);
    void onErrorOccurred(const QString& error);
    void updateConfiguration();

private:
    M51MosaicClient* m_mosaicClient;
    
    // Control widgets
    QSpinBox* m_widthSpinBox;