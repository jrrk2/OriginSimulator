// SimulatorMainWindow.cpp — see header.

#include "SimulatorMainWindow.h"

#include "CelestronOriginSimulator.h"
#include "TelescopeState.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateTime>
#include <QDoubleSpinBox>
#include <QFontDatabase>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QSlider>
#include <QVBoxLayout>
#include <QWidget>

#include <cmath>

namespace {

// Format radians → "HHhMMmSS.Ss" for RA.
QString fmtRaHMS(double raRad)
{
    double hours = raRad * 12.0 / M_PI;
    while (hours < 0)   hours += 24.0;
    while (hours >= 24) hours -= 24.0;
    const int h = int(hours);
    const double mFrac = (hours - h) * 60.0;
    const int m = int(mFrac);
    const double s = (mFrac - m) * 60.0;
    return QString("%1h%2m%3s").arg(h, 2, 10, QChar('0'))
                               .arg(m, 2, 10, QChar('0'))
                               .arg(s, 4, 'f', 1, QChar('0'));
}

// Format radians → "±DD°MM'SS\""
QString fmtDecDMS(double decRad)
{
    double deg = decRad * 180.0 / M_PI;
    const QChar sign = (deg < 0) ? '-' : '+';
    deg = std::fabs(deg);
    const int d = int(deg);
    const double mFrac = (deg - d) * 60.0;
    const int m = int(mFrac);
    const double s = (mFrac - m) * 60.0;
    return QString("%1%2°%3'%4\"")
            .arg(sign).arg(d, 2, 10, QChar('0'))
                      .arg(m, 2, 10, QChar('0'))
                      .arg(s, 4, 'f', 1, QChar('0'));
}

// Format radians → "DDD.D°" for altitude/azimuth (compact). The literal
// degree glyph is multi-byte UTF-8 so it doesn't fit in a plain char
// constant — use the Unicode code point directly.
QString fmtDeg(double rad)
{
    return QString::number(rad * 180.0 / M_PI, 'f', 1) + QChar(0x00B0);
}

} // anon

SimulatorMainWindow::SimulatorMainWindow(CelestronOriginSimulator* sim, QWidget* parent)
    : QMainWindow(parent), m_sim(sim)
{
    setWindowTitle("Celestron Origin Simulator");
    resize(720, 640);
    buildUi();

    // Seed control values from the live simulator state so the GUI reflects
    // whatever the CLI flags configured.
    if (m_sim) {
        m_bortleSlider->setValue(m_sim->bortleClass());
        m_bortleValueLabel->setText(QString::number(m_sim->bortleClass()));
        m_rayleighCheck->setChecked(m_sim->rayleighEnabled());
        m_attenSpin->setValue(m_sim->dsoAttenuation());
    }

    connect(&m_pollTimer, &QTimer::timeout, this, &SimulatorMainWindow::refreshStatus);
    m_pollTimer.start(250);
    refreshStatus();
}

SimulatorMainWindow::~SimulatorMainWindow() = default;

