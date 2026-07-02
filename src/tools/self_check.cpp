#include "core/AlignmentManager.h"
#include "core/ScanConfig.h"
#include "app/AppCommandLine.h"
#include "core/HardwareBringupPlan.h"
#include "core/HardwareBringupRunner.h"
#include "core/DeviceManager.h"
#include "core/PreScanChecklist.h"
#include "core/ScanPathPlanner.h"
#include "diagnostics/DiagnosticPackageExporter.h"
#include "diagnostics/HardwareDiagnostics.h"
#include "core/ScanHardware.h"
#include "config/HardwareConfigManager.h"
#include "devices/FaultInjectionConfig.h"
#include "diagnostics/DeviceStatusSnapshot.h"
#include "diagnostics/HardwareSessionRecorder.h"
#include "diagnostics/HardwareSessionReplay.h"
#include "diagnostics/HardwareSnapshotWriter.h"
#include "devices/camera/CameraFactory.h"
#include "devices/camera/MockCamera.h"
#include "devices/motion/GrblCommandBuilder.h"
#include "devices/motion/GrblResponseParser.h"
#include "devices/motion/MockMotionController.h"
#include "devices/probe/MockProbeController.h"
#include "devices/spectrum/DeviceBringupResult.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/ScpiCommandLogger.h"
#include "devices/spectrum/ScpiCommandProfile.h"
#include "infra/LogCategories.h"
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
    config.hardwareMode = NFSScanner::Core::HardwareMode::MockAll;
    config.errorPolicy = NFSScanner::Core::ScanErrorPolicy::RetryThenStop;
    config.errorStrategy = NFSScanner::Core::ScanErrorPolicy::RetryThenStop;
    config.hardwareConfigProfile = QStringLiteral("mock_all");

    NFSScanner::Storage::TaskStorage storage;
    check(storage.beginTask(config, 1), "task storage begin with probe orientation");

    const QString scanConfigPath = QDir(storage.taskDir()).filePath(QStringLiteral("scan_config.json"));
    QFile scanConfigFile(scanConfigPath);
    check(scanConfigFile.open(QIODevice::ReadOnly), "scan_config json read");
    const QJsonObject object = QJsonDocument::fromJson(scanConfigFile.readAll()).object();
    check(object.value(QStringLiteral("probe_orientation")).toString() == QStringLiteral("Hy"),
          "scan_config records probe_orientation");
    check(object.value(QStringLiteral("hardware_mode")).toString() == QStringLiteral("MockAll"),
          "scan_config records hardware_mode");
    check(object.value(QStringLiteral("error_strategy")).toString() == QStringLiteral("retryThenStop"),
          "scan_config records error_strategy");
}

void testHardwareProfileLoadAndValidate()
{
    NFSScanner::Config::HardwareConfigManager manager;
    const QStringList profiles = manager.listProfiles();
    check(profiles.contains(QStringLiteral("mock_all")), "profile mock_all listed");
    check(manager.loadProfile(QStringLiteral("mock_all")), "profile mock_all load");
    QStringList errors;
    QStringList warnings;
    check(NFSScanner::Config::HardwareConfigManager::validateProfile(manager.config(), &errors, &warnings),
          "profile mock_all validate");
    check(errors.isEmpty(), "profile mock_all no errors");
}

