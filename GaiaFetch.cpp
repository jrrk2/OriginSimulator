#include "GaiaFetch.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

const QString kTap = "https://gea.esac.esa.int/tap-server/tap/async";

struct Star { int32_t ra, dec; int16_t g; };

// one request, waited for; redirects are left to the caller
QByteArray send(QNetworkAccessManager& net, const QNetworkRequest& request, const QByteArray* post, QNetworkReply** keep = nullptr, int timeoutMs = 600000)
{
    QNetworkReply* reply = post ? net.post(request, *post) : net.get(request);
    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    timer.start(timeoutMs);
    loop.exec();
    if (!reply->isFinished()) { reply->abort(); }
    const QByteArray body = reply->readAll();
    if (keep) *keep = reply; else reply->deleteLater();
    return body;
}

// an asynchronous TAP job: submit, wait for it to complete, fetch the CSV
bool query(QNetworkAccessManager& net, const QString& adql, QByteArray& csv)
{
    QNetworkRequest submit{ QUrl(kTap) };
    submit.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    submit.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    QUrlQuery form;
    form.addQueryItem("REQUEST", "doQuery");
    form.addQueryItem("LANG", "ADQL");
    form.addQueryItem("FORMAT", "csv");
    form.addQueryItem("PHASE", "RUN");
    form.addQueryItem("MAXREC", "3000000");          // without it the archive stops each job at 50000 rows
    form.addQueryItem("QUERY", adql);
    const QByteArray body = form.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = nullptr;
    send(net, submit, &body, &reply, 120000);
    const QUrl job = reply->header(QNetworkRequest::LocationHeader).toUrl();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();
    if (!job.isValid() || job.isEmpty()) {
        qWarning() << "GaiaFetch: no job from the archive, HTTP" << status;
        return false;
    }
    for (int i = 0; i < 720; ++i) {                                  // up to an hour
        QNetworkRequest r{ QUrl(job.toString() + "/phase") };
        const QString phase = QString::fromUtf8(send(net, r, nullptr, nullptr, 60000)).trimmed();
        if (phase == "COMPLETED") break;
        if (phase == "ERROR" || phase == "ABORTED") {
            QNetworkRequest e{ QUrl(job.toString() + "/error") };
            qWarning().noquote() << "GaiaFetch: the job failed:" << send(net, e, nullptr, nullptr, 60000).left(500);
            return false;
        }
        QThread::sleep(5);
    }
    QNetworkRequest result{ QUrl(job.toString() + "/results/result") };
    result.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    csv = send(net, result, nullptr, nullptr, 1800000);
    return csv.startsWith("ra,dec,phot_g_mean_mag");
}

bool writeCatalogue(const QString& outputFile, const char magic[8], const std::vector<Star>& stars)
{
    QDir().mkpath(QFileInfo(outputFile).absolutePath());
    QFile out(outputFile);
    if (!out.open(QIODevice::WriteOnly)) {
        qCritical() << "GaiaFetch: cannot write" << outputFile;
        return false;
    }
    const uint32_t count = uint32_t(stars.size()), format = 1;
    out.write(magic, 8);
    out.write(reinterpret_cast<const char*>(&count), 4);
    out.write(reinterpret_cast<const char*>(&format), 4);
    for (const Star& s : stars) {
        out.write(reinterpret_cast<const char*>(&s.ra), 4);
        out.write(reinterpret_cast<const char*>(&s.dec), 4);
        out.write(reinterpret_cast<const char*>(&s.g), 2);
    }
    out.close();
    qInfo() << "GaiaFetch:" << count << "stars written to" << outputFile;
    return true;
}

}   // namespace

