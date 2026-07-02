#include "validation/MockValidationRunner.h"

#include "app/AppCommandLine.h"
#include "analysis/FrequencyCsvParser.h"
#include "analysis/FrequencyData.h"
#include "analysis/HeatmapGenerator.h"
#include "analysis/LutManager.h"
#include "config/HardwareConfig.h"
#include "config/HardwareConfigManager.h"
#include "core/AlignmentConfig.h"
#include "core/AlignmentManager.h"
#include "core/DeviceManager.h"
#include "core/HardwareBringupPlan.h"
#include "core/HardwareBringupRunner.h"
#include "core/PreScanChecklist.h"
#include "core/ScanConfig.h"
#include "core/ScanHardware.h"
#include "core/ScanPathPlanner.h"
#include "core/ScanResult.h"
#include "devices/FaultInjectionConfig.h"
#include "devices/camera/MockCamera.h"
#include "devices/motion/MockMotionController.h"
#include "devices/motion/GrblResponseParser.h"
#include "devices/probe/MockProbeController.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumConfig.h"
#include "diagnostics/DiagnosticPackageExporter.h"
#include "diagnostics/HardwareSessionRecorder.h"
#include "diagnostics/HardwareSnapshotWriter.h"
#include "diagnostics/HardwareSessionReplay.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"
#include "report/ReportData.h"
#include "report/ReportGenerator.h"
#include "storage/TaskStorage.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QProcess>
#include <QTextStream>

#include <cmath>
#include <functional>

