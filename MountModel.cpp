#include "MountModel.h"

#include <QDebug>
#include <QJsonArray>
#include <QRandomGenerator>
#include <QtGlobal>

#include <libnova/julian_day.h>
#include <libnova/precession.h>
#include <libnova/transform.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr double D2R = M_PI / 180.0, R2D = 180.0 / M_PI;
constexpr double kMaxSpeed = 4.0;                 // deg/s at rate 9, also the firmware's goto and track speed

double norm360(double d) { d = std::fmod(d, 360.0); return d < 0 ? d + 360.0 : d; }
double wrap180(double d) { return std::remainder(d, 360.0); }

// measured 2026-11-07: degrees a second at each firmware rate
double rateSpeed(int rate)
{
    static const double s[10] = { 0, 0.002, 0.004, 0.01, 0.033, 0.068, 0.27, 0.97, 2.0, 4.0 };
    const int r = std::min(9, std::abs(rate));
    return (rate < 0 ? -1 : 1) * s[r];
}

void vec(double altDeg, double azDeg, double v[3])
{
    const double a = altDeg * D2R, z = azDeg * D2R;
    v[0] = std::cos(a) * std::cos(z); v[1] = std::cos(a) * std::sin(z); v[2] = std::sin(a);
}

void rotate(const double q[4], const double v[3], double out[3])
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    out[0] = (1 - 2 * (y * y + z * z)) * v[0] + 2 * (x * y - w * z) * v[1] + 2 * (x * z + w * y) * v[2];
    out[1] = 2 * (x * y + w * z) * v[0] + (1 - 2 * (x * x + z * z)) * v[1] + 2 * (y * z - w * x) * v[2];
    out[2] = 2 * (x * z - w * y) * v[0] + 2 * (y * z + w * x) * v[1] + (1 - 2 * (x * x + y * y)) * v[2];
}

