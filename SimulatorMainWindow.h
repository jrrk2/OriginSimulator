// SimulatorMainWindow.h — at-a-glance status + live controls for the
// OriginSimulator. Replaces the stderr-only CLI workflow: status fields
// are polled from the simulator every 250 ms, qDebug/qWarning output is
// routed exclusively to the in-window log pane (stderr is silenced).

#ifndef SIMULATORMAINWINDOW_H
#define SIMULATORMAINWINDOW_H

#include <QMainWindow>
#include <QTimer>

class QLabel;
class QSlider;
class QCheckBox;
class QDoubleSpinBox;
class QPushButton;
class QPlainTextEdit;
class CelestronOriginSimulator;

class SimulatorMainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit SimulatorMainWindow(CelestronOriginSimulator* sim,
                                 QWidget* parent = nullptr);
    ~SimulatorMainWindow() override;

    // Append a single log line. Called by the message handler installed in
    // main.cpp — bypasses Qt's normal stderr/qInstallMessageHandler route.
    // Q_INVOKABLE so QMetaObject::invokeMethod(..., "appendLog", ...) can
    // find it by name when marshalling cross-thread from worker threads.
    Q_INVOKABLE void appendLog(const QString& line);

private slots:
    void refreshStatus();         // QTimer-driven, ~4 Hz
    void onBortleChanged(int v);
    void onRayleighToggled(bool checked);
    void onAttenuationChanged(double v);
    void onHaltClicked();
    void onMinSeverityChanged(int idx);
    void onFilterToggled();       // any of the category checkboxes

private:
    CelestronOriginSimulator* m_sim = nullptr;

    QTimer m_pollTimer;

    // Status row labels (left = key, right = value).
    QLabel* m_lblState      = nullptr;
    QLabel* m_lblClients    = nullptr;
    QLabel* m_lblPointing   = nullptr;
    QLabel* m_lblAltAz      = nullptr;
    QLabel* m_lblSession    = nullptr;
    QLabel* m_lblFrames     = nullptr;
    QLabel* m_lblImageType  = nullptr;
    QLabel* m_lblDsoCount   = nullptr;

    // Controls.
    QSlider*        m_bortleSlider     = nullptr;
    QLabel*         m_bortleValueLabel = nullptr;
    QCheckBox*      m_rayleighCheck    = nullptr;
    QDoubleSpinBox* m_attenSpin        = nullptr;
    QPushButton*    m_haltButton       = nullptr;

    // Log filters.
    class QComboBox* m_minSeverityCombo = nullptr;
    QCheckBox*       m_showMountCheck   = nullptr;
    QCheckBox*       m_showHttpCheck    = nullptr;
    QCheckBox*       m_showWsCheck      = nullptr;

    int  m_minSeverity     = 0;      // 0=DBG, 1=INF, 2=WRN, 3=CRT, 4=FAT
    bool m_showStatusBcast = false;  // SEND [Notification] (Mount/Camera/...): default OFF — very repetitive
    bool m_showHttp        = false;  // "HTTP req:" / "  astro:" / "  image:" — default OFF
    bool m_showWsCommand   = true;   // SEND [Command] / <<< RECV / RESP: kept on (useful, lower volume)

    // Log pane.
    QPlainTextEdit* m_log = nullptr;

    void buildUi();

    // Decide whether a tagged message ("[DBG] …") should reach the pane,
    // given the current filter settings.
    bool passesFilter(const QString& taggedLine) const;
};

#endif // SIMULATORMAINWINDOW_H