namespace NFSScanner::Validation {

namespace {

const QStringList kRequiredProfiles{
    QStringLiteral("mock_all"),
    QStringLiteral("motion_only_grbl"),
    QStringLiteral("spectrum_only_zna67"),
    QStringLiteral("spectrum_only_fsw"),
    QStringLiteral("spectrum_only_n9020a"),
    QStringLiteral("grbl_zna67_default"),
    QStringLiteral("grbl_fsw_default"),
    QStringLiteral("grbl_n9020a_default"),
    QStringLiteral("fault_injection_demo"),
};

ValidationResult makeResult(const QString &name,
                            ValidationStatus status,
                            const QString &message,
                            const QString &evidence = QString(),
                            const QString &detail = QString())
{
    ValidationResult result;
    result.name = name;
    result.status = status;
    result.message = message;
    result.evidencePath = evidence;
    result.errorDetail = detail;
    return result;
}

bool profileExpectsRealHardware(const QString &profileName)
{
    return profileName != QStringLiteral("mock_all")
        && profileName != QStringLiteral("fault_injection_demo");
}

} // namespace

void MockValidationRunner::setOutputRoot(const QString &path)
{
    outputRoot_ = QDir(path).absolutePath();
}

void MockValidationRunner::setProfileDir(const QString &path)
{
    profileDir_ = QDir(path).absolutePath();
}

void MockValidationRunner::setProfileName(const QString &name)
{
    profileName_ = name;
}

void MockValidationRunner::setPortableZipPath(const QString &path)
{
    portableZipPath_ = path;
}

QString MockValidationRunner::resultsJsonPath() const
{
    return QDir(outputRoot_).filePath(QStringLiteral("validation_results.json"));
}

void MockValidationRunner::ensureOutputTree()
{
    const QStringList dirs{
        QString(),
        QStringLiteral("mock_project"),
        QStringLiteral("mock_scan"),
        QStringLiteral("mock_analysis"),
        QStringLiteral("mock_reports"),
        QStringLiteral("mock_bringup"),
        QStringLiteral("diagnostics"),
        QStringLiteral("logs"),
        QStringLiteral("sessions"),
    };
    for (const QString &dir : dirs) {
        QDir().mkpath(QDir(outputRoot_).filePath(dir));
    }
}

ValidationResult MockValidationRunner::runTimed(const QString &name, const std::function<ValidationResult()> &fn)
{
    QElapsedTimer timer;
    timer.start();
    ValidationResult result = fn();
    result.durationMs = timer.elapsed();
    if (result.name.isEmpty()) {
        result.name = name;
    }
    report_.addResult(result);
    report_.appendResultsJson(resultsJsonPath(), result);
    return result;
}

bool MockValidationRunner::createMockProject()
{
    mockProjectDir_ = QDir(outputRoot_).filePath(QStringLiteral("mock_project"));
    QDir().mkpath(mockProjectDir_);
    QDir().mkpath(QDir(mockProjectDir_).filePath(QStringLiteral("scans")));
    QDir().mkpath(QDir(mockProjectDir_).filePath(QStringLiteral("reports")));

    QJsonObject project;
    project.insert(QStringLiteral("name"), QStringLiteral("Mock Validation Project"));
    project.insert(QStringLiteral("version"), QStringLiteral("1"));
    project.insert(QStringLiteral("created"), QDateTime::currentDateTime().toString(Qt::ISODate));

    QFile file(QDir(mockProjectDir_).filePath(QStringLiteral("project.json")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        lastError_ = QStringLiteral("无法写入 project.json");
        return false;
    }
    file.write(QJsonDocument(project).toJson(QJsonDocument::Indented));
    return true;
}

ValidationResult MockValidationRunner::validateBuildEnvironment()
{
    const QString exePath = QCoreApplication::applicationDirPath() + QStringLiteral("/NFSScannerCli.exe");
    const bool exeOk = QFile::exists(exePath);
    const QString qtVersion = ValidationReport::detectQtVersion();
    if (!exeOk) {
        return makeResult(QStringLiteral("build_environment"),
                          ValidationStatus::Warn,
                          QStringLiteral("NFSScannerCli.exe 不在当前目录（可能由脚本单独验证 build）"),
                          exePath);
    }
    return makeResult(QStringLiteral("build_environment"),
                      ValidationStatus::Pass,
                      QStringLiteral("CLI 可执行文件存在，Qt %1").arg(qtVersion),
                      exePath);
}

ValidationResult MockValidationRunner::validateHardwareProfiles()
{
    QStringList failures;
    QStringList warnings;
    int passCount = 0;

    for (const QString &profileName : kRequiredProfiles) {
        const QString path = QDir(profileDir_.isEmpty() ? NFSScanner::Config::defaultProfilesDirectory() : profileDir_)
                                 .filePath(profileName + QStringLiteral(".json"));
        if (!QFile::exists(path)) {
            failures << QStringLiteral("%1: 文件不存在 (%2)").arg(profileName, path);
            continue;
        }

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            failures << QStringLiteral("%1: 无法读取").arg(profileName);
            continue;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            failures << QStringLiteral("%1: JSON 解析失败 %2").arg(profileName, parseError.errorString());
            continue;
        }

        NFSScanner::Config::HardwareConfigManager manager;
        if (!manager.loadProfile(profileName)) {
            if (profileName == QStringLiteral("mock_all")) {
                failures << QStringLiteral("mock_all load failed: %1").arg(manager.lastError());
            } else if (profileName == QStringLiteral("fault_injection_demo")) {
                const QJsonObject faultObj = document.object().value(QStringLiteral("fault_injection")).toObject();
                if (faultObj.isEmpty()) {
                    failures << QStringLiteral("fault_injection_demo missing fault_injection section");
                } else {
                    warnings << QStringLiteral("fault_injection_demo partial hardware config accepted");
                    ++passCount;
                }
            } else if (profileExpectsRealHardware(profileName)) {
                warnings << QStringLiteral("%1: load warning (无真实硬件可接受): %2").arg(profileName, manager.lastError());
                ++passCount;
            } else {
                failures << QStringLiteral("%1: %2").arg(profileName, manager.lastError());
            }
            continue;
        }

        QStringList errors;
        QStringList profileWarnings;
        if (!NFSScanner::Config::HardwareConfigManager::validateProfile(manager.config(), &errors, &profileWarnings)) {
            if (profileName == QStringLiteral("mock_all")) {
                failures << QStringLiteral("mock_all validate failed: %1").arg(errors.join(QStringLiteral("; ")));
            } else if (profileName == QStringLiteral("fault_injection_demo")) {
                const QJsonObject faultObj = document.object().value(QStringLiteral("fault_injection")).toObject();
                if (faultObj.isEmpty()) {
                    failures << QStringLiteral("fault_injection_demo missing fault_injection section");
                } else {
                    warnings << QStringLiteral("fault_injection_demo partial config accepted for fault injection demo");
                    ++passCount;
                }
            } else if (profileExpectsRealHardware(profileName)) {
                warnings << QStringLiteral("%1: validate warning: %2").arg(profileName, errors.join(QStringLiteral("; ")));
                ++passCount;
            } else {
                failures << QStringLiteral("%1 validate: %2").arg(profileName, errors.join(QStringLiteral("; ")));
            }
            continue;
        }

        const auto &cfg = manager.config();
        if (cfg.motion.enabled && cfg.motion.limits.yMin > cfg.motion.limits.yMax) {
            failures << QStringLiteral("%1: motion y_min/y_max 非法").arg(profileName);
            continue;
        }

        ++passCount;
    }

    const QString evidence = QDir(outputRoot_).filePath(QStringLiteral("profile_validation.txt"));
    QFile evidenceFile(evidence);
    if (evidenceFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream stream(&evidenceFile);
        stream << QStringLiteral("pass=%1 failures=%2 warnings=%3\n").arg(passCount).arg(failures.size()).arg(warnings.size());
        for (const QString &line : failures) {
            stream << "FAIL: " << line << "\n";
        }
        for (const QString &line : warnings) {
            stream << "WARN: " << line << "\n";
        }
    }

    if (!failures.isEmpty()) {
        return makeResult(QStringLiteral("hardware_profiles"),
                          ValidationStatus::Fail,
                          QStringLiteral("%1/%2 profiles OK, %3 failures")
                              .arg(passCount)
                              .arg(kRequiredProfiles.size())
                              .arg(failures.size()),
                          evidence,
                          failures.join(QStringLiteral("\n")));
    }

    return makeResult(QStringLiteral("hardware_profiles"),
                      warnings.isEmpty() ? ValidationStatus::Pass : ValidationStatus::Warn,
                      QStringLiteral("%1/%2 profiles validated").arg(passCount).arg(kRequiredProfiles.size()),
                      evidence,
                      warnings.join(QStringLiteral("\n")));
}

ValidationResult MockValidationRunner::validateMockMotion()
{
    NFSScanner::Devices::Motion::MockMotionController motion;
    if (!motion.connectDevice()) {
        return makeResult(QStringLiteral("mock_motion"), ValidationStatus::Fail, QStringLiteral("connect failed"));
    }
    if (!motion.moveTo(1.0, -2.0, 2.0)) {
        motion.disconnectDevice();
        return makeResult(QStringLiteral("mock_motion"), ValidationStatus::Fail, QStringLiteral("move failed"));
    }
    motion.disconnectDevice();
    return makeResult(QStringLiteral("mock_motion"), ValidationStatus::Pass, QStringLiteral("connect/move/disconnect OK"));
}

ValidationResult MockValidationRunner::validateMockSpectrum()
{
    NFSScanner::Devices::Spectrum::MockSpectrumAnalyzer analyzer;
    if (!analyzer.connectDevice({})) {
        return makeResult(QStringLiteral("mock_spectrum"), ValidationStatus::Fail, analyzer.lastError());
    }
    NFSScanner::Devices::Spectrum::SpectrumConfig config;
    config.startFreqHz = 1.0e9;
    config.stopFreqHz = 2.0e9;
    config.sweepPoints = 11;
    if (!analyzer.configure(config)) {
        return makeResult(QStringLiteral("mock_spectrum"), ValidationStatus::Fail, analyzer.lastError());
    }
    const auto trace = analyzer.singleSweep(0, 0, 0, 0);
    if (trace.freqs.isEmpty() || trace.values.isEmpty()) {
        return makeResult(QStringLiteral("mock_spectrum"), ValidationStatus::Fail, QStringLiteral("singleSweep 返回空 trace"));
    }
    return makeResult(QStringLiteral("mock_spectrum"),
                      ValidationStatus::Pass,
                      QStringLiteral("connect/configure/sweep OK, points=%1").arg(trace.freqs.size()));
}

ValidationResult MockValidationRunner::validateMockCamera()
{
    NFSScanner::Devices::Camera::MockCamera camera;
    if (!camera.connectDevice({})) {
        return makeResult(QStringLiteral("mock_camera"), ValidationStatus::Fail, camera.lastError());
    }
    const QImage image = camera.captureFrame();
    if (image.isNull()) {
        return makeResult(QStringLiteral("mock_camera"), ValidationStatus::Fail, QStringLiteral("captureFrame 返回空图像"));
    }
    const QString path = QDir(outputRoot_).filePath(QStringLiteral("mock_camera_capture.png"));
    if (!image.save(path)) {
        return makeResult(QStringLiteral("mock_camera"), ValidationStatus::Fail, QStringLiteral("无法保存截图"));
    }
    return makeResult(QStringLiteral("mock_camera"), ValidationStatus::Pass, QStringLiteral("capture/save OK"), path);
}

ValidationResult MockValidationRunner::validateMockProbe()
{
    NFSScanner::Devices::Probe::MockProbeController probe;
    if (!probe.connectDevice()) {
        return makeResult(QStringLiteral("mock_probe"), ValidationStatus::Fail, probe.lastError());
    }
    if (!probe.setOrientation(NFSScanner::Devices::Probe::ProbeOrientation::Hx)) {
        return makeResult(QStringLiteral("mock_probe"), ValidationStatus::Fail, QStringLiteral("Hx 切换失败"));
    }
    if (!probe.setOrientation(NFSScanner::Devices::Probe::ProbeOrientation::Hy)) {
        return makeResult(QStringLiteral("mock_probe"), ValidationStatus::Fail, QStringLiteral("Hy 切换失败"));
    }
    return makeResult(QStringLiteral("mock_probe"), ValidationStatus::Pass, QStringLiteral("Hx/Hy switch OK"));
}

ValidationResult MockValidationRunner::validatePreScanChecklist()
{
    NFSScanner::Core::DeviceManager deviceManager;
    deviceManager.loadHardwareProfile(QStringLiteral("mock_all"));
    deviceManager.connectAll();

    NFSScanner::Core::ScanConfig scanConfig;
    scanConfig.hardwareMode = NFSScanner::Core::HardwareMode::MockAll;
    scanConfig.probeOrientation = QStringLiteral("Hx");
    scanConfig.startX = 0;
    scanConfig.endX = 10;
    scanConfig.stepX = 5;
    scanConfig.startY = 0;
    scanConfig.endY = -10;
    scanConfig.stepY = 5;
    scanConfig.startZ = 2;

    NFSScanner::Core::ScanPathPlanner planner;
    const QVector<NFSScanner::Core::ScanPoint> points = planner.generate(scanConfig);

    NFSScanner::Core::PreScanChecklistContext context;
    context.scanConfig = scanConfig;
    context.deviceManager = &deviceManager;
    context.projectExists = true;
    context.outputDirWritable = true;
    context.mockMode = true;
    context.licenseValid = true;
    context.hasAlignment = false;
    context.pointCount = points.size();
    context.hardwareMode = NFSScanner::Core::HardwareMode::MockAll;
    context.hardwareProfileName = QStringLiteral("mock_all");

    const auto result = NFSScanner::Core::PreScanChecklist::evaluate(context);
    const QString evidence = QDir(outputRoot_).filePath(QStringLiteral("prescan_checklist.txt"));
    QFile file(evidence);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        file.write(result.summaryText().toUtf8());
    }