int GaiaFetch::run(const QString& outputFile)
{
    QNetworkAccessManager net;
    std::vector<Star> stars;
    stars.reserve(3200000);
    // declination bands of 15 degrees, a few hundred thousand stars each, well within the archive's limits for one job
    for (int d0 = -90; d0 < 90; d0 += 15) {
        const int d1 = d0 + 15;
        const QString adql = QString("SELECT ra, dec, phot_g_mean_mag FROM gaiadr3.gaia_source WHERE phot_g_mean_mag < 12 AND dec >= %1 AND dec %2 %3")
                                 .arg(d0).arg(d1 == 90 ? "<=" : "<").arg(d1);
        QByteArray csv;
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; ++attempt) {
            ok = query(net, adql, csv);
            if (!ok) QThread::sleep(10);
        }
        if (!ok) {
            qCritical() << "GaiaFetch: could not fetch declinations" << d0 << "to" << d1;
            return 1;
        }
        size_t n = 0;
        for (const QByteArray& line : csv.split('\n')) {
            const QList<QByteArray> f = line.trimmed().split(',');
            if (f.size() != 3 || f[0] == "ra") continue;
            bool a = false, b = false, c = false;
            const double ra = f[0].toDouble(&a), dec = f[1].toDouble(&b), g = f[2].toDouble(&c);
            if (!a || !b || !c) continue;
            stars.push_back({ int32_t(std::llround(ra * 3.6e6)), int32_t(std::llround(dec * 3.6e6)), int16_t(std::lround(g * 1000)) });
            ++n;
        }
        qInfo() << "GaiaFetch: declinations" << d0 << "to" << d1 << ":" << n << "stars";
    }
    return writeCatalogue(outputFile, "GAIADR3G", stars) ? 0 : 1;
}

int GaiaFetch::runHipparcos(const QString& outputFile)
{
    // ESA's archive (the same as Gaia's) holds the Hipparcos catalogue as public.hipparcos; under 5000 rows, so one synchronous query
    QNetworkAccessManager net;
    QNetworkRequest r{ QUrl("https://gea.esac.esa.int/tap-server/tap/sync") };
    r.setHeader(QNetworkRequest::ContentTypeHeader, "application/x-www-form-urlencoded");
    r.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    QUrlQuery form;
    form.addQueryItem("REQUEST", "doQuery");
    form.addQueryItem("LANG", "ADQL");
    form.addQueryItem("FORMAT", "csv");
    form.addQueryItem("QUERY", "SELECT hip, ra, de, pmra, pmde, vmag, b_v FROM public.hipparcos WHERE vmag < 6");
    const QByteArray body = form.toString(QUrl::FullyEncoded).toUtf8();
    QByteArray csv;
    for (int attempt = 0; attempt < 3 && !csv.startsWith("hip,ra,de,"); ++attempt) {
        if (attempt) QThread::sleep(10);
        csv = send(net, r, &body, nullptr, 180000);
    }
    constexpr double years = 2016.0 - 1991.25;           // Hipparcos' epoch to Gaia DR3's
    std::vector<Star> stars;
    for (const QByteArray& line : csv.split('\n')) {
        const QList<QByteArray> f = line.trimmed().split(',');
        if (f.size() != 7 || f[0] == "hip") continue;
        bool ok[5] = {};
        const double ra = f[1].trimmed().toDouble(&ok[0]), dec = f[2].trimmed().toDouble(&ok[1]), v = f[5].trimmed().toDouble(&ok[2]);
        if (!ok[0] || !ok[1] || !ok[2]) continue;           // the few stars without a position
        double pmRa = f[3].trimmed().toDouble(&ok[3]), pmDec = f[4].trimmed().toDouble(&ok[4]);
        if (!ok[3] || !ok[4]) pmRa = pmDec = 0;
        bool hasBv = false;
        const double bv = f[6].trimmed().toDouble(&hasBv);
        const double g = hasBv ? v - 0.02704 + 0.01424 * bv - 0.2156 * bv * bv + 0.01426 * bv * bv * bv : v;
        // pmRA is mu_alpha* (already times cos dec), in milliarcseconds a year
        const double d = std::clamp(dec + pmDec * years / 3.6e6, -90.0, 90.0);
        double a = ra + pmRa * years / 3.6e6 / std::max(1e-6, std::cos(dec * M_PI / 180.0));
        a = std::fmod(std::fmod(a, 360.0) + 360.0, 360.0);
        stars.push_back({ int32_t(std::llround(a * 3.6e6) % 1296000000LL), int32_t(std::llround(d * 3.6e6)), int16_t(std::lround(g * 1000)) });
    }
    if (stars.size() < 4000) {
        qCritical() << "GaiaFetch: only" << stars.size() << "Hipparcos stars brighter than V 6 (there are about 5000)";
        return 1;
    }
    return writeCatalogue(outputFile, "HIPPARCO", stars) ? 0 : 1;
}
