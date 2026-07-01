#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"
#include "core/ScanPathPlanner.h"
#include "analysis/FrequencyCsvParser.h"
#include "analysis/FrequencyData.h"
#include "analysis/LutManager.h"
#include "license/LicenseSignatureVerifier.h"
#include "project/ProjectManager.h"
#include "report/ReportData.h"
#include "report/ReportGenerator.h"

extern "C" {
#include "license/ed25519/ed25519.h"
}

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>
#include <QTemporaryDir>
#include <QTextStream>
#include <QtGlobal>

#include <cstdio>

namespace {

int gFailures = 0;

void check(bool condition, const char *message)
{
    if (!condition) {
        std::fprintf(stderr, "FAIL: %s\n", message);
        ++gFailures;
    } else {
        std::printf("PASS: %s\n", message);
    }
}

void testScanPathSnake()
{
    NFSScanner::Core::ScanConfig config;
    config.startX = 0.0;
    config.endX = 2.0;
    config.stepX = 1.0;
    config.startY = 0.0;
    config.endY = 1.0;
    config.stepY = 1.0;
    config.snakeMode = true;

    NFSScanner::Core::ScanPathPlanner planner;
    const auto points = planner.generate(config);
    check(points.size() == 6, "snake 3x2 grid produces 6 points");
    if (points.size() >= 4) {
        check(points.at(0).x <= points.at(1).x, "row 0 x ascending");
        check(points.at(3).x >= points.at(2).x, "row 1 x reversed in snake mode");
    }
}

void testScanPathInvalidStep()
{
    NFSScanner::Core::ScanConfig config;
    config.stepX = 0.0;
    NFSScanner::Core::ScanPathPlanner planner;
    const auto points = planner.generate(config);
    check(points.isEmpty(), "invalid stepX rejected");
}

void testAlignmentMapping()
{
    NFSScanner::Core::AlignmentManager manager;
    NFSScanner::Core::AlignmentConfig config;
    config.enabled = true;
    config.worldXMin = 0.0;
    config.worldXMax = 100.0;
    config.worldYMin = 0.0;
    config.worldYMax = 50.0;
    config.pixelXMin = 0.0;
    config.pixelXMax = 200.0;
    config.pixelYMin = 0.0;
    config.pixelYMax = 100.0;
    manager.setConfig(config);

    const QPointF px = manager.worldToPixel(50.0, 25.0);
    check(qAbs(px.x() - 100.0) < 0.01, "worldToPixel x center");
    check(qAbs(px.y() - 50.0) < 0.01, "worldToPixel y center");

    const QPointF world = manager.pixelToWorld(200.0, 100.0);
    check(qAbs(world.x() - 100.0) < 0.01, "pixelToWorld x max");
    check(qAbs(world.y() - 50.0) < 0.01, "pixelToWorld y max");
}

void testPerspectiveAlignment()
{
    NFSScanner::Core::AlignmentManager manager;
    NFSScanner::Core::AlignmentConfig config;
    config.enabled = true;
    config.mappingMode = NFSScanner::Core::AlignmentMappingMode::PerspectiveFourPoint;
    config.syncCornersFromRectangle();
    config.worldXMin = 0.0;
    config.worldXMax = 100.0;
    config.worldYMin = 0.0;
    config.worldYMax = 100.0;
    config.pixelXMin = 0.0;
    config.pixelXMax = 200.0;
    config.pixelYMin = 0.0;
    config.pixelYMax = 200.0;
    config.syncCornersFromRectangle();
    manager.setConfig(config);

    const QPointF center = manager.worldToPixel(50.0, 50.0);
    check(qAbs(center.x() - 100.0) < 0.5, "perspective worldToPixel center x");
    check(qAbs(center.y() - 100.0) < 0.5, "perspective worldToPixel center y");

    const QPointF back = manager.pixelToWorld(center.x(), center.y());
    check(qAbs(back.x() - 50.0) < 0.5, "perspective pixelToWorld center x");
    check(qAbs(back.y() - 50.0) < 0.5, "perspective pixelToWorld center y");
}

void testAlignmentJsonRoundTrip()
{
    QTemporaryDir temp;
    check(temp.isValid(), "alignment temp dir valid");

    NFSScanner::Core::AlignmentManager manager;
    NFSScanner::Core::AlignmentConfig config;
    config.enabled = true;
    config.mappingMode = NFSScanner::Core::AlignmentMappingMode::PerspectiveFourPoint;
    config.worldXMin = 10.0;
    config.worldXMax = 110.0;
    config.worldYMin = 5.0;
    config.worldYMax = 55.0;
    config.worldZ = 12.5;
    config.pixelXMin = 0.0;
    config.pixelXMax = 400.0;
    config.pixelYMin = 0.0;
    config.pixelYMax = 200.0;
    config.fixedAspectRatio = true;
    config.syncCornersFromRectangle();
    manager.setConfig(config);

    const QString path = QDir(temp.path()).filePath(QStringLiteral("alignment.json"));
    check(manager.saveToFile(path), "alignment json save");
    check(QFile::exists(path), "alignment json file exists");

    NFSScanner::Core::AlignmentManager loaded;
    check(loaded.loadFromFile(path), "alignment json load");
    check(qAbs(loaded.config().worldXMax - 110.0) < 0.01, "alignment json world x max");
    check(qAbs(loaded.config().worldZ - 12.5) < 0.01, "alignment json world z");
    check(loaded.config().mappingMode == NFSScanner::Core::AlignmentMappingMode::PerspectiveFourPoint,
          "alignment json perspective mode");
}

void testTracesCsvParser()
{
    QTemporaryDir temp;
    check(temp.isValid(), "traces temp dir valid");

    const QString csvPath = QDir(temp.path()).filePath(QStringLiteral("traces.csv"));
    QFile file(csvPath);
    check(file.open(QIODevice::WriteOnly | QIODevice::Text), "traces csv create");
    QTextStream out(&file);
    out << "fre,1.0,2.0\n";
    out << "0_0_0_S11_re,1.0,2.0\n";
    out << "0_0_0_S11_im,0.0,0.0\n";
    out << "1_0_0_S11_re,3.0,4.0\n";
    out << "1_0_0_S11_im,0.0,0.0\n";
    file.close();

    NFSScanner::Analysis::FrequencyData data;
    NFSScanner::Analysis::FrequencyCsvParser parser;
    check(parser.loadFile(csvPath, &data), "traces csv load");
    check(data.isValid(), "traces csv data valid");
    check(data.frequencyCount() == 2, "traces csv frequency count");
    check(data.pointCount() == 2, "traces csv point count");
    check(data.traceIds().contains(QStringLiteral("S11")), "traces csv trace id");
    check(data.hasValue(0.0, 0.0, 0.0, QStringLiteral("S11")), "traces csv point 0_0_0");
}

void testLicenseEd25519Signature()
{
    unsigned char seed[32];
    const char *text = "NFSScannerLicenseDevSeed2026";
    for (int i = 0; i < 32; ++i) {
        seed[i] = static_cast<unsigned char>(text[i % 27]);
    }

    unsigned char publicKey[32];
    unsigned char privateKey[64];
    ed25519_create_keypair(publicKey, privateKey, seed);

    QJsonObject licenseObject;
    licenseObject.insert(QStringLiteral("license_id"), QStringLiteral("SELF-CHECK-001"));
    licenseObject.insert(QStringLiteral("machine_id"), QStringLiteral("demo-machine"));
    licenseObject.insert(QStringLiteral("expire_date"), QStringLiteral("2099-12-31"));
    licenseObject.insert(QStringLiteral("features"), QJsonArray{QStringLiteral("scan")});

    const QByteArray payload = NFSScanner::License::LicenseSignatureVerifier::buildCanonicalPayload(licenseObject);
    unsigned char signature[64];
    ed25519_sign(signature,
                 reinterpret_cast<const unsigned char *>(payload.constData()),
                 static_cast<size_t>(payload.size()),
                 publicKey,
                 privateKey);

    const QByteArray signatureBase64 = QByteArray(reinterpret_cast<const char *>(signature), 64).toBase64();
    check(NFSScanner::License::LicenseSignatureVerifier::verifyEd25519(payload, signatureBase64),
          "license ed25519 verify valid signature");

    unsigned char badSignature[64];
    for (int i = 0; i < 64; ++i) {
        badSignature[i] = signature[i] ^ 0xFF;
    }
    const QByteArray badBase64 = QByteArray(reinterpret_cast<const char *>(badSignature), 64).toBase64();
    check(!NFSScanner::License::LicenseSignatureVerifier::verifyEd25519(payload, badBase64),
          "license ed25519 reject tampered signature");
}

void testReportExport()
{
    QTemporaryDir temp;
    check(temp.isValid(), "report temp dir valid");

    NFSScanner::Report::ReportData data;
    data.projectName = QStringLiteral("SelfCheckProject");
    data.operatorName = QStringLiteral("SelfCheck");
    data.scanTaskDir = temp.path();
    data.scanTime = QDateTime::currentDateTimeUtc();
    data.traceId = QStringLiteral("S11");
    data.lutName = QStringLiteral("turbo");
    data.vmin = -40.0;
    data.vmax = 0.0;
    data.notes = QStringLiteral("mock report from self_check");

    NFSScanner::Report::ReportGenerator generator;
    const QString mdPath = QDir(temp.path()).filePath(QStringLiteral("report.md"));
    const QString htmlPath = QDir(temp.path()).filePath(QStringLiteral("report.html"));
    check(generator.exportMarkdown(data, mdPath), "report markdown export");
    check(QFile::exists(mdPath), "report markdown file exists");
    check(generator.exportHtml(data, htmlPath), "report html export");
    check(QFile::exists(htmlPath), "report html file exists");
}

void testLutManager()
{
    const QStringList luts = NFSScanner::Analysis::LutManager::availableLuts();
    check(!luts.isEmpty(), "LUT list non-empty");
    check(luts.contains(QStringLiteral("turbo")), "turbo LUT exists");

    const QImage bar = NFSScanner::Analysis::LutManager::createColorbar(QStringLiteral("turbo"), 28, 120, 255);
    check(!bar.isNull() && bar.width() == 28 && bar.height() == 120, "colorbar size");
}

void testProjectCreate()
{
    QTemporaryDir temp;
    check(temp.isValid(), "temp dir valid");
    NFSScanner::Project::ProjectManager manager;
    const bool ok = manager.createProject(QStringLiteral("SelfCheckProject"), temp.path());
    check(ok, "project create");
    check(QDir(manager.currentProject().scansDir()).exists(), "project scans dir exists");
    check(QFile::exists(manager.currentProject().projectJsonPath()), "project.json exists");
}

void testProjectDefaultPaths()
{
    QTemporaryDir temp;
    check(temp.isValid(), "project path temp dir valid");
    NFSScanner::Project::ProjectManager manager;
    check(manager.createProject(QStringLiteral("PathProject"), temp.path()), "project path create");

    const QString scansDir = manager.defaultScanOutputDir();
    const QString reportsDir = manager.defaultReportsDir();
    check(scansDir.endsWith(QStringLiteral("scans")), "project default scans dir");
    check(reportsDir.endsWith(QStringLiteral("reports")), "project default reports dir");

    const QString taskDir = QDir(scansDir).filePath(QStringLiteral("scan_selfcheck_task"));
    QDir().mkpath(taskDir);
    QFile tracesFile(QDir(taskDir).filePath(QStringLiteral("traces.csv")));
    check(tracesFile.open(QIODevice::WriteOnly | QIODevice::Text), "project task traces create");
    tracesFile.write("fre,1.0\n0_0_0_S11_re,1.0\n0_0_0_S11_im,0.0\n");
    tracesFile.close();

    check(manager.listScanTaskDirs().contains(QDir(taskDir).absolutePath()), "project list scan task dirs");
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    std::printf("NFSScanner Self Check\n");
    testScanPathSnake();
    testScanPathInvalidStep();
    testAlignmentMapping();
    testPerspectiveAlignment();
    testAlignmentJsonRoundTrip();
    testTracesCsvParser();
    testLicenseEd25519Signature();
    testLutManager();
    testProjectCreate();
    testProjectDefaultPaths();
    testReportExport();

    if (gFailures == 0) {
        std::printf("All self-check tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d test(s) failed.\n", gFailures);
    return 1;
}