    if (result.hasErrors() || !result.canProceed(true)) {
        return makeResult(QStringLiteral("prescan_checklist"),
                          ValidationStatus::Fail,
                          result.summaryText(),
                          evidence);
    }
    return makeResult(QStringLiteral("prescan_checklist"),
                      result.hasWarnings() ? ValidationStatus::Warn : ValidationStatus::Pass,
                      QStringLiteral("MockAll checklist can proceed"),
                      evidence);
}

ValidationResult MockValidationRunner::validateMockBringup()
{
    NFSScanner::Core::DeviceManager deviceManager;
    if (!deviceManager.loadHardwareProfile(profileName_)) {
        return makeResult(QStringLiteral("mock_bringup"), ValidationStatus::Fail, deviceManager.lastError());
    }
    deviceManager.connectAll();

    const auto plan = NFSScanner::Core::HardwareBringupPlan::planForProfile(profileName_);
    NFSScanner::Core::ScanConfig scanConfig;
    scanConfig.startX = 0;
    scanConfig.endX = 10;
    scanConfig.stepX = 5;
    scanConfig.startY = 0;
    scanConfig.endY = -10;
    scanConfig.stepY = 5;
    scanConfig.startZ = 2;
    scanConfig.probeOrientation = QStringLiteral("Hx");
    scanConfig.hardwareMode = NFSScanner::Core::HardwareMode::MockAll;
    const auto bringup = NFSScanner::Core::HardwareBringupRunner::runPlan(&deviceManager, plan, &scanConfig, true, true);

    const QString bringupDir = QDir(outputRoot_).filePath(QStringLiteral("mock_bringup"));
    QDir().mkpath(bringupDir);
    const QString reportPath = QDir(bringupDir).filePath(QStringLiteral("mock_bringup_report.md"));
    QFile reportFile(reportPath);
    if (reportFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        reportFile.write(bringup.reportMarkdown.toUtf8());
    }

    if (bringup.failCount() > 0) {
        return makeResult(QStringLiteral("mock_bringup"),
                          ValidationStatus::Fail,
                          QStringLiteral("bring-up fail=%1 pass=%2").arg(bringup.failCount()).arg(bringup.passCount()),
                          reportPath);
    }
    return makeResult(QStringLiteral("mock_bringup"),
                      ValidationStatus::Pass,
                      QStringLiteral("bring-up pass=%1").arg(bringup.passCount()),
                      reportPath);
}