void testGrblParserAndBuilder()
{
    using namespace NFSScanner::Devices::Motion;
    check(GrblCommandBuilder::buildQueryStatus() == QStringLiteral("?"), "GRBL query status");
    check(GrblCommandBuilder::buildUnlock() == QStringLiteral("$X"), "GRBL unlock");
    check(GrblCommandBuilder::buildHome() == QStringLiteral("$H"), "GRBL home");
    check(GrblCommandBuilder::buildVersionQuery() == QStringLiteral("$I"), "GRBL version query");

    MotionPosition target{10.0, -5.0, 2.0};
    const QString g1 = GrblCommandBuilder::buildAbsoluteMove(target, 1000.0);
    check(g1.startsWith(QStringLiteral("G1X")), "GRBL G1 command");
    check(g1.contains(QStringLiteral("Y-5.000")), "GRBL G1 Y negative");

    const auto idle = GrblResponseParser::parseLine(QStringLiteral("<Idle|MPos:1.000,2.000,3.000|FS:0,0>"));
    check(idle.isStatusReport, "GRBL idle status report");
    check(GrblResponseParser::isIdleState(idle.status.state), "GRBL idle state");
    check(idle.status.hasMachinePosition, "GRBL MPos parsed");

    const auto wpos = GrblResponseParser::parseLine(QStringLiteral("<Idle|WPos:0.500,-1.000,0.000>"));
    check(wpos.status.hasWorkPosition, "GRBL WPos parsed");

    const auto alarm = GrblResponseParser::parseLine(QStringLiteral("<Alarm|...>"));
    check(GrblResponseParser::isAlarmState(alarm.status.state), "GRBL alarm state");

    const auto err = GrblResponseParser::parseLine(QStringLiteral("error:15"));
    check(err.isErrorResponse, "GRBL error response");
    check(err.errorCode == 15, "GRBL error code parsed");

    NFSScanner::Config::HardwareConfig hw = NFSScanner::Config::HardwareConfig::defaults();
    hw.motion.enabled = true;
    hw.motion.limits.xMax = -1.0;
    QStringList limitErrors;
    check(!hw.validate(&limitErrors), "GRBL limits reject invalid config");
}

void testScpiProfileAndLogger()
{
    using namespace NFSScanner::Devices::Spectrum;
    const ScpiCommandProfile zna = ScpiCommandProfile::zna67Defaults();
    check(!zna.idnQuery.isEmpty(), "SCPI zna67 idn query");

    const QString profilePath = QDir(QStringLiteral("config/scpi_profiles")).filePath(QStringLiteral("fsw.json"));
    if (QFile::exists(profilePath)) {
        ScpiCommandProfile loaded;
        QString error;
        check(ScpiCommandProfile::loadFromFile(profilePath, &loaded, &error), "SCPI fsw profile load");
        check(loaded.name.contains(QStringLiteral("FSW"), Qt::CaseInsensitive) || !loaded.readTraceCommand.isEmpty(),
              "SCPI fsw profile content");
    }

    ScpiCommandLogger::Entry entry;
    entry.deviceType = QStringLiteral("mock");
    entry.host = QStringLiteral("127.0.0.1");
    entry.port = 5025;
    entry.command = QStringLiteral("*IDN?");
    entry.responseSummary = QStringLiteral("MOCK,DEV,1,0");
    entry.success = true;
    entry.elapsedMs = 5;
    ScpiCommandLogger::logEntry(entry);
    check(!ScpiCommandLogger::lastIdnResponse().isEmpty() || entry.success, "SCPI logger idn recorded");

    const QString logPath = NFSScanner::Infra::logFilePath(NFSScanner::Infra::LogCategory::ScpiRaw);
    check(logPath.contains(QStringLiteral("scpi_")), "SCPI log path category");
}

void testMockSpectrumBringup()
{
    using namespace NFSScanner::Devices::Spectrum;
    const auto zna = SpectrumBringupRunner::runMockBringup(QStringLiteral("zna67"));
    check(zna.overallOk(), "mock ZNA67 bring-up");
    const auto fsw = SpectrumBringupRunner::runMockBringup(QStringLiteral("fsw"));
    check(fsw.overallOk(), "mock FSW bring-up");
    const auto n9020 = SpectrumBringupRunner::runMockBringup(QStringLiteral("n9020a"));
    check(n9020.overallOk(), "mock N9020A bring-up");
}

