#ifndef MOUNTMODEL_H
#define MOUNTMODEL_H

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <vector>

/**
 * @brief The mount as a machine: two encoders, a base that need not be level, a tube that flexes, cables that wrap, and the firmware's idea of where it points.
 *
 * Everything that host-side calibration and cord-wrap protection have to cope with on a real Origin (measured 2026-11-07/08):
 *
 *  - Truth. Encoder readings give the mount's frame: azimuth Enc0, altitude a - F cos(a) with a = Enc1 + K (K the altitude encoder's zero, F the
 *    flexure at the horizon). A rotation takes that frame onto the true horizon (x north, y east, z up): the azimuth encoder's zero and the base's
 *    tilt (5 degrees toward azimuth 18 on grass). Frames are rendered at the true pointing, so plate solves see it.
 *  - The firmware's frame. After an initialisation the firmware reports altitude = Enc1 + altOffset and azimuth = Enc0 + azOffset: a level-base model.
 *    A fake initialisation declares the current pointing to be azimuth 180 at the tilt sensor's altitude; a real one uses the true pointing there. Its
 *    RA/Dec are J2000.
 *  - Motion. Slew rates move the encoders at the measured speeds (rate 9 about 4 deg/s). Firmware gotos and tracks drive the azimuth the shortest way
 *    round, as the firmware does, so only the host can keep the cables safe.
 *  - Cables. The running count of azimuth from cable-neutral: past 180 the cables are taut, and the log says so (cord-wrap detection).
 *  - Tilt sensor: the true altitude plus 1.15 degrees.
 *  - Periodic error: between each encoder and its axis, a sinusoid in phase with the worm (period 360/teeth degrees of axis), with a second harmonic.
 *  - Cone error: the optical axis not square to the altitude axis, a sideways offset C at every altitude (b = (cos C cos E, sin C, cos C sin E) before azimuth).
 *  - Refraction: stars are seen higher by Saemundsson's R(h) = 1.02' cot(h + 10.3/(h + 5.11)), scaled by pressure / 1010 hPa and 283 K / (273 K + T),
 *    star by star, so a low field is squashed vertically. The tube points at the apparent sky; the field centre's J2000 is where the light came from.
 *  - Settling: for 30 s after a slew stops the tube rings, and stars are oval, elongated along the image direction of the last motion.
 *  - Field rotation and drift come from sampling this model through each exposure (CelestronOriginSimulator): the camera turns with the mount, so a
 *    tracked field turns about its centre, fastest near the zenith, and a still mount lets the stars trail.
 */
class MountModel {
public:
    struct Config {
        double tiltDeg = 5.0, tiltTowardDeg = 18.0;   // base tilt and the azimuth of its low side
        double azimuthZeroDeg = 137.0;                // true azimuth of Enc0 = 0 on a level base
        double altitudeZeroDeg = 43.5;                // K
        double flexureDeg = -0.4;                     // F
        double tiltSensorBiasDeg = 1.15;
        double startEnc0Deg = 0.0, startEnc1Deg = 0.0;
        double cableNeutralEnc0Deg = 0.0;
        double cordTautDeg = 180.0;
        double peAzArcsec = 15, peAltArcsec = 15;     // periodic error amplitudes (first harmonic; the second is 30 % of it)
        double pePhaseAzDeg = 0, pePhaseAltDeg = 0, pe2PhaseAzDeg = 0, pe2PhaseAltDeg = 0;
        double wormTeeth = 144;                       // placeholder: the Origin's worm is not known; ORIGIN_SIM_WORM_TEETH
        double coneArcmin = 0;
        bool refraction = true;
        double pressureHPa = 1010, temperatureC = 10;
        double settleSeconds = 30, settleElongation = 2.5;   // star length over width just after a stop, falling to 1
        static Config fromEnvironment();
    };

    struct TrackPoint { double raRad, decRad, jd; };

    explicit MountModel(const Config& c = Config::fromEnvironment());