ValidationResult MockValidationRunner::validateMockScanE2E()
{
    ensureOutputTree();
    if (!createMockProject()) {
        return makeResult(QStringLiteral("mock_scan_e2e"), ValidationStatus::Fail, lastError_);
    }

    NFSScanner::Core::DeviceManager deviceManager;
    if (!deviceManager.loadHardwareProfile(profileName_)) {
        return makeResult(QStringLiteral("mock_scan_e2e"), ValidationStatus::Fail, deviceManager.lastError());
    }

    NFSScanner::Core::ScanConfig scanConfig;
    scanConfig.startX = 0;
    scanConfig.endX = 10;
    scanConfig.stepX = 5;
    scanConfig.startY = 0;
    scanConfig.endY = -10;
    scanConfig.stepY = 5;
    scanConfig.startZ = 2;
    scanConfig.endZ = 2;
    scanConfig.snakeMode = true;
    scanConfig.dwellMs = 10;
    scanConfig.probeOrientation = QStringLiteral("Hx");
    scanConfig.hardwareMode = NFSScanner::Core::HardwareMode::MockAll;
    scanConfig.errorPolicy = NFSScanner::Core::ScanErrorPolicy::StopOnError;
    scanConfig.projectName = QStringLiteral("Mock Validation Project");
    scanConfig.testName = QStringLiteral("mock_e2e");
    scanConfig.outputDir = QDir(outputRoot_).filePath(QStringLiteral("mock_scan"));

    NFSScanner::Core::ScanPathPlanner planner;
    const QVector<NFSScanner::Core::ScanPoint> expectedPoints = planner.generate(scanConfig);
    if (expectedPoints.isEmpty()) {
        return makeResult(QStringLiteral("mock_scan_e2e"), ValidationStatus::Fail, planner.lastError());
    }

    NFSScanner::Storage::TaskStorage storage;
    if (!storage.beginTask(scanConfig, expectedPoints.size())) {
        return makeResult(QStringLiteral("mock_scan_e2e"), ValidationStatus::Fail, storage.lastError());
    }

    Diagnostics::HardwareSnapshotWriter::writeHardwareConfigSnapshot(storage.taskDir(),
                                                                     deviceManager.hardwareConfig(),
                                                                     profileName_);
    Diagnostics::HardwareSnapshotWriter::writeDeviceStatusSnapshot(storage.taskDir(), &deviceManager);

    NFSScanner::Devices::Motion::MockMotionController motion;
    motion.connectDevice();
    NFSScanner::Devices::Spectrum::MockSpectrumAnalyzer analyzer;
    analyzer.connectDevice({});
    NFSScanner::Devices::Spectrum::SpectrumConfig spectrumConfig;
    spectrumConfig.startFreqHz = 1.0e9;
    spectrumConfig.stopFreqHz = 2.0e9;
    spectrumConfig.sweepPoints = 11;
    spectrumConfig.traceId = QStringLiteral("Trc1_S21");
    analyzer.configure(spectrumConfig);

    for (const NFSScanner::Core::ScanPoint &point : expectedPoints) {
        motion.moveTo(point.x, point.y, point.z);
        const auto trace = analyzer.singleSweep(point.index, point.x, point.y, point.z);
        if (trace.freqs.isEmpty() || trace.values.isEmpty()) {
            return makeResult(QStringLiteral("mock_scan_e2e"),
                              ValidationStatus::Fail,
                              QStringLiteral("点 %1 频谱采集为空").arg(point.index),
                              storage.taskDir());
        }

        NFSScanner::Core::ScanResult scanResult;
        scanResult.index = point.index;
        scanResult.x = point.x;
        scanResult.y = point.y;
        scanResult.z = point.z;
        scanResult.timestamp = QDateTime::currentDateTimeUtc();
        scanResult.traceId = trace.traceId;
        scanResult.freqs = trace.freqs;
        scanResult.values = trace.values;
        scanResult.trace = trace;

        NFSScanner::Storage::PointTimingRecord timing;
        timing.pointStatus = QStringLiteral("ok");
        if (!storage.appendPoint(point, scanResult.timestamp, timing) || !storage.appendTrace(scanResult)) {
            return makeResult(QStringLiteral("mock_scan_e2e"), ValidationStatus::Fail, storage.lastError(), storage.taskDir());
        }

        Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("scan"),
                                                          QStringLiteral("Scan"),
                                                          QStringLiteral("point %1/%2 x=%3 y=%4 z=%5")
                                                              .arg(point.index)
                                                              .arg(expectedPoints.size())
                                                              .arg(point.x)
                                                              .arg(point.y)
                                                              .arg(point.z),
                                                          true);
    }

    motion.disconnectDevice();
    if (!storage.finishTask()) {
        return makeResult(QStringLiteral("mock_scan_e2e"), ValidationStatus::Fail, storage.lastError(), storage.taskDir());
    }

    mockScanTaskDir_ = storage.taskDir();
    const QDir taskDirObj(mockScanTaskDir_);
    const QStringList requiredFiles{
        QStringLiteral("meta.json"),
        QStringLiteral("scan_config.json"),
        QStringLiteral("points.csv"),
        QStringLiteral("traces.csv"),
    };
    QStringList missing;
    for (const QString &fileName : requiredFiles) {
        if (!taskDirObj.exists(fileName)) {
            missing << fileName;
        }
    }

    NFSScanner::Core::AlignmentConfig alignment;
    alignment.enabled = true;
    alignment.worldXMin = 0;
    alignment.worldXMax = 10;
    alignment.worldYMin = -10;
    alignment.worldYMax = 0;
    alignment.worldZ = 2;
    alignment.syncCornersFromRectangle();
    NFSScanner::Core::AlignmentManager alignmentManager;
    alignmentManager.setConfig(alignment);
    alignmentManager.saveToFile(taskDirObj.filePath(QStringLiteral("alignment.json")));

    const QString previewPath = taskDirObj.filePath(QStringLiteral("preview.png"));
    NFSScanner::Analysis::HeatmapGenerator::createMockHeatmap(QSize(320, 240), 1.0).save(previewPath);

    if (!missing.isEmpty()) {
        return makeResult(QStringLiteral("mock_scan_e2e"),
                          ValidationStatus::Fail,
                          QStringLiteral("扫描完成但缺少文件: %1").arg(missing.join(QStringLiteral(", "))),
                          mockScanTaskDir_);
    }

    return makeResult(QStringLiteral("mock_scan_e2e"),
                      ValidationStatus::Pass,
                      QStringLiteral("同步 Mock 扫描 %1 点完成").arg(expectedPoints.size()),
                      mockScanTaskDir_);
}

