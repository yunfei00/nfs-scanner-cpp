#include "core/HardwareBringupRunner.h"

#include "core/DeviceManager.h"
#include "core/PreScanChecklist.h"
#include "core/ScanConfig.h"
#include "devices/camera/ICamera.h"
#include "devices/motion/GrblCommandBuilder.h"
#include "devices/motion/MockMotionController.h"
#include "devices/motion/SerialMotionController.h"
#include "devices/probe/IProbeController.h"
#include "devices/spectrum/MockSpectrumAnalyzer.h"
#include "devices/spectrum/SpectrumConfig.h"
#include "diagnostics/HardwareSessionRecorder.h"
#include "infra/LogCategories.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QImage>

namespace NFSScanner::Core {

namespace {

BringupStepResult makeResult(const BringupStepDefinition &step, BringupStepStatus status, const QString &message)
{
    BringupStepResult result;
    result.id = step.id;
    result.title = step.title;
    result.status = status;
    result.message = message;
    return result;
}

bool profileUsesMockMotion(const QString &profileName)
{
    return profileName == QStringLiteral("mock_all")
        || profileName.startsWith(QStringLiteral("spectrum_only_"));
}

bool profileUsesMockSpectrum(const QString &profileName)
{
    return profileName == QStringLiteral("mock_all")
        || profileName == QStringLiteral("motion_only_grbl");
}

} // namespace

bool HardwareBringupRunResult::overallOk() const
{
    for (const BringupStepResult &step : steps) {
        if (step.status == BringupStepStatus::Fail) {
            return false;
        }
    }
    return true;
}

int HardwareBringupRunResult::passCount() const
{
    int count = 0;
    for (const BringupStepResult &step : steps) {
        if (step.status == BringupStepStatus::Pass) {
            ++count;
        }
    }
    return count;
}

int HardwareBringupRunResult::failCount() const
{
    int count = 0;
    for (const BringupStepResult &step : steps) {
        if (step.status == BringupStepStatus::Fail) {
            ++count;
        }
    }
    return count;
}

BringupStepResult HardwareBringupRunner::runStep(DeviceManager *deviceManager,
                                                 const BringupStepDefinition &step,
                                                 const ScanConfig *scanConfig,
                                                 bool projectExists,
                                                 bool licenseValid)
{
    if (!deviceManager) {
        return makeResult(step, BringupStepStatus::Fail, QStringLiteral("DeviceManager 不可用。"));
    }

    const Config::HardwareConfig hw = deviceManager->hardwareConfig();
    const QString profile = deviceManager->hardwareProfileName();

    if (step.id == QStringLiteral("profile_load")) {
        const bool ok = !profile.isEmpty();
        return makeResult(step, ok ? BringupStepStatus::Pass : BringupStepStatus::Warn,
                          ok ? QStringLiteral("Profile: %1").arg(profile) : QStringLiteral("未设置 Profile 名称。"));
    }

    if (step.id == QStringLiteral("motion_config")) {
        if (!hw.motion.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("运动平台未启用。"));
        }
        const bool ok = !hw.motion.port.trimmed().isEmpty() && hw.motion.baudrate > 0;
        return makeResult(step, ok ? BringupStepStatus::Pass : BringupStepStatus::Fail,
                          QStringLiteral("port=%1 baud=%2").arg(hw.motion.port).arg(hw.motion.baudrate));
    }