    // site (radians, as the protocol carries it)
    void setSite(double latitudeRad, double longitudeRad) { m_lat = latitudeRad; m_lon = longitudeRad; }
    double latitudeRad() const { return m_lat; }
    double longitudeRad() const { return m_lon; }

    // time step: moves the axes under the current mode
    void step(double seconds, double jd);

    // commands
    void setRates(int altRate, int azmRate);          // manual; zero rates leave a goto or track running
    bool gotoJ2000(double raRad, double decRad, double jd);   // false if the firmware is not initialised
    bool track(const std::vector<TrackPoint>& points);
    void halt();
    void initialise(bool fake, double jd);             // sets the firmware frame from here
    void slipAzimuth(double degrees);                  // a knock: the sky turns, the encoder does not

    // readings
    double enc0Deg() const;                           // wrapped to [-180, 180), as reported
    double enc1Deg() const { return m_enc1; }
    double turnedDeg() const { return m_enc0 - m_cfg.cableNeutralEnc0Deg; }
    bool isInitialised() const { return m_initialised; }
    bool isFakeInitialised() const { return m_fake; }
    bool isGotoOver() const { return m_mode != Mode::Goto; }
    bool isTracking() const { return m_mode == Mode::Track; }
    void trueAltAz(double& altDeg, double& azDeg) const;
    void trueJ2000(double jd, double& raRad, double& decRad) const;
    // the frame: the tube's direction on the apparent sky and the image's up and right there (horizon frame: x north, y east, z up)
    void imageBasis(double boresight[3], double up[3], double right[3]) const;
    // where a J2000 star appears, refraction included, as a unit vector in the same frame
    void apparentDirection(double raRad, double decRad, double jd, double v[3]) const;
    // star shape now: length over width (1 = round) and the long axis's angle in the image, from the right toward up, degrees
    void starShape(double& elongation, double& angleDeg) const;
    double tiltSensorDeg() const;
    void firmwareAltAz(double& altDeg, double& azDeg) const;
    void firmwareJ2000(double jd, double& raRad, double& decRad) const;
    QJsonObject state(double jd) const;               // what can be seen at the telescope: GET /sim/state
    QJsonObject truth(double jd) const;               // the secret geometry and the true pointing: GET /sim/truth, for scoring afterwards
    QStringList takeEvents();                         // cord events since the last call, for the log

private:
    enum class Mode { Idle, Manual, Goto, Track };
    Config m_cfg;
    double m_q[4];                                    // truth rotation, unit quaternion (w, x, y, z)
    double m_lat, m_lon;
    double m_enc0, m_enc1;                            // degrees; Enc0 unwrapped (it is the cable count plus neutral)
    double m_skyAzSlip = 0;                           // degrees the sky has turned less than the encoder (knocks)
    int m_altRate = 0, m_azmRate = 0;
    Mode m_mode = Mode::Idle;
    double m_gotoEnc0 = 0, m_gotoEnc1 = 0, m_gotoRa = 0, m_gotoDec = 0;
    std::vector<TrackPoint> m_track;
    bool m_initialised = false, m_fake = false;
    double m_altOffset = 0, m_azOffset = 0;
    double m_maxTurned = 0, m_minTurned = 0;
    bool m_taut = false;
    int m_tautCount = 0;
    QStringList m_events;

    double m_sinceStop = 1e9;                         // seconds since the axes last stopped
    double m_lastMove0 = 0, m_lastMove1 = 0;          // the last motion's direction, encoder degrees
    double m_prevEnc0 = 0, m_prevEnc1 = 0;
    double periodicError(double axisDeg, double amplitudeArcsec, double phase1Deg, double phase2Deg) const;
    void tubeVector(double enc0, double enc1, double v[3]) const;      // the optical axis, true horizon frame
    double refraction(double trueAltDeg) const;                       // degrees, for a geometric altitude
    void sky(double enc0, double enc1, double& altDeg, double& azDeg) const;
    void moveAxes(double target0, double target1, double maxSpeed, double seconds);
    void moveEnc0(double delta);
    void firmwareTargetEncoders(double raRad, double decRad, double jd, double& enc0, double& enc1) const;
};

#endif
