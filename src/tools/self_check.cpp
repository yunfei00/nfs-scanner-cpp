#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"
#include "config/HardwareConfigManager.h"
#include "core/DeviceManager.h"
#include "core/PreScanChecklist.h"
#include "core/ScanPathPlanner.h"
#include "diagnostics/HardwareDiagnostics.h"
#include "devices/camera/CameraFactory.h"
#include "devices/camera/MockCamera.h"
#include "devices/motion/MockMotionController.h"
#include "devices/probe/MockProbeController.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "storage/TaskStorage.h"
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

#include <QGuiApplication>
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
    data.probeOrientation = QStringLiteral("Hy");

    NFSScanner::Report::ReportGenerator generator;
    const QString mdPath = QDir(temp.path()).filePath(QStringLiteral("report.md"));
    const QString htmlPath = QDir(temp.path()).filePath(QStringLiteral("report.html"));
    check(generator.exportMarkdown(data, mdPath), "report markdown export");
    check(QFile::exists(mdPath), "report markdown file exists");
    {
        QFile mdFile(mdPath);
        check(mdFile.open(QIODevice::ReadOnly | QIODevice::Text), "report markdown read");
        const QString content = QString::fromUtf8(mdFile.readAll());
        check(content.contains(QStringLiteral("Hy")), "report shows probe orientation");
    }
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

void testHardwareConfigDefaultAndRoundTrip()
{
    QTemporaryDir temp;
    check(temp.isValid(), "hardware config temp dir valid");

    const QString path = QDir(temp.path()).filePath(QStringLiteral("hardware_config.json"));
    NFSScanner::Config::HardwareConfigManager manager;
    check(manager.ensureDefaultFile(path), "hardware config default generate");
    check(QFile::exists(path), "hardware config file exists");

    check(manager.load(path), "hardware config load");
    manager.config().motion.port = QStringLiteral("COM9");
    check(manager.save(path), "hardware config save");

    NFSScanner::Config::HardwareConfigManager reloaded;
    check(reloaded.load(path), "hardware config reload");
    check(reloaded.config().motion.port == QStringLiteral("COM9"), "hardware config port roundtrip");
}

void testMotionLimitsValidation()
{
    NFSScanner::Config::HardwareConfig config = NFSScanner::Config::HardwareConfig::defaults();
    config.motion.enabled = true;
    config.motion.limits.yMin = 0.0;
    config.motion.limits.yMax = -1.0;
    QStringList errors;
    check(!config.validate(&errors), "invalid motion limits rejected");
    check(!errors.isEmpty(), "motion limits validation message");
}

void testMockMotionController()
{
    NFSScanner::Devices::Motion::MockMotionController motion;
    check(motion.connectDevice(), "mock motion connect");
    check(motion.isConnected(), "mock motion connected");
    check(motion.moveTo(10.0, -20.0, 2.0), "mock motion move");
    motion.disconnectDevice();
    check(!motion.isConnected(), "mock motion disconnect");
}

void testMockSpectrumAnalyzer()
{
    NFSScanner::Devices::Spectrum::MockSpectrumAnalyzer analyzer;
    QVariantMap options;
    options.insert(QStringLiteral("mock"), true);
    check(analyzer.connectDevice(options), "mock spectrum connect");
    check(analyzer.isConnected(), "mock spectrum connected");
    NFSScanner::Devices::Spectrum::SpectrumConfig config;
    config.startFreqHz = 1e9;
    config.stopFreqHz = 2e9;
    config.sweepPoints = 11;
    check(analyzer.configure(config), "mock spectrum configure");
    const auto trace = analyzer.singleSweep(0, 0.0, 0.0, 1.0);
    check(!trace.freqs.isEmpty(), "mock spectrum trace freqs");
    analyzer.disconnectDevice();
}

void testMockCameraCapture()
{
    NFSScanner::Devices::Camera::MockCamera camera;
    check(camera.connectDevice({}), "mock camera connect");
    const QImage image = camera.captureFrame();
    check(!image.isNull(), "mock camera capture");
    QString savedPath;
    check(NFSScanner::Devices::Camera::saveCameraImage(image, QDir::tempPath(), &savedPath), "mock camera save");
    check(QFile::exists(savedPath), "mock camera saved file exists");
}