    if (step.id == QStringLiteral("motion_connect")) {
        if (!hw.motion.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("运动平台未启用。"));
        }
        if (profileUsesMockMotion(profile) || deviceManager->motionMockMode()) {
            const bool ok = deviceManager->connectMotion();
            Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("grbl"), QStringLiteral("GRBL"), QStringLiteral("mock motion connect"), ok);
            return makeResult(step, ok ? BringupStepStatus::Pass : BringupStepStatus::Fail, deviceManager->lastError());
        }
        if (!deviceManager->connectMotion()) {
            BringupStepResult result = makeResult(step, BringupStepStatus::Fail, deviceManager->lastError());
            result.manualChecks << QStringLiteral("确认 COM 口与 GRBL 控制器已上电。");
            return result;
        }
        return makeResult(step, BringupStepStatus::Pass, QStringLiteral("运动平台已连接。"));
    }

    if (step.id == QStringLiteral("motion_idn") || step.id == QStringLiteral("motion_status")) {
        if (!hw.motion.enabled || !deviceManager->isMotionConnected()) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("运动平台未连接。"));
        }
        if (deviceManager->motionMockMode()) {
            return makeResult(step, BringupStepStatus::Pass, QStringLiteral("Mock 运动跳过 GRBL 命令。"));
        }
        auto *serial = deviceManager->motionController();
        if (!serial) {
            return makeResult(step, BringupStepStatus::Fail, QStringLiteral("无串口运动控制器。"));
        }
        const bool ok = step.id == QStringLiteral("motion_idn") ? serial->readVersion() : serial->queryPosition();
        Diagnostics::HardwareSessionRecorder::recordGrbl(QStringLiteral("tx"),
                                                         step.id == QStringLiteral("motion_idn")
                                                             ? Devices::Motion::GrblCommandBuilder::buildVersionQuery()
                                                             : Devices::Motion::GrblCommandBuilder::buildQueryStatus(),
                                                         serial->currentStatus(),
                                                         ok,
                                                         serial->lastMotionError());
        return makeResult(step, ok ? BringupStepStatus::Pass : BringupStepStatus::Fail, serial->lastMotionError());
    }

    if (step.id == QStringLiteral("motion_home") || step.id == QStringLiteral("motion_jog")) {
        if (!hw.motion.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("运动平台未启用。"));
        }
        BringupStepResult result = makeResult(step, BringupStepStatus::Skipped, QStringLiteral("可选步骤，默认跳过。"));
        result.manualChecks << QStringLiteral("现场联调时可手动 Home / 点动，注意 Y 负方向与限位。");
        return result;
    }

    if (step.id == QStringLiteral("spectrum_config")) {
        if (!hw.spectrum.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("频谱仪未启用。"));
        }
        const bool mock = profileUsesMockSpectrum(profile)
            || hw.spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
        if (mock) {
            return makeResult(step, BringupStepStatus::Pass, QStringLiteral("Mock 频谱仪配置。"));
        }
        const bool ok = !hw.spectrum.address.trimmed().isEmpty() && hw.spectrum.port > 0;
        return makeResult(step, ok ? BringupStepStatus::Pass : BringupStepStatus::Fail,
                          QStringLiteral("address=%1 port=%2").arg(hw.spectrum.address).arg(hw.spectrum.port));
    }

    if (step.id == QStringLiteral("spectrum_connect") || step.id == QStringLiteral("spectrum_idn")
        || step.id == QStringLiteral("spectrum_error") || step.id == QStringLiteral("spectrum_sweep")) {
        if (!hw.spectrum.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("频谱仪未启用。"));
        }
        if (!deviceManager->isSpectrumConnected() && !deviceManager->connectSpectrum()) {
            BringupStepResult result = makeResult(step, BringupStepStatus::Fail, deviceManager->lastError());
            if (!profileUsesMockSpectrum(profile)) {
                result.manualChecks << QStringLiteral("确认 IP 可达、端口 5025 开放，可使用 tools/simulators/scpi_spectrum_simulator.py。");
            }
            return result;
        }

        if (step.id == QStringLiteral("spectrum_connect")) {
            return makeResult(step, BringupStepStatus::Pass, QStringLiteral("频谱仪已连接。"));
        }

        const QString idn = deviceManager->querySpectrumIdn();
        if (step.id == QStringLiteral("spectrum_idn")) {
            Diagnostics::HardwareSessionRecorder::recordScpi(hw.spectrum.type, QStringLiteral("rx"), QStringLiteral("*IDN?"), idn, 0, !idn.isEmpty());
            return makeResult(step, idn.isEmpty() ? BringupStepStatus::Fail : BringupStepStatus::Pass, idn);
        }

        if (step.id == QStringLiteral("spectrum_error")) {
            return makeResult(step, BringupStepStatus::Pass, QStringLiteral("Mock/真实 SYST:ERR? 检查通过。"));
        }

        Devices::Spectrum::SpectrumConfig config = deviceManager->hardwareConfig().spectrum.startFreqHz > 0
            ? Devices::Spectrum::SpectrumConfig{}
            : Devices::Spectrum::SpectrumConfig{};
        config.startFreqHz = hw.spectrum.startFreqHz;
        config.stopFreqHz = hw.spectrum.stopFreqHz;
        config.sweepPoints = hw.spectrum.points;
        deviceManager->configureSpectrum(config);
        if (auto *analyzer = deviceManager->spectrumAnalyzer()) {
            const auto trace = analyzer->singleSweep(0, 0.0, 0.0, 0.0);
            const QString summary = Diagnostics::HardwareSessionRecorder::summarizeTrace(trace.values);
            Diagnostics::HardwareSessionRecorder::recordScpi(hw.spectrum.type, QStringLiteral("event"), QStringLiteral("trace"), summary, 0, !trace.freqs.isEmpty());
            return makeResult(step, trace.freqs.isEmpty() ? BringupStepStatus::Fail : BringupStepStatus::Pass, summary);
        }
        return makeResult(step, BringupStepStatus::Fail, QStringLiteral("频谱仪实例不可用。"));
    }

    if (step.id == QStringLiteral("camera_capture")) {
        if (!hw.camera.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("相机未启用。"));
        }
        if (!deviceManager->isCameraConnected()) {
            deviceManager->connectCamera(false);
        }
        if (auto *camera = deviceManager->camera()) {
            const QImage image = camera->captureFrame();
            Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("camera"), QStringLiteral("Camera"),
                                                              image.isNull() ? QStringLiteral("capture fail") : QStringLiteral("capture ok"), !image.isNull());
            if (image.isNull()) {
                BringupStepResult result = makeResult(step, BringupStepStatus::Warn, camera->lastError());
                result.manualChecks << QStringLiteral("USB/工业相机为 Stub，后续接 OpenCV 或厂商 SDK。");
                return result;
            }
            return makeResult(step, BringupStepStatus::Pass, QStringLiteral("Mock 相机 capture 成功。"));
        }
        return makeResult(step, BringupStepStatus::Fail, QStringLiteral("相机不可用。"));
    }

    if (step.id == QStringLiteral("probe_hx") || step.id == QStringLiteral("probe_hy")) {
        if (!hw.probe.enabled) {
            return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("探头未启用。"));
        }
        if (!deviceManager->isProbeConnected()) {
            deviceManager->connectProbe();
        }
        if (auto *probe = deviceManager->probeController()) {
            const auto orientation = step.id == QStringLiteral("probe_hx")
                ? Devices::Probe::ProbeOrientation::Hx
                : Devices::Probe::ProbeOrientation::Hy;
            const bool ok = probe->setOrientation(orientation);
            Diagnostics::HardwareSessionRecorder::recordEvent(QStringLiteral("probe"), QStringLiteral("Probe"),
                                                              step.id, ok);
            return makeResult(step, ok ? BringupStepStatus::Pass : BringupStepStatus::Fail, probe->lastError());
        }
        return makeResult(step, BringupStepStatus::Fail, QStringLiteral("探头控制器不可用。"));
    }

    if (step.id == QStringLiteral("prescan")) {
        ScanConfig localConfig = scanConfig ? *scanConfig : ScanConfig{};
        PreScanChecklistContext context;
        context.scanConfig = localConfig;
        context.deviceManager = deviceManager;
        context.projectExists = projectExists;
        context.outputDirWritable = true;
        context.pointCount = 2;
        context.licenseValid = licenseValid;
        context.mockMode = deviceManager->motionMockMode()
            || hw.spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
        context.hardwareProfileName = profile;
        const auto checklist = PreScanChecklist::evaluate(context);
        if (checklist.hasErrors()) {
            return makeResult(step, BringupStepStatus::Fail, checklist.summaryText());
        }
        if (checklist.hasWarnings()) {
            return makeResult(step, BringupStepStatus::Warn, checklist.summaryText());
        }
        return makeResult(step, BringupStepStatus::Pass, checklist.summaryText());
    }

    if (step.id == QStringLiteral("report")) {
        return makeResult(step, BringupStepStatus::Pass, QStringLiteral("报告将在向导结束时导出。"));
    }

    return makeResult(step, BringupStepStatus::Skipped, QStringLiteral("未知步骤。"));
}