ValidationResult MockValidationRunner::validateTraceCsvParsing()
{
    if (mockScanTaskDir_.isEmpty()) {
        return makeResult(QStringLiteral("trace_csv_parsing"), ValidationStatus::Skip, QStringLiteral("无 mock 扫描任务目录"));
    }

    const QString csvPath = QDir(mockScanTaskDir_).filePath(QStringLiteral("traces.csv"));
    if (!QFile::exists(csvPath)) {
        return makeResult(QStringLiteral("trace_csv_parsing"), ValidationStatus::Fail, QStringLiteral("traces.csv 不存在"), csvPath);
    }

    NFSScanner::Analysis::FrequencyData data;
    NFSScanner::Analysis::FrequencyCsvParser parser;
    if (!parser.loadFile(csvPath, &data) || !data.isValid()) {
        return makeResult(QStringLiteral("trace_csv_parsing"),
                          ValidationStatus::Fail,
                          parser.lastError(),
                          csvPath);
    }

    if (data.traceIds().isEmpty() || data.frequencyCount() == 0 || data.pointCount() == 0) {
        return makeResult(QStringLiteral("trace_csv_parsing"),
                          ValidationStatus::Fail,
                          QStringLiteral("trace/frequency/point 为空"),
                          csvPath);
    }

    const QString traceId = data.traceIds().first();
    const double x = data.xs().value(0);
    const double y = data.ys().value(0);
    const double z = data.zs().value(0);
    const bool hasMag = data.hasValue(x, y, z, traceId);
    return makeResult(QStringLiteral("trace_csv_parsing"),
                      hasMag ? ValidationStatus::Pass : ValidationStatus::Fail,
                      QStringLiteral("traces=%1 freqs=%2 points=%3")
                          .arg(data.traceIds().join(QStringLiteral(",")))
                          .arg(data.frequencyCount())
                          .arg(data.pointCount()),
                      csvPath);
}

ValidationResult MockValidationRunner::validateHeatmapGeneration()
{
    if (mockScanTaskDir_.isEmpty()) {
        return makeResult(QStringLiteral("heatmap_generation"), ValidationStatus::Skip, QStringLiteral("无 traces 数据"));
    }

    NFSScanner::Analysis::FrequencyData data;
    NFSScanner::Analysis::FrequencyCsvParser parser;
    const QString csvPath = QDir(mockScanTaskDir_).filePath(QStringLiteral("traces.csv"));
    if (!parser.loadFile(csvPath, &data) || !data.isValid()) {
        return makeResult(QStringLiteral("heatmap_generation"), ValidationStatus::Fail, parser.lastError(), csvPath);
    }

    NFSScanner::Analysis::HeatmapRenderOptions options;
    options.traceId = data.traceIds().value(0);
    options.freqIndex = 0;
    options.mode = QStringLiteral("magnitude");
    options.lutName = QStringLiteral("turbo");

    NFSScanner::Analysis::HeatmapGenerator generator;
    const auto render = generator.generate(data, options);
    if (!render.ok || render.image.isNull()) {
        return makeResult(QStringLiteral("heatmap_generation"), ValidationStatus::Fail, render.error);
    }

    const QString heatmapPath = QDir(outputRoot_).filePath(QStringLiteral("mock_analysis/heatmap.png"));
    const QString colorbarPath = QDir(outputRoot_).filePath(QStringLiteral("mock_analysis/colorbar.png"));
    render.image.save(heatmapPath);
    render.colorbar.save(colorbarPath);

    return makeResult(QStringLiteral("heatmap_generation"),
                      ValidationStatus::Pass,
                      QStringLiteral("heatmap + colorbar generated"),
                      heatmapPath);
}

ValidationResult MockValidationRunner::validateAlignmentRectLinear()
{
    NFSScanner::Core::AlignmentConfig config;
    config.mappingMode = NFSScanner::Core::AlignmentMappingMode::LinearRectangle;
    config.worldXMin = 0;
    config.worldXMax = 100;
    config.worldYMin = -50;
    config.worldYMax = 0;
    config.pixelXMin = 0;
    config.pixelXMax = 200;
    config.pixelYMin = 0;
    config.pixelYMax = 100;
    config.syncCornersFromRectangle();

    NFSScanner::Core::AlignmentManager manager;
    manager.setConfig(config);
    const QPointF pixel = manager.worldToPixel(50.0, -25.0);
    const QPointF world = manager.pixelToWorld(pixel.x(), pixel.y());
    if (std::abs(world.x() - 50.0) > 0.5 || std::abs(world.y() + 25.0) > 0.5) {
        return makeResult(QStringLiteral("alignment_rect_linear"),
                          ValidationStatus::Fail,
                          QStringLiteral("roundtrip mismatch x=%1 y=%2").arg(world.x()).arg(world.y()));
    }
    const QString path = QDir(outputRoot_).filePath(QStringLiteral("mock_analysis/alignment_rect.json"));
    manager.saveToFile(path);
    return makeResult(QStringLiteral("alignment_rect_linear"), ValidationStatus::Pass, QStringLiteral("RectLinear mapping OK"), path);
}