void testMockProbeHxHy()
{
    NFSScanner::Devices::Probe::MockProbeController probe;
    check(probe.connectDevice(), "mock probe connect");
    check(probe.setOrientation(NFSScanner::Devices::Probe::ProbeOrientation::Hy), "mock probe Hy");
    check(probe.currentOrientation() == NFSScanner::Devices::Probe::ProbeOrientation::Hy, "mock probe Hy state");
    check(probe.setOrientation(NFSScanner::Devices::Probe::ProbeOrientation::Hx), "mock probe Hx");
}

void testPreScanChecklistMockPass()
{
    NFSScanner::Core::DeviceManager deviceManager;
    NFSScanner::Core::ScanConfig config;
    config.startX = 0.0;
    config.endX = 1.0;
    config.stepX = 1.0;
    config.startY = -10.0;
    config.endY = -5.0;
    config.stepY = 5.0;
    config.probeOrientation = QStringLiteral("Hx");

    NFSScanner::Core::PreScanChecklistContext context;
    context.scanConfig = config;
    context.deviceManager = &deviceManager;
    context.projectExists = false;
    context.outputDirWritable = true;
    context.mockMode = true;
    context.pointCount = 4;
    context.licenseDemo = true;
    context.licenseValid = true;

    const auto result = NFSScanner::Core::PreScanChecklist::evaluate(context);
    check(result.canProceed(true), "pre-scan checklist mock pass");
    check(!result.hasErrors(), "pre-scan checklist mock no errors");
}

void testPreScanChecklistRealMotionBlocks()
{
    NFSScanner::Core::DeviceManager deviceManager;
    NFSScanner::Config::HardwareConfig hw = deviceManager.hardwareConfig();
    hw.motion.enabled = true;
    deviceManager.setHardwareConfig(hw);
    deviceManager.setMotionMockMode(false);

    NFSScanner::Core::ScanConfig config;
    config.startX = 0.0;
    config.endX = 1.0;
    config.stepX = 1.0;
    config.startY = 0.0;
    config.endY = 0.0;
    config.stepY = 1.0;

    NFSScanner::Core::PreScanChecklistContext context;
    context.scanConfig = config;
    context.deviceManager = &deviceManager;
    context.outputDirWritable = true;
    context.mockMode = false;
    context.pointCount = 2;
    context.licenseValid = true;

    const auto result = NFSScanner::Core::PreScanChecklist::evaluate(context);
    check(result.hasErrors(), "pre-scan checklist blocks real motion disconnected");
    check(!result.canProceed(true), "pre-scan checklist cannot proceed");
}

void testDiagnosticsMarkdownExport()
{
    NFSScanner::Core::DeviceManager deviceManager;
    NFSScanner::Diagnostics::HardwareDiagnosticsOptions options;
    options.deviceManager = &deviceManager;
    QString exportPath;
    check(NFSScanner::Diagnostics::HardwareDiagnostics::exportMarkdownReport(options, &exportPath),
          "diagnostics markdown export");
    check(QFile::exists(exportPath), "diagnostics markdown file exists");
}

void testTaskStorageProbeOrientation()
{
    QTemporaryDir temp;
    check(temp.isValid(), "task storage temp dir valid");

    NFSScanner::Core::ScanConfig config;
    config.projectName = QStringLiteral("hw_test");
    config.testName = QStringLiteral("probe");
    config.outputDir = temp.path();
    config.probeOrientation = QStringLiteral("Hy");

    NFSScanner::Storage::TaskStorage storage;
    check(storage.beginTask(config, 1), "task storage begin with probe orientation");

    const QString scanConfigPath = QDir(storage.taskDir()).filePath(QStringLiteral("scan_config.json"));
    QFile scanConfigFile(scanConfigPath);
    check(scanConfigFile.open(QIODevice::ReadOnly), "scan_config json read");
    const QJsonObject object = QJsonDocument::fromJson(scanConfigFile.readAll()).object();
    check(object.value(QStringLiteral("probe_orientation")).toString() == QStringLiteral("Hy"),
          "scan_config records probe_orientation");
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
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
    testHardwareConfigDefaultAndRoundTrip();
    testMotionLimitsValidation();
    testMockMotionController();
    testMockSpectrumAnalyzer();
    testMockCameraCapture();
    testMockProbeHxHy();
    testTaskStorageProbeOrientation();
    testPreScanChecklistMockPass();
    testPreScanChecklistRealMotionBlocks();
    testDiagnosticsMarkdownExport();

    if (gFailures == 0) {
        std::printf("All self-check tests passed.\n");
        return 0;
    }

    std::fprintf(stderr, "%d test(s) failed.\n", gFailures);
    return 1;
}
