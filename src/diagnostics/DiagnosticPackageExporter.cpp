#include "diagnostics/DiagnosticPackageExporter.h"

#include "core/DeviceManager.h"
#include "diagnostics/HardwareDiagnostics.h"
#include "diagnostics/HardwareSnapshotWriter.h"
#include "infra/LogCategories.h"

#include <QDateTime>
#include <QDir>
#include <QFile>

namespace NFSScanner::Diagnostics {

namespace {

bool copyRecentLogs(const QString &targetDir)
{
    QDir src(Infra::logsRootDirectory());
    QDir dst(targetDir);
    dst.mkpath(QStringLiteral("latest_logs"));

    const QFileInfoList files = src.entryInfoList({QStringLiteral("*.log")}, QDir::Files, QDir::Time);
    int copied = 0;
    for (const QFileInfo &info : files) {
        if (copied >= 20) {
            break;
        }
        const QString target = dst.filePath(QStringLiteral("latest_logs/%1").arg(info.fileName()));
        if (QFile::copy(info.absoluteFilePath(), target)) {
            ++copied;
        }
    }
    return true;
}

} // namespace

bool DiagnosticPackageExporter::exportPackage(const DiagnosticPackageOptions &options, QString *outputDirectory)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString root = QDir(Infra::logsRootDirectory()).filePath(
        QStringLiteral("diagnostics/NFSScanner_Diagnostics_%1").arg(stamp));
    QDir().mkpath(root);

    HardwareDiagnosticsOptions diagOptions;
    diagOptions.deviceManager = options.deviceManager;
    diagOptions.licenseManager = options.licenseManager;
    diagOptions.projectManager = options.projectManager;
    const QString markdown = HardwareDiagnostics::buildMarkdownSummary(diagOptions);

    QFile mdFile(QDir(root).filePath(QStringLiteral("diagnostics.md")));
    if (!mdFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    mdFile.write(markdown.toUtf8());

    if (options.deviceManager) {
        HardwareSnapshotWriter::writeHardwareConfigSnapshot(root,
                                                            options.deviceManager->hardwareConfig(),
                                                            options.deviceManager->hardwareProfileName());
        HardwareSnapshotWriter::writeDeviceStatusSnapshot(root, options.deviceManager);
    }

    copyRecentLogs(root);

    const QString summary = options.selfCheckSummary.isEmpty()
        ? QStringLiteral("Run NFSScannerSelfCheck.exe for latest results.")
        : options.selfCheckSummary;
    QFile summaryFile(QDir(root).filePath(QStringLiteral("self_check_summary.txt")));
    if (summaryFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        summaryFile.write(summary.toUtf8());
    }

    if (outputDirectory) {
        *outputDirectory = root;
    }
    return true;
}

} // namespace NFSScanner::Diagnostics