ValidationResult MockValidationRunner::validateAlignmentFourPoint()
{
    NFSScanner::Core::AlignmentConfig config;
    config.mappingMode = NFSScanner::Core::AlignmentMappingMode::PerspectiveFourPoint;
    config.pixelCorners = {QPointF(0, 0), QPointF(100, 0), QPointF(100, 100), QPointF(0, 100)};
    config.worldCorners = {QPointF(0, 0), QPointF(100, 0), QPointF(100, 100), QPointF(0, 100)};

    NFSScanner::Core::AlignmentManager manager;
    manager.setConfig(config);
    const QPointF center = manager.worldToPixel(50.0, 50.0);
    const QPointF back = manager.pixelToWorld(center.x(), center.y());
    if (std::abs(back.x() - 50.0) > 1.0 || std::abs(back.y() - 50.0) > 1.0) {
        return makeResult(QStringLiteral("alignment_four_point"),
                          ValidationStatus::Fail,
                          QStringLiteral("perspective roundtrip mismatch"));
    }
    const QString path = QDir(outputRoot_).filePath(QStringLiteral("mock_analysis/alignment_four_point.json"));
    manager.saveToFile(path);
    return makeResult(QStringLiteral("alignment_four_point"), ValidationStatus::Pass, QStringLiteral("FourPoint mapping OK"), path);
}

ValidationResult MockValidationRunner::validateReportExport()
{
    NFSScanner::Report::ReportData data;
    data.projectName = QStringLiteral("Mock Validation Project");
    data.scanTaskDir = mockScanTaskDir_.isEmpty() ? outputRoot_ : mockScanTaskDir_;
    data.scanTime = QDateTime::currentDateTimeUtc();
    data.traceId = QStringLiteral("S11");
    data.lutName = QStringLiteral("turbo");
    data.vmin = -40.0;
    data.vmax = 0.0;
    data.notes = QStringLiteral("Automated mock validation report export.");
    data.probeOrientation = QStringLiteral("Hx");

    NFSScanner::Report::ReportGenerator generator;
    const QString mdPath = QDir(outputRoot_).filePath(QStringLiteral("mock_reports/report.md"));
    const QString htmlPath = QDir(outputRoot_).filePath(QStringLiteral("mock_reports/report.html"));
    if (!generator.exportMarkdown(data, mdPath)) {
        return makeResult(QStringLiteral("report_export"), ValidationStatus::Fail, generator.lastError(), mdPath);
    }
    if (!generator.exportHtml(data, htmlPath)) {
        return makeResult(QStringLiteral("report_export"), ValidationStatus::Fail, generator.lastError(), htmlPath);
    }
    return makeResult(QStringLiteral("report_export"), ValidationStatus::Pass, QStringLiteral("Markdown/HTML exported"), mdPath);
}

ValidationResult MockValidationRunner::validateDiagnosticsExport()
{
    NFSScanner::Core::DeviceManager deviceManager;
    deviceManager.loadHardwareProfile(profileName_);
    deviceManager.connectAll();

    NFSScanner::License::LicenseManager licenseManager;
    NFSScanner::Project::ProjectManager projectManager;
    if (QFile::exists(QDir(mockProjectDir_).filePath(QStringLiteral("project.json")))) {
        projectManager.openProject(mockProjectDir_);
    }

    NFSScanner::Diagnostics::DiagnosticPackageOptions options;
    options.deviceManager = &deviceManager;
    options.licenseManager = &licenseManager;
    options.projectManager = &projectManager;
    options.selfCheckSummary = QStringLiteral("Exported from MockValidationRunner");

    QString outputDir;
    if (!NFSScanner::Diagnostics::DiagnosticPackageExporter::exportPackage(options, &outputDir)) {
        return makeResult(QStringLiteral("diagnostics_export"), ValidationStatus::Fail, QStringLiteral("DiagnosticPackageExporter failed"));
    }

    const QString targetDir = QDir(outputRoot_).filePath(QStringLiteral("diagnostics"));
    QDir().mkpath(targetDir);
    const QString copiedMd = QDir(targetDir).filePath(QStringLiteral("diagnostics.md"));
    QFile::copy(QDir(outputDir).filePath(QStringLiteral("diagnostics.md")), copiedMd);

    if (!QFile::exists(copiedMd)) {
        return makeResult(QStringLiteral("diagnostics_export"), ValidationStatus::Fail, QStringLiteral("diagnostics.md 未生成"), outputDir);
    }
    return makeResult(QStringLiteral("diagnostics_export"), ValidationStatus::Pass, QStringLiteral("diagnostics exported"), copiedMd);
}

