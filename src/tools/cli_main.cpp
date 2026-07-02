#include "app/AppVersion.h"
#include "app/CliCommandLine.h"
#include "validation/MockValidationRunner.h"
#include "validation/ValidationReport.h"

#include <QGuiApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>

#include <cstdio>

namespace {

NFSScanner::Validation::ValidationReportMeta readExternalMeta(const QString &outputDir)
{
    NFSScanner::Validation::ValidationReportMeta meta;
    meta.appVersion = QStringLiteral(APP_VERSION);
    meta.qtVersion = NFSScanner::Validation::ValidationReport::detectQtVersion();
    meta.gitCommit = NFSScanner::Validation::ValidationReport::detectGitCommit();

    const auto readStatus = [&](const QString &fileName, bool *flag, QString *message) {
        const QString path = QDir(outputDir).filePath(fileName);
        if (!QFile::exists(path)) {
            return;
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return;
        }
        const QString content = QString::fromUtf8(file.readAll()).trimmed();
        if (message) {
            *message = content;
        }
        if (flag) {
            *flag = content.startsWith(QStringLiteral("PASS"), Qt::CaseInsensitive);
        }
    };

    readStatus(QStringLiteral("build_result.txt"), &meta.buildPass, &meta.buildMessage);
    readStatus(QStringLiteral("selfcheck_result.txt"), &meta.selfCheckPass, &meta.selfCheckMessage);

    const QString portableInfo = QDir(outputDir).filePath(QStringLiteral("portable_result.txt"));
    if (QFile::exists(portableInfo)) {
        QFile file(portableInfo);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QStringList lines = QString::fromUtf8(file.readAll()).split(QStringLiteral("\n"), Qt::SkipEmptyParts);
            if (!lines.isEmpty()) {
                meta.portablePass = lines.first().startsWith(QStringLiteral("PASS"), Qt::CaseInsensitive);
                meta.portableZipPath = lines.value(1).trimmed();
            }
            if (lines.size() > 2) {
                meta.selfCheckMessage = lines.mid(2).join(QStringLiteral("\n"));
            }
        }
    }

    const QString countPath = QDir(outputDir).filePath(QStringLiteral("selfcheck_count.txt"));
    if (QFile::exists(countPath)) {
        QFile file(countPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            meta.selfCheckCount = QString::fromUtf8(file.readAll()).trimmed().toInt();
        }
    }

    return meta;
}

int writeFailureAndExit(const QString &message)
{
    std::fprintf(stderr, "%s\n", message.toUtf8().constData());
    return 1;
}

} // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName(QStringLiteral("NFSScannerCli"));
    QGuiApplication::setApplicationVersion(QStringLiteral(APP_VERSION));

    const NFSScanner::App::CliCommandLineOptions options = NFSScanner::App::CliCommandLine::parse(&app);
    if (options.showHelp) {
        std::printf("%s\n", NFSScanner::App::CliCommandLine::helpText().toUtf8().constData());
        return 0;
    }

    const bool hasAction = options.validateProfiles || options.runBringup || options.runMockE2e
        || options.exportDiagnostics || options.generateValidationReport || options.runFullValidation;
    if (!hasAction) {
        std::printf("%s\n", NFSScanner::App::CliCommandLine::helpText().toUtf8().constData());
        return 1;
    }

    NFSScanner::Validation::MockValidationRunner runner;
    runner.setOutputRoot(options.inputDir.isEmpty() ? options.outputDir : options.inputDir);
    if (!options.profileDir.isEmpty()) {
        runner.setProfileDir(options.profileDir);
    }
    runner.setProfileName(options.profile);
    if (!options.portableZipPath.isEmpty()) {
        runner.setPortableZipPath(options.portableZipPath);
    }

    bool ok = true;

    if (options.runFullValidation) {
        ok = runner.runAllMockValidations();
    } else {
        if (options.validateProfiles) {
            ok = runner.runValidateProfiles() && ok;
        }
        if (options.runBringup) {
            ok = runner.runMockBringup() && ok;
        }
        if (options.runMockE2e) {
            ok = runner.runMockE2E() && ok;
        }
        if (options.exportDiagnostics) {
            ok = runner.runExportDiagnostics() && ok;
        }
    }

    if (options.generateValidationReport || options.runFullValidation) {
        NFSScanner::Validation::ValidationReportMeta meta = readExternalMeta(runner.outputRoot());
        if (!options.portableZipPath.isEmpty()) {
            meta.portableZipPath = options.portableZipPath;
            meta.portablePass = QFile::exists(options.portableZipPath);
        }
        if (!runner.generateFinalReport(meta)) {
            return writeFailureAndExit(QStringLiteral("Failed to write validation report."));
        }
        std::printf("Validation report: %s\n",
                    QDir(runner.outputRoot()).filePath(QStringLiteral("FULL_MOCK_VALIDATION_REPORT.md")).toUtf8().constData());
    }

    if (!ok || runner.report().failCount() > 0) {
        std::fprintf(stderr, "Validation failed: %d FAIL, %d PASS\n",
                     runner.report().failCount(),
                     runner.report().passCount());
        return 1;
    }

    std::printf("Validation passed: %d checks\n", runner.report().passCount());
    return 0;
}
