#include "validation/ValidationReport.h"

#include "app/AppVersion.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

namespace NFSScanner::Validation {

namespace {

QJsonObject resultToJson(const ValidationResult &result)
{
    QJsonObject obj;
    obj.insert(QStringLiteral("name"), result.name);
    obj.insert(QStringLiteral("status"), validationStatusText(result.status));
    obj.insert(QStringLiteral("duration_ms"), result.durationMs);
    obj.insert(QStringLiteral("message"), result.message);
    obj.insert(QStringLiteral("evidence_path"), result.evidencePath);
    obj.insert(QStringLiteral("error_detail"), result.errorDetail);
    return obj;
}

ValidationResult resultFromJson(const QJsonObject &obj)
{
    ValidationResult result;
    result.name = obj.value(QStringLiteral("name")).toString();
    const QString status = obj.value(QStringLiteral("status")).toString();
    if (status == QStringLiteral("PASS")) {
        result.status = ValidationStatus::Pass;
    } else if (status == QStringLiteral("WARN")) {
        result.status = ValidationStatus::Warn;
    } else if (status == QStringLiteral("SKIP")) {
        result.status = ValidationStatus::Skip;
    } else {
        result.status = ValidationStatus::Fail;
    }
    result.durationMs = static_cast<qint64>(obj.value(QStringLiteral("duration_ms")).toDouble());
    result.message = obj.value(QStringLiteral("message")).toString();
    result.evidencePath = obj.value(QStringLiteral("evidence_path")).toString();
    result.errorDetail = obj.value(QStringLiteral("error_detail")).toString();
    return result;
}

} // namespace

void ValidationReport::addResult(const ValidationResult &result)
{
    results_.push_back(result);
}

int ValidationReport::passCount() const
{
    int count = 0;
    for (const ValidationResult &result : results_) {
        if (result.status == ValidationStatus::Pass) {
            ++count;
        }
    }
    return count;
}

int ValidationReport::failCount() const
{
    int count = 0;
    for (const ValidationResult &result : results_) {
        if (result.status == ValidationStatus::Fail) {
            ++count;
        }
    }
    return count;
}

int ValidationReport::warnCount() const
{
    int count = 0;
    for (const ValidationResult &result : results_) {
        if (result.status == ValidationStatus::Warn) {
            ++count;
        }
    }
    return count;
}

int ValidationReport::skipCount() const
{
    int count = 0;
    for (const ValidationResult &result : results_) {
        if (result.status == ValidationStatus::Skip) {
            ++count;
        }
    }
    return count;
}

bool ValidationReport::overallPass() const
{
    return failCount() == 0;
}

QString ValidationReport::overallConclusion() const
{
    if (failCount() > 0) {
        return QStringLiteral("FAIL");
    }
    if (warnCount() > 0 || skipCount() > 0) {
        return QStringLiteral("PASS_WITH_HARDWARE_WARNINGS");
    }
    return QStringLiteral("PASS");
}

bool ValidationReport::saveResultsJson(const QString &path) const
{
    QJsonArray array;
    for (const ValidationResult &result : results_) {
        array.append(resultToJson(result));
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    file.write(QJsonDocument(array).toJson(QJsonDocument::Indented));
    return true;
}

bool ValidationReport::loadResultsJson(const QString &path)
{
    results_.clear();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isArray()) {
        return false;
    }
    for (const QJsonValue &value : document.array()) {
        if (value.isObject()) {
            results_.push_back(resultFromJson(value.toObject()));
        }
    }
    return true;
}

bool ValidationReport::appendResultsJson(const QString &path, const ValidationResult &result) const
{
    ValidationReport merged;
    merged.loadResultsJson(path);
    merged.addResult(result);
    return merged.saveResultsJson(path);
}

QString ValidationReport::detectGitCommit()
{
    QProcess process;
    process.setProgram(QStringLiteral("git"));
    process.setArguments({QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")});
    process.setWorkingDirectory(QDir::currentPath());
    process.start();
    if (!process.waitForFinished(5000) || process.exitCode() != 0) {
        return QStringLiteral("unknown");
    }
    return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString ValidationReport::detectQtVersion()
{
    return QString::fromLatin1(QT_VERSION_STR);
}

QString ValidationReport::generateMarkdown(const ValidationReportMeta &meta) const
{
    QStringList lines;
    lines << QStringLiteral("# NFS Scanner Full Mock Validation Report");
    lines << QString();
    lines << QStringLiteral("| 项 | 值 |");
    lines << QStringLiteral("|---|---|");
    lines << QStringLiteral("| 验收时间 | %1 |").arg(QDateTime::currentDateTime().toString(Qt::ISODate));
    lines << QStringLiteral("| Git commit | %1 |").arg(meta.gitCommit.isEmpty() ? detectGitCommit() : meta.gitCommit);
    lines << QStringLiteral("| 软件版本 | %1 |").arg(meta.appVersion.isEmpty() ? QStringLiteral(APP_VERSION) : meta.appVersion);
    lines << QStringLiteral("| Qt 版本 | %1 |").arg(meta.qtVersion.isEmpty() ? detectQtVersion() : meta.qtVersion);
    lines << QString();

    lines << QStringLiteral("## 外部步骤");
    lines << QStringLiteral("| 步骤 | 状态 | 说明 |");
    lines << QStringLiteral("|---|---|---|");
    lines << QStringLiteral("| Build | %1 | %2 |")
                 .arg(meta.buildPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                 .arg(meta.buildMessage);
    lines << QStringLiteral("| SelfCheck | %1 | %2 |")
                 .arg(meta.selfCheckPass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                 .arg(meta.selfCheckMessage);
    lines << QStringLiteral("| Portable | %1 | %2 |")
                 .arg(meta.portablePass ? QStringLiteral("PASS") : QStringLiteral("FAIL"))
                 .arg(meta.portableZipPath);
    lines << QString();

    lines << QStringLiteral("## 自动验证项汇总");
    lines << QStringLiteral("| 状态 | 数量 |");
    lines << QStringLiteral("|---|---|");
    lines << QStringLiteral("| PASS | %1 |").arg(passCount());
    lines << QStringLiteral("| FAIL | %1 |").arg(failCount());
    lines << QStringLiteral("| WARN | %1 |").arg(warnCount());
    lines << QStringLiteral("| SKIP | %1 |").arg(skipCount());
    lines << QString();

    auto appendSection = [&](const QString &title, ValidationStatus filter) {
        lines << QStringLiteral("## %1").arg(title);
        bool any = false;
        for (const ValidationResult &result : results_) {
            if (result.status != filter) {
                continue;
            }
            any = true;
            lines << QStringLiteral("- **%1** — %2").arg(result.name, result.message);
            if (!result.evidencePath.isEmpty()) {
                lines << QStringLiteral("  - evidence: `%1`").arg(result.evidencePath);
            }
            if (!result.errorDetail.isEmpty()) {
                lines << QStringLiteral("  - detail: %1").arg(result.errorDetail);
            }
        }
        if (!any) {
            lines << QStringLiteral("_无_");
        }
        lines << QString();
    };

    lines << QStringLiteral("## 详细结果");
    lines << QStringLiteral("| 名称 | 状态 | 耗时(ms) | 消息 | 证据 |");
    lines << QStringLiteral("|---|---|---:|---|---|");
    for (const ValidationResult &result : results_) {
        lines << QStringLiteral("| %1 | %2 | %3 | %4 | %5 |")
                     .arg(result.name,
                          validationStatusText(result.status),
                          QString::number(result.durationMs),
                          result.message,
                          result.evidencePath);
    }
    lines << QString();

    appendSection(QStringLiteral("FAIL 列表"), ValidationStatus::Fail);
    appendSection(QStringLiteral("WARN 列表"), ValidationStatus::Warn);
    appendSection(QStringLiteral("SKIP 列表"), ValidationStatus::Skip);

    lines << QStringLiteral("## 真实硬件待人工验证");
    lines << QStringLiteral("- GRBL 串口 Home / 限位 / Y 负方向长时稳定性");
    lines << QStringLiteral("- ZNA67 / FSW / N9020A 真实 trace 格式");
    lines << QStringLiteral("- USB / 工业相机厂商 SDK");
    lines << QStringLiteral("- 探头 Hx/Hy 继电器时序");
    lines << QStringLiteral("- 全真实连续扫描 SNR / 重复性");
    lines << QStringLiteral("- Installer 安装验证");
    lines << QString();

    lines << QStringLiteral("## 最终结论");
    lines << QStringLiteral("**%1**").arg(overallConclusion());
    lines << QString();
    lines << QStringLiteral("> 报告由 NFSScannerCli 自动生成。");

    return lines.join(QStringLiteral("\n"));
}

bool ValidationReport::writeMarkdown(const QString &path, const ValidationReportMeta &meta) const
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream << generateMarkdown(meta);
    return true;
}

} // namespace NFSScanner::Validation