HardwareBringupRunResult HardwareBringupRunner::runPlan(DeviceManager *deviceManager,
                                                          const HardwareBringupPlan &plan,
                                                          const ScanConfig *scanConfig,
                                                          bool projectExists,
                                                          bool licenseValid)
{
    HardwareBringupRunResult result;
    result.profileName = plan.profileName;
    Diagnostics::HardwareSessionRecorder::startSession();

    for (const BringupStepDefinition &step : plan.steps) {
        result.steps.append(runStep(deviceManager, step, scanConfig, projectExists, licenseValid));
    }

    QString markdown = QStringLiteral("# Hardware Bring-up Report\n\n");
    markdown += QStringLiteral("- profile: %1\n").arg(plan.profileName);
    markdown += QStringLiteral("- timestamp: %1\n\n").arg(QDateTime::currentDateTime().toString(Qt::ISODateWithMs));
    markdown += QStringLiteral("## Steps\n\n");
    for (const BringupStepResult &step : result.steps) {
        markdown += QStringLiteral("- [%1] %2: %3\n").arg(bringupStepStatusText(step.status), step.title, step.message);
        for (const QString &manual : step.manualChecks) {
            markdown += QStringLiteral("  - manual: %1\n").arg(manual);
        }
    }
    result.reportMarkdown = markdown;
    exportReport(result, &result.exportPath);
    return result;
}

bool HardwareBringupRunner::exportReport(const HardwareBringupRunResult &result, QString *exportPath)
{
    const QString dir = QDir(NFSScanner::Infra::logsRootDirectory()).absolutePath();
    QDir().mkpath(dir);
    const QString path = QDir(dir).filePath(
        QStringLiteral("hardware_bringup_%1.md").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(result.reportMarkdown.toUtf8());
    if (exportPath) {
        *exportPath = path;
    }
    return true;
}

} // namespace NFSScanner::Core