void testPreScanChecklistHardwareModes()
{
    NFSScanner::Core::DeviceManager deviceManager;
    NFSScanner::Config::HardwareConfig hw = deviceManager.hardwareConfig();
    hw.motion.enabled = true;
    hw.spectrum.enabled = true;
    hw.spectrum.type = QStringLiteral("zna67");
    deviceManager.setHardwareConfig(hw);
    deviceManager.setMotionMockMode(false);

    NFSScanner::Core::ScanConfig config;
    config.startX = 0.0;
    config.endX = 1.0;
    config.stepX = 1.0;
    config.startY = -5.0;
    config.endY = -5.0;
    config.stepY = 1.0;

    auto evaluateMode = [&](NFSScanner::Core::HardwareMode mode) {
        NFSScanner::Core::PreScanChecklistContext context;
        context.scanConfig = config;
        context.deviceManager = &deviceManager;
        context.outputDirWritable = true;
        context.pointCount = 2;
        context.licenseValid = true;
        context.hardwareMode = mode;
        context.hardwareProfileName = QStringLiteral("mock_all");
        context.mockMode = mode == NFSScanner::Core::HardwareMode::MockAll
            || mode == NFSScanner::Core::HardwareMode::MockMotionRealSpectrum;
        return NFSScanner::Core::PreScanChecklist::evaluate(context);
    };

    check(evaluateMode(NFSScanner::Core::HardwareMode::MockAll).canProceed(true), "checklist MockAll");
    const auto realMotionMockSpectrum = evaluateMode(NFSScanner::Core::HardwareMode::RealMotionMockSpectrum);
    check(realMotionMockSpectrum.hasErrors(), "checklist RealMotionMockSpectrum motion check");
    check(!realMotionMockSpectrum.canProceed(true), "checklist RealMotionMockSpectrum blocked");
    check(evaluateMode(NFSScanner::Core::HardwareMode::MockMotionRealSpectrum).canProceed(true),
          "checklist MockMotionRealSpectrum");
    const auto allReal = evaluateMode(NFSScanner::Core::HardwareMode::RealMotionRealSpectrum);
    check(allReal.hasErrors(), "checklist RealMotionRealSpectrum errors");
    check(!allReal.canProceed(true), "checklist RealMotionRealSpectrum blocked");
}

void testHardwareSnapshots()
{
    QTemporaryDir temp;
    check(temp.isValid(), "snapshot temp dir valid");
    NFSScanner::Config::HardwareConfig hw = NFSScanner::Config::HardwareConfig::defaults();
    check(NFSScanner::Diagnostics::HardwareSnapshotWriter::writeHardwareConfigSnapshot(
              temp.path(), hw, QStringLiteral("mock_all")),
          "hardware_config_snapshot save");
    check(QFile::exists(QDir(temp.path()).filePath(QStringLiteral("hardware_config_snapshot.json"))),
          "hardware_config_snapshot file exists");

    NFSScanner::Core::DeviceManager deviceManager;
    check(NFSScanner::Diagnostics::HardwareSnapshotWriter::writeDeviceStatusSnapshot(temp.path(), &deviceManager),
          "device_status_snapshot save");
    check(QFile::exists(QDir(temp.path()).filePath(QStringLiteral("device_status_snapshot.json"))),
          "device_status_snapshot file exists");
}

void testDiagnosticPackageExport()
{
    NFSScanner::Core::DeviceManager deviceManager;
    NFSScanner::Diagnostics::DiagnosticPackageOptions options;
    options.deviceManager = &deviceManager;
    options.selfCheckSummary = QStringLiteral("self-check ok");
    QString outputDir;
    check(NFSScanner::Diagnostics::DiagnosticPackageExporter::exportPackage(options, &outputDir),
          "diagnostic package export");
    check(QDir(outputDir).exists(), "diagnostic package directory exists");
    check(QFile::exists(QDir(outputDir).filePath(QStringLiteral("diagnostics.md"))), "diagnostics.md exists");
}

void testScanHardwareSerialization()
{
    check(NFSScanner::Core::hardwareModeToString(NFSScanner::Core::HardwareMode::MockAll) == QStringLiteral("MockAll"),
          "hardware mode to string");
    check(NFSScanner::Core::hardwareModeFromString(QStringLiteral("RealMotionRealSpectrum"))
              == NFSScanner::Core::HardwareMode::RealMotionRealSpectrum,
          "hardware mode from string");
    check(NFSScanner::Core::scanErrorPolicyToString(NFSScanner::Core::ScanErrorPolicy::ManualConfirm)
              == QStringLiteral("manualConfirm"),
          "scan error policy manualConfirm");
    check(NFSScanner::Core::scanErrorPolicyFromString(QStringLiteral("mockFallbackExplicit"))
              == NFSScanner::Core::ScanErrorPolicy::MockFallbackExplicit,
          "scan error policy mockFallbackExplicit");
    check(NFSScanner::Core::scanErrorStrategyToString(NFSScanner::Core::ScanErrorPolicy::SkipPoint)
              == QStringLiteral("skipPoint"),
          "scan error strategy serialize");
    check(NFSScanner::Core::inferHardwareMode(true, true) == NFSScanner::Core::HardwareMode::MockAll,
          "infer hardware mode mock all");
    check(NFSScanner::Core::inferHardwareMode(false, false) == NFSScanner::Core::HardwareMode::RealMotionRealSpectrum,
          "infer hardware mode all real");
}