void SimulatorMainWindow::buildUi()
{
    auto* central = new QWidget(this);
    auto* root    = new QVBoxLayout(central);

    // ---------------- Status group ----------------
    auto* statusBox    = new QGroupBox("Simulator Status", central);
    auto* statusLayout = new QVBoxLayout(statusBox);

    auto makeRow = [](const QString& key, QLabel*& valueOut) {
        auto* row = new QHBoxLayout;
        auto* k = new QLabel(key);
        k->setMinimumWidth(110);
        valueOut = new QLabel("—");
        valueOut->setTextInteractionFlags(Qt::TextSelectableByMouse);
        row->addWidget(k);
        row->addWidget(valueOut, 1);
        return row;
    };
    statusLayout->addLayout(makeRow("State:",      m_lblState));
    statusLayout->addLayout(makeRow("Clients:",    m_lblClients));
    statusLayout->addLayout(makeRow("Pointing:",   m_lblPointing));
    statusLayout->addLayout(makeRow("Alt/Az:",     m_lblAltAz));
    statusLayout->addLayout(makeRow("Session:",    m_lblSession));
    statusLayout->addLayout(makeRow("Frames:",     m_lblFrames));
    statusLayout->addLayout(makeRow("Last image:", m_lblImageType));
    statusLayout->addLayout(makeRow("DSOs:",       m_lblDsoCount));

    // ---------------- Controls group ----------------
    auto* controlsBox    = new QGroupBox("Live Controls", central);
    auto* controlsLayout = new QVBoxLayout(controlsBox);

    // Bortle slider 1..9
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("Bortle 1–9:"));
        m_bortleSlider = new QSlider(Qt::Horizontal);
        m_bortleSlider->setRange(1, 9);
        m_bortleSlider->setTickPosition(QSlider::TicksBelow);
        m_bortleSlider->setTickInterval(1);
        m_bortleValueLabel = new QLabel("5");
        m_bortleValueLabel->setMinimumWidth(20);
        row->addWidget(m_bortleSlider, 1);
        row->addWidget(m_bortleValueLabel);
        controlsLayout->addLayout(row);
    }
    connect(m_bortleSlider, &QSlider::valueChanged,
            this, &SimulatorMainWindow::onBortleChanged);

    // Rayleigh on/off
    m_rayleighCheck = new QCheckBox("Rayleigh scattering (airmass-driven sky brightening)");
    controlsLayout->addWidget(m_rayleighCheck);
    connect(m_rayleighCheck, &QCheckBox::toggled,
            this, &SimulatorMainWindow::onRayleighToggled);

    // DSO attenuation
    {
        auto* row = new QHBoxLayout;
        row->addWidget(new QLabel("DSO attenuation:"));
        m_attenSpin = new QDoubleSpinBox;
        m_attenSpin->setRange(0.05, 10.0);
        m_attenSpin->setSingleStep(0.05);
        m_attenSpin->setDecimals(2);
        m_attenSpin->setValue(1.0);
        row->addWidget(m_attenSpin);
        row->addStretch();
        controlsLayout->addLayout(row);
    }
    connect(m_attenSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &SimulatorMainWindow::onAttenuationChanged);

    // Halt button
    {
        auto* row = new QHBoxLayout;
        m_haltButton = new QPushButton("Halt Imaging");
        row->addWidget(m_haltButton);
        row->addStretch();
        controlsLayout->addLayout(row);
    }
    connect(m_haltButton, &QPushButton::clicked,
            this, &SimulatorMainWindow::onHaltClicked);

    // ---------------- Log pane (with filters above) ----------------
    auto* logBox    = new QGroupBox("Log", central);
    auto* logLayout = new QVBoxLayout(logBox);

    {
        auto* filterRow = new QHBoxLayout;
        filterRow->addWidget(new QLabel("Min severity:"));
        m_minSeverityCombo = new QComboBox;
        m_minSeverityCombo->addItems(
            { "Debug+", "Info+", "Warning+", "Critical+", "Fatal only" });
        filterRow->addWidget(m_minSeverityCombo);
        filterRow->addSpacing(20);

        m_showStatusBcast = false;
        m_showHttp        = false;
        m_showWsCommand   = true;

        m_showWsCheck = new QCheckBox("WS commands");
        m_showWsCheck->setChecked(m_showWsCommand);
        m_showWsCheck->setToolTip(
            "SEND [Command] / RECV / RESP frames. Lower volume than "
            "notifications; useful to see App-driven actions.");
        filterRow->addWidget(m_showWsCheck);

        m_showHttpCheck = new QCheckBox("HTTP");
        m_showHttpCheck->setChecked(m_showHttp);
        m_showHttpCheck->setToolTip(
            "Per-request HTTP lines (Images/Temp/*.jpg, Astrophotography/...). "
            "Fires every time the App or PixInsight pulls a preview, so "
            "they pile up quickly.");
        filterRow->addWidget(m_showHttpCheck);

        m_showMountCheck = new QCheckBox("Status broadcasts");
        m_showMountCheck->setChecked(m_showStatusBcast);
        m_showMountCheck->setToolTip(
            "Unsolicited SEND [Notification] frames — Mount/GetStatus, "
            "Camera/GetCaptureParameters, DewHeater/GetStatus, "
            "Focuser/GetStatus etc. These broadcast every second and "
            "drown the log if left on.");
        filterRow->addWidget(m_showMountCheck);

        filterRow->addStretch();
        logLayout->addLayout(filterRow);
    }
    connect(m_minSeverityCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SimulatorMainWindow::onMinSeverityChanged);
    connect(m_showMountCheck, &QCheckBox::toggled, this, &SimulatorMainWindow::onFilterToggled);
    connect(m_showHttpCheck,  &QCheckBox::toggled, this, &SimulatorMainWindow::onFilterToggled);
    connect(m_showWsCheck,    &QCheckBox::toggled, this, &SimulatorMainWindow::onFilterToggled);

    m_log = new QPlainTextEdit;
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(2000);          // ring-buffer to avoid memory growth
    m_log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    logLayout->addWidget(m_log);

    root->addWidget(statusBox);
    root->addWidget(controlsBox);
    root->addWidget(logBox, 1);

    setCentralWidget(central);
}