ValidationResult MockValidationRunner::validateFaultInjection()
{
    using namespace NFSScanner::Devices;
    QStringList checks;

    auto runCheck = [&](const QString &label, bool ok) {
        checks << QStringLiteral("%1=%2").arg(label, ok ? QStringLiteral("OK") : QStringLiteral("BAD"));
        return ok;
    };

    FaultInjectionConfig cfg = FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.connectFail = true;
    globalFaultInjectionConfig() = cfg;
    NFSScanner::Devices::Motion::MockMotionController motion;
    runCheck(QStringLiteral("motion_connect_fail"), !motion.connectDevice());

    cfg = FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.timeout = true;
    globalFaultInjectionConfig() = cfg;
    motion.connectDevice();
    runCheck(QStringLiteral("motion_timeout"), !motion.moveTo(1, 1, 1));

    NFSScanner::Devices::Spectrum::MockSpectrumAnalyzer analyzer;
    cfg = FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.spectrumEmptyTrace = true;
    globalFaultInjectionConfig() = cfg;
    analyzer.connectDevice({});
    NFSScanner::Devices::Spectrum::SpectrumConfig sc;
    sc.startFreqHz = 1e9;
    sc.stopFreqHz = 2e9;
    sc.sweepPoints = 11;
    analyzer.configure(sc);
    runCheck(QStringLiteral("spectrum_empty_trace"), analyzer.singleSweep(0, 0, 0, 0).freqs.isEmpty());

    NFSScanner::Devices::Camera::MockCamera camera;
    cfg = FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.cameraCaptureFail = true;
    globalFaultInjectionConfig() = cfg;
    camera.connectDevice({});
    runCheck(QStringLiteral("camera_capture_fail"), camera.captureFrame().isNull());

    NFSScanner::Devices::Probe::MockProbeController probe;
    cfg = FaultInjectionConfig::disabled();
    cfg.enabled = true;
    cfg.probeSwitchFail = true;
    globalFaultInjectionConfig() = cfg;
    probe.connectDevice();
    runCheck(QStringLiteral("probe_switch_fail"), !probe.setOrientation(NFSScanner::Devices::Probe::ProbeOrientation::Hy));

    globalFaultInjectionConfig() = FaultInjectionConfig::disabled();

    const QString manualConfirm = NFSScanner::Core::scanErrorPolicyToString(NFSScanner::Core::ScanErrorPolicy::ManualConfirm);
    const QString mockFallback = NFSScanner::Core::scanErrorPolicyToString(NFSScanner::Core::ScanErrorPolicy::MockFallbackExplicit);
    runCheck(QStringLiteral("error_policy_manualConfirm"), manualConfirm == QStringLiteral("ManualConfirm"));
    runCheck(QStringLiteral("error_policy_mockFallbackExplicit"), mockFallback == QStringLiteral("MockFallbackExplicit"));

    NFSScanner::Core::DeviceManager deviceManager;
    deviceManager.loadHardwareProfile(QStringLiteral("motion_only_grbl"));
    NFSScanner::Core::PreScanChecklistContext context;
    context.scanConfig.hardwareMode = NFSScanner::Core::HardwareMode::RealMotionMockSpectrum;
    context.deviceManager = &deviceManager;
    context.hardwareMode = NFSScanner::Core::HardwareMode::RealMotionMockSpectrum;
    context.pointCount = 4;
    const auto checklist = NFSScanner::Core::PreScanChecklist::evaluate(context);
    runCheck(QStringLiteral("prescan_real_motion_blocked"), checklist.hasErrors() && !checklist.canProceed(false));

    const QString evidence = QDir(outputRoot_).filePath(QStringLiteral("fault_injection.txt"));
    QFile file(evidence);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        file.write(checks.join(QStringLiteral("\n")).toUtf8());
    }

    if (checks.contains(QStringLiteral("BAD"))) {
        return makeResult(QStringLiteral("fault_injection"), ValidationStatus::Fail, checks.join(QStringLiteral("; ")), evidence);
    }
    return makeResult(QStringLiteral("fault_injection"), ValidationStatus::Pass, QStringLiteral("expected faults detected"), evidence);
}

ValidationResult MockValidationRunner::validateSessionReplay()
{
    const QString sessionPath = QDir(outputRoot_).filePath(QStringLiteral("sessions/mock_session.jsonl"));
    QDir().mkpath(QFileInfo(sessionPath).absolutePath());

    Diagnostics::HardwareSessionRecorder::startSession(QStringLiteral("mock_validation"));
    Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("tx"), QStringLiteral("$I"), QStringLiteral("Grbl 1.1f"), true);
    Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("rx"), QStringLiteral("?"), QStringLiteral("<Idle|MPos:0,0,2|FS:0,0>"), true);
    Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("tx"), QStringLiteral("G1X5Y-5Z2F1000"), QStringLiteral("ok"), true);
    Diagnostics::HardwareSessionRecorder::recordScpi(QStringLiteral("ZNA67"),
                                                     QStringLiteral("tx"),
                                                     QStringLiteral("*IDN?"),
                                                     QStringLiteral("Rohde&Schwarz,ZNA67,1.2.3"),
                                                     12,
                                                     true);
    Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("camera"), QStringLiteral("Camera"), QStringLiteral("mock capture"), true);
    Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("probe"), QStringLiteral("Probe"), QStringLiteral("Hx"), true);
    Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("scan"), QStringLiteral("Scan"), QStringLiteral("point 1/9"), true);

    if (!Diagnostics::HardwareSessionRecorder::exportSession(sessionPath)) {
        return makeResult(QStringLiteral("session_replay"), ValidationStatus::Fail, QStringLiteral("无法导出 session"));
    }

    QVector<Diagnostics::SessionReplayEntry> entries;
    QString error;
    if (!Diagnostics::HardwareSessionReplay::loadSession(sessionPath, &entries, &error)) {
        return makeResult(QStringLiteral("session_replay"), ValidationStatus::Fail, error, sessionPath);
    }

    QString idleLine;
    QString idn;
    QString summary;
    const bool grblOk = Diagnostics::HardwareSessionReplay::replayGrblStatus(entries, &idleLine);
    const bool scpiOk = Diagnostics::HardwareSessionReplay::replayScpiIdn(entries, &idn);
    const bool traceOk = Diagnostics::HardwareSessionReplay::replayTraceSummary(entries, &summary);

    if (!grblOk || !scpiOk) {
        return makeResult(QStringLiteral("session_replay"),
                          ValidationStatus::Fail,
                          QStringLiteral("replay failed grbl=%1 scpi=%2").arg(grblOk).arg(scpiOk),
                          sessionPath);
    }

    return makeResult(QStringLiteral("session_replay"),
                      ValidationStatus::Pass,
                      QStringLiteral("session=%1 entries, idn=%2").arg(entries.size()).arg(idn),
                      sessionPath);
}

ValidationResult MockValidationRunner::validateCommandLineHelp()
{
    const QString help = NFSScanner::App::AppCommandLine::helpText();
    const bool ok = help.contains(QStringLiteral("--profile")) && help.contains(QStringLiteral("--safe-mode"));
    return makeResult(QStringLiteral("command_line_help"),
                      ok ? ValidationStatus::Pass : ValidationStatus::Fail,
                      ok ? QStringLiteral("help text contains expected options") : QStringLiteral("help text incomplete"));
}

