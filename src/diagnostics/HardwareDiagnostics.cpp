#include "diagnostics/HardwareDiagnostics.h"

#include "app/AppVersion.h"
#include "config/HardwareConfigManager.h"
#include "core/DeviceManager.h"
#include "infra/Logger.h"
#include "license/LicenseManager.h"
#include "project/ProjectManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QTextStream>

namespace NFSScanner::Diagnostics {

namespace {

QString hardwareConfigSummary(NFSScanner::Core::DeviceManager *deviceManager)
{
    if (!deviceManager) {
        return QStringLiteral("- 设备管理器未初始化");
    }

    const Config::HardwareConfig config = deviceManager->hardwareConfig();
    return QStringLiteral(
               "- motion: enabled=%1 type=%2 port=%3\n"
               "- spectrum: enabled=%4 type=%5 address=%6:%7\n"
               "- camera: enabled=%8 type=%9\n"
               "- probe: enabled=%10 type=%11 orientation=%12")
        .arg(config.motion.enabled ? QStringLiteral("true") : QStringLiteral("false"),
             config.motion.type,
             config.motion.port,
             config.spectrum.enabled ? QStringLiteral("true") : QStringLiteral("false"),
             config.spectrum.type,
             config.spectrum.address,
             QString::number(config.spectrum.port),
             config.camera.enabled ? QStringLiteral("true") : QStringLiteral("false"),
             config.camera.type,
             config.probe.enabled ? QStringLiteral("true") : QStringLiteral("false"),
             config.probe.type,
             config.probe.orientation);
}

QString deviceStateLine(NFSScanner::Core::DeviceManager *deviceManager)
{
    if (!deviceManager) {
        return QStringLiteral("- 未初始化");
    }

    return QStringLiteral(
               "- motion: %1 (mock=%2)\n"
               "- spectrum: %3\n"
               "- camera: %4\n"
               "- probe: %5\n"
               "- last error: %6")
        .arg(NFSScanner::Core::deviceConnectionStateText(deviceManager->motionState()),
             deviceManager->motionMockMode() ? QStringLiteral("yes") : QStringLiteral("no"),
             NFSScanner::Core::deviceConnectionStateText(deviceManager->spectrumState()),
             NFSScanner::Core::deviceConnectionStateText(deviceManager->cameraState()),
             NFSScanner::Core::deviceConnectionStateText(deviceManager->probeState()),
             deviceManager->lastError().isEmpty() ? QStringLiteral("-") : deviceManager->lastError());
}

QString licenseSummary(NFSScanner::License::LicenseManager *licenseManager)
{
    if (!licenseManager) {
        return QStringLiteral("- Demo (manager unavailable)");
    }
    return QStringLiteral("- status: %1\n- machine id: %2\n- signature: (redacted)")
        .arg(License::licenseStatusText(licenseManager->status()), licenseManager->machineId());
}

QString recentLogs(int lineCount)
{
    const QStringList lines = Infra::Logger::recentLines(lineCount);
    if (lines.isEmpty()) {
        return QStringLiteral("_无日志_");
    }
    return lines.join(QStringLiteral("\n"));
}

} // namespace

QString HardwareDiagnostics::buildMarkdownSummary(const HardwareDiagnosticsOptions &options)
{
    const QDateTime now = QDateTime::currentDateTime();
    QString markdown;
    QTextStream stream(&markdown);
    stream << "# NFSScanner Hardware Diagnostics\n\n";
    stream << "- generated_at: " << now.toString(Qt::ISODate) << "\n";
    stream << "- app: " << APP_NAME << " v" << APP_VERSION << "\n";
    stream << "- qt: " << QT_VERSION_STR << "\n";
    stream << "- data_format: " << DATA_FORMAT_VERSION << "\n\n";

    stream << "## Project\n";
    if (options.projectManager && options.projectManager->hasOpenProject()) {
        stream << "- path: " << options.projectManager->currentProject().rootPath << "\n";
    } else if (!options.projectPath.isEmpty()) {
        stream << "- path: " << options.projectPath << "\n";
    } else {
        stream << "- path: (none)\n";
    }

    stream << "\n## Hardware Config Summary\n";
    stream << hardwareConfigSummary(options.deviceManager) << "\n\n";

    stream << "## Device States\n";
    stream << deviceStateLine(options.deviceManager) << "\n\n";

    stream << "## License\n";
    stream << licenseSummary(options.licenseManager) << "\n\n";

    stream << "## SelfCheck\n";
    stream << "- last run: see NFSScannerSelfCheck.exe\n";
    stream << "- expected: all PASS (hardware scaffolding tests included)\n\n";

    stream << "## Recent Logs (max " << options.recentLogLineCount << ")\n";
    stream << "```\n" << recentLogs(options.recentLogLineCount) << "\n```\n";

    return markdown;
}

bool HardwareDiagnostics::exportMarkdownReport(const HardwareDiagnosticsOptions &options, QString *outputPath)
{
    const QString markdown = buildMarkdownSummary(options);
    const QString logsDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../logs"));
    QDir().mkpath(logsDir);

    const QString fileName = QStringLiteral("diagnostics_%1.md")
                                 .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QDir(logsDir).filePath(fileName);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        return false;
    }
    file.write(markdown.toUtf8());
    if (outputPath) {
        *outputPath = path;
    }
    return true;
}

} // namespace NFSScanner::Diagnostics