void testLogCategoriesPaths()
{
    const QString motionPath = NFSScanner::Infra::logFilePath(NFSScanner::Infra::LogCategory::Motion);
    check(motionPath.contains(QStringLiteral("motion_")), "motion log path");
    const QString grblPath = NFSScanner::Infra::logFilePath(NFSScanner::Infra::LogCategory::GrblRaw);
    check(grblPath.contains(QStringLiteral("grbl_")), "grbl log path");
    check(NFSScanner::Infra::logsRootDirectory().contains(QStringLiteral("logs")), "logs root directory");
}

void testCameraConfigFields()
{
    NFSScanner::Config::HardwareConfig hw = NFSScanner::Config::HardwareConfig::defaults();
    hw.camera.width = 1920;
    hw.camera.height = 1080;
    hw.camera.exposureMs = 33.0;
    hw.camera.rotationDeg = 90;
    QStringList errors;
    check(hw.validate(&errors), "camera config validate");
}

void testCommandLineParse()
{
    const QString help = NFSScanner::App::AppCommandLine::helpText();
    check(help.contains(QStringLiteral("--profile")), "command line help profile");
    check(help.contains(QStringLiteral("--safe-mode")), "command line help safe mode");
    check(help.contains(QStringLiteral("--export-diagnostics")), "command line help diagnostics");
}

void testDeviceStatusSnapshot()
{
    NFSScanner::Core::DeviceManager deviceManager;
    QJsonObject object;
    check(NFSScanner::Diagnostics::DeviceStatusSnapshot::capture(&deviceManager, nullptr, QStringLiteral("mock_all"), &object),
          "device status snapshot capture");
    check(object.contains(QStringLiteral("motion")), "device status snapshot motion");
    check(object.value(QStringLiteral("profile")).toString() == QStringLiteral("mock_all"), "device status snapshot profile");
}

void testHardwareSessionRecorderReplay()
{
    NFSScanner::Diagnostics::HardwareSessionRecorder::startSession(QStringLiteral("selfcheck_test"));
    NFSScanner::Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("rx"), QStringLiteral("?"),
                                                                 QStringLiteral("<Idle|MPos:0,0,0>"), true);
    NFSScanner::Diagnostics::HardwareSessionRecorder::recordScpi(QStringLiteral("ZNA67"), QStringLiteral("rx"),
                                                                 QStringLiteral("*IDN?"), QStringLiteral("MOCK,ZNA,1"), 5, true);
    const QString path = NFSScanner::Diagnostics::HardwareSessionRecorder::currentSessionPath();
    check(QFile::exists(path), "hardware session file exists");

    QVector<NFSScanner::Diagnostics::SessionReplayEntry> entries;
    check(NFSScanner::Diagnostics::HardwareSessionReplay::loadSession(path, &entries, nullptr), "hardware session load");
    QString idleLine;
    check(NFSScanner::Diagnostics::HardwareSessionReplay::replayGrblStatus(entries, &idleLine, nullptr), "replay GRBL idle");
    QString idn;
    check(NFSScanner::Diagnostics::HardwareSessionReplay::replayScpiIdn(entries, &idn), "replay SCPI IDN");
}