// Classify a tagged line ("[DBG] ...") against the current filter settings.
// Returns true if the line should reach the visible log pane.
bool SimulatorMainWindow::passesFilter(const QString& line) const
{
    // ---- Severity gate ----
    // Tag is the first 5 chars: "[DBG] ", "[INF] ", etc. Anything that
    // doesn't conform is treated as Debug (most permissive).
    int sev = 0;
    if (line.startsWith(QLatin1String("[INF]"))) sev = 1;
    else if (line.startsWith(QLatin1String("[WRN]"))) sev = 2;
    else if (line.startsWith(QLatin1String("[CRT]"))) sev = 3;
    else if (line.startsWith(QLatin1String("[FAT]"))) sev = 4;
    if (sev < m_minSeverity) return false;

    // ---- Category gate ----
    // Warnings/errors always pass regardless of category (the user has
    // already opted in via severity; never silently hide a problem).
    if (sev >= 2) return true;

    // Status broadcasts: anything emitted by the simulator's StatusSender
    // as SEND [Notification]. Mount/GetStatus, Camera/GetCaptureParameters,
    // DewHeater/GetStatus, Focuser/GetStatus, Environment/GetStatus etc.
    if (line.contains(QLatin1String("SEND [Notification]")))
        return m_showStatusBcast;

    // WS commands: SEND [Command] / RECV / RESP frames.
    if (line.contains(QLatin1String("SEND [Command]")) ||
        line.contains(QLatin1String("RECV ["))         ||
        line.contains(QLatin1String("RESP ")))
        return m_showWsCommand;

    // HTTP per-request lines. These come from handleHttpImageRequest /
    // handleHttpAstroImageRequest and always lead with these prefixes.
    if (line.contains(QLatin1String("HTTP req:")) ||
        line.contains(QLatin1String("  astro:"))   ||
        line.contains(QLatin1String("  image:")))
        return m_showHttp;

    // Everything else (state changes, init steps, render notices, errors)
    // always shows.
    return true;
}

void SimulatorMainWindow::appendLog(const QString& line)
{
    if (!m_log) return;
    if (!passesFilter(line)) return;

    const QString ts = QDateTime::currentDateTime().toString("HH:mm:ss");
    const bool atBottom =
        m_log->verticalScrollBar()->value() == m_log->verticalScrollBar()->maximum();
    m_log->appendPlainText(ts + "  " + line);
    if (atBottom)
        m_log->verticalScrollBar()->setValue(m_log->verticalScrollBar()->maximum());
}

void SimulatorMainWindow::refreshStatus()
{
    if (!m_sim) return;
    const TelescopeState* st = m_sim->state();
    if (!st) return;

    m_lblState->setText(st->state);
    m_lblClients->setText(QString::number(m_sim->connectedClientCount()));
    m_lblPointing->setText(QString("RA %1   Dec %2")
                               .arg(fmtRaHMS(st->ra)).arg(fmtDecDMS(st->dec)));
    m_lblAltAz->setText(QString("Alt %1   Az %2")
                            .arg(fmtDeg(st->altitude)).arg(fmtDeg(st->azimuth)));

    if (st->imagingSessionDir.isEmpty())
        m_lblSession->setText("—");
    else
        m_lblSession->setText(st->imagingSessionDir);

    if (st->isImaging) {
        m_lblFrames->setText(QString("%1 captured (imaging in progress, %2s left)")
                                 .arg(st->stackDepth)
                                 .arg(st->imagingTimeLeft));
    } else {
        m_lblFrames->setText(QString("%1 captured (idle)").arg(st->stackDepth));
    }
    m_lblImageType->setText(m_sim->lastImageType().isEmpty() ? "—" : m_sim->lastImageType());
    m_lblDsoCount->setText(QString::number(m_sim->dsoCatalogueSize()));
}

void SimulatorMainWindow::onBortleChanged(int v)
{
    m_bortleValueLabel->setText(QString::number(v));
    if (m_sim) m_sim->setBortleClass(v);
}

void SimulatorMainWindow::onRayleighToggled(bool checked)
{
    if (m_sim) m_sim->setRayleighEnabled(checked);
}

void SimulatorMainWindow::onAttenuationChanged(double v)
{
    if (m_sim) m_sim->setDsoAttenuation(v);
}

void SimulatorMainWindow::onHaltClicked()
{
    if (m_sim) m_sim->guiHaltImaging();
    appendLog("[GUI] Halt Imaging clicked.");
}

void SimulatorMainWindow::onMinSeverityChanged(int idx)
{
    m_minSeverity = idx;
}

void SimulatorMainWindow::onFilterToggled()
{
    if (m_showMountCheck) m_showStatusBcast = m_showMountCheck->isChecked();
    if (m_showHttpCheck)  m_showHttp        = m_showHttpCheck->isChecked();
    if (m_showWsCheck)    m_showWsCommand   = m_showWsCheck->isChecked();
}