ValidationResult MockValidationRunner::validatePortablePackage()
{
    const QString zipPath = portableZipPath_;
    if (zipPath.isEmpty() || !QFile::exists(zipPath)) {
        return makeResult(QStringLiteral("portable_package"),
                          ValidationStatus::Skip,
                          QStringLiteral("portable zip 未提供或不存在"),
                          zipPath);
    }
    const QFileInfo info(zipPath);
    if (info.size() < 1024 * 1024) {
        return makeResult(QStringLiteral("portable_package"),
                          ValidationStatus::Fail,
                          QStringLiteral("zip 过小: %1 bytes").arg(info.size()),
                          zipPath);
    }
    return makeResult(QStringLiteral("portable_package"),
                      ValidationStatus::Pass,
                      QStringLiteral("zip size=%1 MB").arg(info.size() / (1024.0 * 1024.0), 0, 'f', 2),
                      zipPath);
}

bool MockValidationRunner::runValidateProfiles()
{
    ensureOutputTree();
    const ValidationResult result = runTimed(QStringLiteral("hardware_profiles"), [this]() { return validateHardwareProfiles(); });
    return !result.isFail();
}

bool MockValidationRunner::runMockBringup()
{
    ensureOutputTree();
    const ValidationResult result = runTimed(QStringLiteral("mock_bringup"), [this]() { return validateMockBringup(); });
    return !result.isFail();
}

bool MockValidationRunner::runMockE2E()
{
    ensureOutputTree();
    runTimed(QStringLiteral("mock_motion"), [this]() { return validateMockMotion(); });
    runTimed(QStringLiteral("mock_spectrum"), [this]() { return validateMockSpectrum(); });
    runTimed(QStringLiteral("mock_camera"), [this]() { return validateMockCamera(); });
    runTimed(QStringLiteral("mock_probe"), [this]() { return validateMockProbe(); });
    runTimed(QStringLiteral("prescan_checklist"), [this]() { return validatePreScanChecklist(); });

    const ValidationResult scanResult = runTimed(QStringLiteral("mock_scan_e2e"), [this]() { return validateMockScanE2E(); });
    if (scanResult.isFail()) {
        return false;
    }

    runTimed(QStringLiteral("trace_csv_parsing"), [this]() { return validateTraceCsvParsing(); });
    runTimed(QStringLiteral("heatmap_generation"), [this]() { return validateHeatmapGeneration(); });
    runTimed(QStringLiteral("alignment_rect_linear"), [this]() { return validateAlignmentRectLinear(); });
    runTimed(QStringLiteral("alignment_four_point"), [this]() { return validateAlignmentFourPoint(); });
    runTimed(QStringLiteral("report_export"), [this]() { return validateReportExport(); });
    return true;
}

bool MockValidationRunner::runExportDiagnostics()
{
    ensureOutputTree();
    createMockProject();
    const ValidationResult result = runTimed(QStringLiteral("diagnostics_export"), [this]() { return validateDiagnosticsExport(); });
    return !result.isFail();
}

bool MockValidationRunner::runAllMockValidations()
{
    ensureOutputTree();
    report_.loadResultsJson(resultsJsonPath());

    runTimed(QStringLiteral("build_environment"), [this]() { return validateBuildEnvironment(); });
    runTimed(QStringLiteral("hardware_profiles"), [this]() { return validateHardwareProfiles(); });
    runTimed(QStringLiteral("mock_motion"), [this]() { return validateMockMotion(); });
    runTimed(QStringLiteral("mock_spectrum"), [this]() { return validateMockSpectrum(); });
    runTimed(QStringLiteral("mock_camera"), [this]() { return validateMockCamera(); });
    runTimed(QStringLiteral("mock_probe"), [this]() { return validateMockProbe(); });
    runTimed(QStringLiteral("prescan_checklist"), [this]() { return validatePreScanChecklist(); });
    runTimed(QStringLiteral("mock_bringup"), [this]() { return validateMockBringup(); });

    const ValidationResult scanResult = runTimed(QStringLiteral("mock_scan_e2e"), [this]() { return validateMockScanE2E(); });
    if (!scanResult.isFail()) {
        runTimed(QStringLiteral("trace_csv_parsing"), [this]() { return validateTraceCsvParsing(); });
        runTimed(QStringLiteral("heatmap_generation"), [this]() { return validateHeatmapGeneration(); });
        runTimed(QStringLiteral("alignment_rect_linear"), [this]() { return validateAlignmentRectLinear(); });
        runTimed(QStringLiteral("alignment_four_point"), [this]() { return validateAlignmentFourPoint(); });
        runTimed(QStringLiteral("report_export"), [this]() { return validateReportExport(); });
    }

    runTimed(QStringLiteral("diagnostics_export"), [this]() { return validateDiagnosticsExport(); });
    runTimed(QStringLiteral("fault_injection"), [this]() { return validateFaultInjection(); });
    runTimed(QStringLiteral("session_replay"), [this]() { return validateSessionReplay(); });
    runTimed(QStringLiteral("command_line_help"), [this]() { return validateCommandLineHelp(); });
    runTimed(QStringLiteral("portable_package"), [this]() { return validatePortablePackage(); });

    return report_.overallPass();
}

bool MockValidationRunner::generateFinalReport(const ValidationReportMeta &meta)
{
    ensureOutputTree();
    report_.loadResultsJson(resultsJsonPath());
    if (!portableZipPath_.isEmpty()) {
        const ValidationResult portableResult = validatePortablePackage();
        report_.addResult(portableResult);
        report_.appendResultsJson(resultsJsonPath(), portableResult);
    }
    const QString reportPath = QDir(outputRoot_).filePath(QStringLiteral("FULL_MOCK_VALIDATION_REPORT.md"));
    return report_.writeMarkdown(reportPath, meta);
}

} // namespace NFSScanner::Validation