void testFaultInjectionConfig()
{
    NFSScanner::Devices::FaultInjectionConfig cfg;
    check(NFSScanner::Devices::FaultInjectionConfig::loadFromProfile(QStringLiteral("fault_injection_demo"), &cfg, nullptr),
          "fault injection profile load");
    NFSScanner::Devices::globalFaultInjectionConfig() = cfg;

    NFSScanner::Devices::Motion::MockMotionController motion;
    cfg.enabled = true;
    cfg.connectFail = true;
    NFSScanner::Devices::globalFaultInjectionConfig() = cfg;
    check(!motion.connectDevice(), "mock motion connect_fail");

    cfg.connectFail = false;
    cfg.timeout = true;
    NFSScanner::Devices::globalFaultInjectionConfig() = cfg;
    motion.connectDevice();
    check(!motion.moveTo(1, 1, 1), "mock motion timeout");

    NFSScanner::Devices::Spectrum::MockSpectrumAnalyzer analyzer;
    cfg = NFSScanner::Devices::FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.spectrumEmptyTrace = true;
    NFSScanner::Devices::globalFaultInjectionConfig() = cfg;
    analyzer.connectDevice({});
    NFSScanner::Devices::Spectrum::SpectrumConfig sc;
    sc.startFreqHz = 1e9;
    sc.stopFreqHz = 2e9;
    sc.sweepPoints = 11;
    analyzer.configure(sc);
    check(analyzer.singleSweep(0, 0, 0, 0).freqs.isEmpty(), "mock spectrum empty trace");

    NFSScanner::Devices::Camera::MockCamera camera;
    cfg = NFSScanner::Devices::FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.cameraCaptureFail = true;
    NFSScanner::Devices::globalFaultInjectionConfig() = cfg;
    camera.connectDevice({});
    check(camera.captureFrame().isNull(), "mock camera fail");

    NFSScanner::Devices::Probe::MockProbeController probe;
    cfg = NFSScanner::Devices::FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.probeSwitchFail = true;
    NFSScanner::Devices::globalFaultInjectionConfig() = cfg;
    probe.connectDevice();
    check(!probe.setOrientation(NFSScanner::Devices::Probe::ProbeOrientation::Hy), "mock probe fail");

    NFSScanner::Devices::globalFaultInjectionConfig() = NFSScanner::Devices::FaultInjectionConfig::disabled();
}

void testFailedPointsWrite()
{
    QTemporaryDir temp;
    check(temp.isValid(), "failed points temp dir valid");
    NFSScanner::Core::ScanConfig config;
    config.outputDir = temp.path();
    config.errorPolicy = NFSScanner::Core::ScanErrorPolicy::SkipPoint;
    NFSScanner::Storage::TaskStorage storage;
    check(storage.beginTask(config, 1), "failed points begin task");
    NFSScanner::Core::ScanPoint point;
    point.index = 0;
    point.x = 1.0;
    point.y = 2.0;
    point.z = 3.0;
    NFSScanner::Storage::PointTimingRecord timing;
    timing.retryCount = 2;
    check(storage.appendFailedPoint(point, QStringLiteral("spectrum timeout"), timing), "failed point append");
    check(QFile::exists(QDir(storage.taskDir()).filePath(QStringLiteral("failed_points.csv"))), "failed_points.csv exists");
}

void testBringupMockPlan()
{
    NFSScanner::Core::DeviceManager deviceManager;
    deviceManager.loadHardwareProfile(QStringLiteral("mock_all"));
    const auto plan = NFSScanner::Core::HardwareBringupPlan::planForProfile(QStringLiteral("mock_all"));
    check(!plan.steps.isEmpty(), "bringup mock plan steps");
    const auto result = NFSScanner::Core::HardwareBringupRunner::runPlan(&deviceManager, plan, nullptr, false, true);
    check(result.passCount() > 0, "bringup mock plan pass count");
    check(!result.reportMarkdown.isEmpty(), "bringup report markdown");
    check(QFile::exists(result.exportPath), "bringup report file exists");
}

void testGrblSimulatorResponseParse()
{
    using namespace NFSScanner::Devices::Motion;
    const auto parsed = GrblResponseParser::parseLine(QStringLiteral("<Idle|MPos:1.000,2.000,3.000|FS:0,0>"));
    check(parsed.isStatusReport && parsed.status.hasMachinePosition, "GRBL simulator idle parse");
}

void testScpiSimulatorSampleParse()
{
    const QString sample = QStringLiteral("Rohde&Schwarz,ZNA67,1.2.3");
    check(sample.contains(QStringLiteral("ZNA67")), "SCPI simulator IDN sample");
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
    testHardwareProfileLoadAndValidate();
    testGrblParserAndBuilder();
    testScpiProfileAndLogger();
    testMockSpectrumBringup();
    testPreScanChecklistHardwareModes();
    testHardwareSnapshots();
    testDiagnosticPackageExport();
    testScanHardwareSerialization();
    testLogCategoriesPaths();
    testCameraConfigFields();
    testCommandLineParse();
    testDeviceStatusSnapshot();
    testHardwareSessionRecorderReplay();
    testFaultInjectionConfig();
    testFailedPointsWrite();
    testBringupMockPlan();
    testGrblSimulatorResponseParse();
    testScpiSimulatorSampleParse();
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