void mul(const double a[4], const double b[4], double o[4])
{
    o[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    o[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    o[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    o[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

double envD(const char* name, double fallback)
{
    bool ok = false;
    const double v = qEnvironmentVariable(name).toDouble(&ok);
    return ok ? v : fallback;
}

bool envPair(const char* name, double& a, double& b)
{
    const QStringList p = qEnvironmentVariable(name).split(',');
    if (p.size() != 2) return false;
    bool ok1 = false, ok2 = false;
    const double x = p[0].toDouble(&ok1), y = p[1].toDouble(&ok2);
    if (!ok1 || !ok2) return false;
    a = x; b = y;
    return true;
}

// horizon (north azimuth, degrees) <-> J2000 (radians) at the site, unrefracted; the firmware and the plate solves both use J2000
void horizonToJ2000(double altDeg, double azDeg, double latRad, double lonRad, double jd, double& raRad, double& decRad)
{
    ln_lnlat_posn obs{ lonRad * R2D, latRad * R2D };
    ln_hrz_posn h{ norm360(azDeg - 180.0), altDeg };          // libnova measures azimuth from the south
    ln_equ_posn ofDate, j2000;
    ln_get_equ_from_hrz(&h, &obs, jd, &ofDate);
    ln_get_equ_prec2(&ofDate, jd, JD2000, &j2000);
    raRad = norm360(j2000.ra) * D2R; decRad = j2000.dec * D2R;
}

void j2000ToHorizon(double raRad, double decRad, double latRad, double lonRad, double jd, double& altDeg, double& azDeg)
{
    ln_lnlat_posn obs{ lonRad * R2D, latRad * R2D };
    ln_equ_posn j2000{ raRad * R2D, decRad * R2D }, ofDate;
    ln_get_equ_prec2(&j2000, JD2000, jd, &ofDate);
    ln_hrz_posn h;
    ln_get_hrz_from_equ(&ofDate, &obs, jd, &h);
    altDeg = h.alt; azDeg = norm360(h.az + 180.0);
}

}   // namespace

MountModel::Config MountModel::Config::fromEnvironment()
{
    Config c;
    // A secret set-up, as a telescope put down in the dark on uneven ground: a level error up to 10 degrees toward any side, the azimuth encoder's zero
    // anywhere, a plausible altitude zero, flexure and tilt-sensor offset. ORIGIN_SIM_SEED repeats one; ORIGIN_SIM_SECRET=0 keeps the 2026-11-08 mount;
    // any of the variables below sets that part. Revealed only by GET /sim/truth, for scoring a calibration after it has finished.
    if (qEnvironmentVariable("ORIGIN_SIM_SECRET", "1") != "0") {
        bool ok = false;
        const quint32 seed = qEnvironmentVariable("ORIGIN_SIM_SEED").toUInt(&ok);
        QRandomGenerator r = ok ? QRandomGenerator(seed) : QRandomGenerator::securelySeeded();
        c.tiltDeg = 10.0 * std::sqrt(r.generateDouble());        // up to 10, more often large than small (uniform over the cap)
        c.tiltTowardDeg = 360.0 * r.generateDouble();
        c.azimuthZeroDeg = 360.0 * r.generateDouble();
        c.altitudeZeroDeg = 40.0 + 7.0 * r.generateDouble();
        c.flexureDeg = -0.8 * r.generateDouble();
        c.tiltSensorBiasDeg = -2.0 + 4.0 * r.generateDouble();
        c.peAzArcsec = 5 + 25 * r.generateDouble();
        c.peAltArcsec = 5 + 25 * r.generateDouble();
        c.pePhaseAzDeg = 360 * r.generateDouble(); c.pePhaseAltDeg = 360 * r.generateDouble();
        c.pe2PhaseAzDeg = 360 * r.generateDouble(); c.pe2PhaseAltDeg = 360 * r.generateDouble();
        c.coneArcmin = -6 + 12 * r.generateDouble();
    }
    envPair("ORIGIN_SIM_PE", c.peAzArcsec, c.peAltArcsec);                 // "azimuth,altitude" arcseconds
    c.wormTeeth = envD("ORIGIN_SIM_WORM_TEETH", c.wormTeeth);
    c.coneArcmin = envD("ORIGIN_SIM_CONE", c.coneArcmin);
    c.refraction = qEnvironmentVariable("ORIGIN_SIM_REFRACTION", "1") != "0";
    c.pressureHPa = envD("ORIGIN_SIM_PRESSURE", c.pressureHPa);
    c.temperatureC = envD("ORIGIN_SIM_TEMPERATURE", c.temperatureC);
    c.settleSeconds = envD("ORIGIN_SIM_SETTLE", c.settleSeconds);
    envPair("ORIGIN_SIM_TILT", c.tiltDeg, c.tiltTowardDeg);                 // "degrees,toward azimuth"
    c.azimuthZeroDeg = envD("ORIGIN_SIM_AZ_ZERO", c.azimuthZeroDeg);
    c.altitudeZeroDeg = envD("ORIGIN_SIM_ALT_ZERO", c.altitudeZeroDeg);
    c.flexureDeg = envD("ORIGIN_SIM_FLEXURE", c.flexureDeg);
    c.tiltSensorBiasDeg = envD("ORIGIN_SIM_TILT_SENSOR_BIAS", c.tiltSensorBiasDeg);
    envPair("ORIGIN_SIM_START", c.startEnc0Deg, c.startEnc1Deg);           // "enc0,enc1" degrees
    c.cableNeutralEnc0Deg = envD("ORIGIN_SIM_CABLE_NEUTRAL", c.startEnc0Deg);
    c.cordBreakDeg = envD("ORIGIN_SIM_CORD_BREAK", c.cordBreakDeg);
    c.slipRatio = envD("ORIGIN_SIM_SLIP_RATIO", c.slipRatio);
    return c;
}

MountModel::MountModel(const Config& c) : m_cfg(c)
{
    // the base's tilt: a rotation about the horizontal axis at right angles to the low side's direction, after the azimuth zero about the vertical
    const double t = c.tiltTowardDeg * D2R, h = c.tiltDeg * D2R / 2, z = c.azimuthZeroDeg * D2R / 2;
    const double tilt[4] = { std::cos(h), -std::sin(t) * std::sin(h), std::cos(t) * std::sin(h), 0 };
    const double zero[4] = { std::cos(z), 0, 0, std::sin(z) };
    mul(tilt, zero, m_q);
    m_lat = 52.2054 * D2R; m_lon = 0.1218 * D2R;
    m_enc0 = c.startEnc0Deg; m_enc1 = c.startEnc1Deg;
    m_maxTurned = m_minTurned = turnedDeg();
}

double MountModel::enc0Deg() const { return wrap180(m_enc0); }

double MountModel::periodicError(double axisDeg, double amplitudeArcsec, double phase1Deg, double phase2Deg) const
{
    const double worm = axisDeg * m_cfg.wormTeeth * D2R;      // the worm's phase: once round per tooth
    return amplitudeArcsec / 3600.0 * (std::sin(worm + phase1Deg * D2R) + 0.3 * std::sin(2 * worm + phase2Deg * D2R));
}

void MountModel::tubeVector(double enc0, double enc1, double v[3]) const
{
    // axes from the encoders through the worms; flexure in altitude; the cone error sideways; then the base's rotation
    const double A = (enc0 + periodicError(enc0, m_cfg.peAzArcsec, m_cfg.pePhaseAzDeg, m_cfg.pe2PhaseAzDeg) - m_skyAzSlip) * D2R;
    const double a = enc1 + periodicError(enc1, m_cfg.peAltArcsec, m_cfg.pePhaseAltDeg, m_cfg.pe2PhaseAltDeg) + m_cfg.altitudeZeroDeg;
    const double E = (a - m_cfg.flexureDeg * std::cos(a * D2R)) * D2R, C = m_cfg.coneArcmin / 60.0 * D2R;
    const double b[3] = { std::cos(C) * std::cos(E), std::sin(C), std::cos(C) * std::sin(E) };
    const double m[3] = { b[0] * std::cos(A) - b[1] * std::sin(A), b[0] * std::sin(A) + b[1] * std::cos(A), b[2] };
    rotate(m_q, m, v);
}

double MountModel::refraction(double trueAltDeg) const
{
    if (!m_cfg.refraction || trueAltDeg < -2) return 0;
    // Saemundsson (1986), for the geometric altitude; within 0.1' of the tables above 15 degrees
    const double r = 1.02 / std::tan((trueAltDeg + 10.3 / (trueAltDeg + 5.11)) * D2R) / 60.0;
    return r * (m_cfg.pressureHPa / 1010.0) * (283.0 / (273.0 + m_cfg.temperatureC));
}

void MountModel::sky(double enc0, double enc1, double& altDeg, double& azDeg) const
{
    // where the tube points on the apparent sky
    double t[3];
    tubeVector(enc0, enc1, t);
    altDeg = std::asin(std::clamp(t[2], -1.0, 1.0)) * R2D;
    azDeg = norm360(std::atan2(t[1], t[0]) * R2D);
}

void MountModel::apparentDirection(double raRad, double decRad, double jd, double v[3]) const
{
    double alt, az;
    j2000ToHorizon(raRad, decRad, m_lat, m_lon, jd, alt, az);
    vec(alt + refraction(alt), az, v);
}

void MountModel::imageBasis(double boresight[3], double up[3], double right[3]) const
{
    // up: toward a slightly higher altitude encoder, made square to the boresight; right = up x boresight (west, looking south with up at the zenith)
    double hi[3];
    tubeVector(m_enc0, m_enc1, boresight);
    tubeVector(m_enc0, m_enc1 + 0.01, hi);
    const double d = hi[0] * boresight[0] + hi[1] * boresight[1] + hi[2] * boresight[2];
    double n = 0;
    for (int k = 0; k < 3; ++k) { up[k] = hi[k] - d * boresight[k]; n += up[k] * up[k]; }
    n = std::sqrt(n);
    for (int k = 0; k < 3; ++k) up[k] /= n;
    right[0] = up[1] * boresight[2] - up[2] * boresight[1];
    right[1] = up[2] * boresight[0] - up[0] * boresight[2];
    right[2] = up[0] * boresight[1] - up[1] * boresight[0];
}

void MountModel::starShape(double& elongation, double& angleDeg) const
{
    const double f = m_sinceStop < m_cfg.settleSeconds ? 1.0 - m_sinceStop / m_cfg.settleSeconds : 0.0;
    elongation = 1.0 + (m_cfg.settleElongation - 1.0) * f * f;           // fades fast, then lingers a little
    // the last motion, mostly in azimuth or in altitude: azimuth moves the image sideways, altitude up and down
    angleDeg = std::atan2(m_lastMove1, m_lastMove0 * std::cos((m_enc1 + m_cfg.altitudeZeroDeg) * D2R)) * R2D;
}

void MountModel::trueAltAz(double& altDeg, double& azDeg) const { sky(m_enc0, m_enc1, altDeg, azDeg); }

void MountModel::trueJ2000(double jd, double& raRad, double& decRad) const
{
    // the tube points at the apparent sky; the stars there came from lower down by the refraction (one step is enough at the arcsecond level)
    double alt, az;
    trueAltAz(alt, az);
    double geometric = alt - refraction(alt);
    geometric = alt - refraction(geometric);
    horizonToJ2000(geometric, az, m_lat, m_lon, jd, raRad, decRad);
}

double MountModel::tiltSensorDeg() const
{
    double alt, az;
    trueAltAz(alt, az);
    return alt + m_cfg.tiltSensorBiasDeg;
}

void MountModel::firmwareAltAz(double& altDeg, double& azDeg) const
{
    altDeg = m_enc1 + m_altOffset;
    azDeg = norm360(m_enc0 + m_azOffset);
}

void MountModel::firmwareJ2000(double jd, double& raRad, double& decRad) const
{
    double alt, az;
    firmwareAltAz(alt, az);
    horizonToJ2000(alt, az, m_lat, m_lon, jd, raRad, decRad);
}

void MountModel::firmwareTargetEncoders(double raRad, double decRad, double jd, double& enc0, double& enc1) const
{
    double alt, az;
    j2000ToHorizon(raRad, decRad, m_lat, m_lon, jd, alt, az);
    enc1 = alt - m_altOffset;
    // the firmware turns the short way round, whatever the cables
    enc0 = m_enc0 + wrap180(az - m_azOffset - m_enc0);
}

void MountModel::initialise(bool fake, double jd)
{
    Q_UNUSED(jd);
    double alt, az;
    trueAltAz(alt, az);
    if (fake) {
        // declared: azimuth 180 here, and the tilt sensor's altitude
        m_altOffset = tiltSensorDeg() - m_enc1;
        m_azOffset = 180.0 - m_enc0;
    } else {
        // a real alignment finds where it points here, and then assumes a level base everywhere
        m_altOffset = alt - m_enc1;
        m_azOffset = az - m_enc0;
    }
    m_azOffset = wrap180(m_azOffset);
    m_initialised = true;
    m_fake = fake;
    m_mode = Mode::Idle;
}

void MountModel::setRates(int altRate, int azmRate)
{
    m_altRate = std::clamp(altRate, -9, 9);
    m_azmRate = std::clamp(azmRate, -9, 9);
    if (m_altRate != 0 || m_azmRate != 0) m_mode = Mode::Manual;
    else if (m_mode == Mode::Manual) m_mode = Mode::Idle;
}

bool MountModel::gotoJ2000(double raRad, double decRad, double jd)
{
    if (!m_initialised) return false;
    firmwareTargetEncoders(raRad, decRad, jd, m_gotoEnc0, m_gotoEnc1);
    m_gotoRa = raRad; m_gotoDec = decRad;
    m_altRate = m_azmRate = 0;
    m_mode = Mode::Goto;
    return true;
}

bool MountModel::track(const std::vector<TrackPoint>& points)
{
    if (!m_initialised || points.empty()) return false;
    m_track = points;
    std::sort(m_track.begin(), m_track.end(), [](const TrackPoint& a, const TrackPoint& b) { return a.jd < b.jd; });
    m_altRate = m_azmRate = 0;
    m_mode = Mode::Track;
    return true;
}

void MountModel::halt()
{
    m_altRate = m_azmRate = 0;
    m_mode = Mode::Idle;
}

void MountModel::slipAzimuth(double degrees)
{
    m_skyAzSlip -= degrees;
    m_events << QString("azimuth slipped %1 degrees against its encoder").arg(degrees, 0, 'f', 2);
}

void MountModel::moveEnc0(double delta)
{
    m_enc0 += delta;
    if (m_snapped) m_skyAzSlip += (1.0 - m_cfg.slipRatio) * delta;        // the clutch slips: the sky turns less than the encoder
    const double t = turnedDeg();
    m_maxTurned = std::max(m_maxTurned, t);
    m_minTurned = std::min(m_minTurned, t);
    if (std::fabs(t) > m_cfg.cordTautDeg && !m_taut) {
        m_taut = true; ++m_tautCount;
        m_events << QString("CORD WRAP: azimuth %1 degrees from cable-neutral, past the %2 degree limit").arg(t, 0, 'f', 1).arg(m_cfg.cordTautDeg, 0, 'f', 0);
    } else if (std::fabs(t) < m_cfg.cordTautDeg - 5 && m_taut) {
        m_taut = false;
        m_events << QString("cord slack again at %1 degrees").arg(t, 0, 'f', 1);
    }
    if (std::fabs(t) > m_cfg.cordBreakDeg && !m_snapped) {
        m_snapped = true;
        m_events << QString("CORD SNAPPED at %1 degrees from cable-neutral: external power lost, the azimuth clutch now slips (ratio %2)").arg(t, 0, 'f', 1).arg(m_cfg.slipRatio);
    }
}

void MountModel::moveAxes(double target0, double target1, double maxSpeed, double seconds)
{
    const double limit = maxSpeed * seconds;
    const double d1 = std::clamp(target1 - m_enc1, -limit, limit);
    const double d0 = std::clamp(target0 - m_enc0, -limit, limit);
    m_enc1 += d1;
    moveEnc0(d0);
}

void MountModel::step(double seconds, double jd)
{
    if (seconds <= 0) return;
    m_prevEnc0 = m_enc0; m_prevEnc1 = m_enc1;
    switch (m_mode) {
    case Mode::Manual:
        m_enc1 += rateSpeed(m_altRate) * seconds;
        moveEnc0(rateSpeed(m_azmRate) * seconds);
        break;
    case Mode::Goto:
        firmwareTargetEncoders(m_gotoRa, m_gotoDec, jd, m_gotoEnc0, m_gotoEnc1);     // the target moves with the sky during the slew
        moveAxes(m_gotoEnc0, m_gotoEnc1, kMaxSpeed, seconds);
        if (std::fabs(m_gotoEnc0 - m_enc0) < 1e-4 && std::fabs(m_gotoEnc1 - m_enc1) < 1e-4) {
            // arrived: the firmware tracks the target from here, as the real one does after a goto, until halted or sent elsewhere
            m_track = { { m_gotoRa, m_gotoDec, jd + 1e6 } };
            m_mode = Mode::Track;
        }
        break;
    case Mode::Track: {
        if (jd > m_track.back().jd) { m_mode = Mode::Idle; break; }
        // the target now, interpolated along the track (held at the first point until it starts)
        TrackPoint p = m_track.front();
        for (size_t i = 1; i < m_track.size(); ++i) {
            if (jd <= m_track[i].jd) {
                const TrackPoint& a = m_track[i - 1]; const TrackPoint& b = m_track[i];
                const double f = jd <= a.jd ? 0 : (jd - a.jd) / (b.jd - a.jd);
                p.raRad = a.raRad + f * std::remainder(b.raRad - a.raRad, 2 * M_PI);
                p.decRad = a.decRad + f * (b.decRad - a.decRad);
                break;
            }
        }
        double e0, e1;
        firmwareTargetEncoders(p.raRad, p.decRad, jd, e0, e1);
        moveAxes(e0, e1, kMaxSpeed, seconds);
        break;
    }
    case Mode::Idle:
        break;
    }
    // the tube stops at the horizon and at the zenith
    const double a = m_enc1 + m_cfg.altitudeZeroDeg;
    if (a < -5) m_enc1 = -5 - m_cfg.altitudeZeroDeg;
    if (a > 90) m_enc1 = 90 - m_cfg.altitudeZeroDeg;
    // settling: a slew is anything faster than tracking; the clock restarts when it stops
    const double d0 = m_enc0 - m_prevEnc0, d1 = m_enc1 - m_prevEnc1;
    if (std::hypot(d0, d1) / seconds > 0.05) { m_sinceStop = 0; m_lastMove0 = d0; m_lastMove1 = d1; }
    else m_sinceStop += seconds;
}

QStringList MountModel::takeEvents()
{
    QStringList e = m_events;
    m_events.clear();
    return e;
}

QJsonObject MountModel::state(double jd) const
{
    // what someone at the telescope could see or read: the encoders, the firmware's frame, the tilt sensor, and the cables
    double falt, faz;
    firmwareAltAz(falt, faz);
    static const char* modes[] = { "IDLE", "MANUAL", "GOTO", "TRACK" };
    QJsonObject cord{ { "neutral_enc0_deg", m_cfg.cableNeutralEnc0Deg }, { "turned_deg", turnedDeg() }, { "max_turned_deg", m_maxTurned }, { "min_turned_deg", m_minTurned },
                      { "taut_limit_deg", m_cfg.cordTautDeg }, { "break_limit_deg", m_cfg.cordBreakDeg }, { "taut", m_taut }, { "times_taut", m_tautCount }, { "snapped", m_snapped } };
    QJsonObject firmware{ { "initialised", m_initialised }, { "fake", m_fake }, { "alt_deg", falt }, { "az_deg", faz } };
    return QJsonObject{
        { "enc0_deg", enc0Deg() }, { "enc0_unwrapped_deg", m_enc0 }, { "enc1_deg", m_enc1 }, { "mode", modes[int(m_mode)] }, { "tilt_sensor_deg", tiltSensorDeg() },
        { "latitude_deg", m_lat * R2D }, { "longitude_deg", m_lon * R2D }, { "jd", jd }, { "cord", cord }, { "firmware", firmware } };
}

QJsonObject MountModel::truth(double jd) const
{
    // the secret: for scoring a calibration once it has finished
    double alt, az, ra, dec;
    trueAltAz(alt, az);
    trueJ2000(jd, ra, dec);
    return QJsonObject{
        { "tilt_deg", m_cfg.tiltDeg }, { "tilt_toward_deg", m_cfg.tiltTowardDeg }, { "azimuth_zero_deg", m_cfg.azimuthZeroDeg },
        { "altitude_zero_deg", m_cfg.altitudeZeroDeg }, { "flexure_deg", m_cfg.flexureDeg }, { "tilt_sensor_bias_deg", m_cfg.tiltSensorBiasDeg },
        { "quaternion", QJsonArray{ m_q[0], m_q[1], m_q[2], m_q[3] } }, { "sky_azimuth_slip_deg", -m_skyAzSlip },
        { "firmware_altitude_offset_deg", m_altOffset }, { "firmware_azimuth_offset_deg", m_azOffset },
        { "periodic_error_arcsec", QJsonArray{ m_cfg.peAzArcsec, m_cfg.peAltArcsec } }, { "worm_teeth", m_cfg.wormTeeth }, { "cone_arcmin", m_cfg.coneArcmin },
        { "refraction", m_cfg.refraction }, { "pressure_hpa", m_cfg.pressureHPa }, { "temperature_c", m_cfg.temperatureC },
        { "refraction_here_arcmin", refraction(alt) * 60 }, { "seconds_since_slew", m_sinceStop },
        { "apparent_alt_deg", alt }, { "true_az_deg", az }, { "true_ra_deg", ra * R2D }, { "true_dec_deg", dec * R2D }, { "jd", jd } };
}
