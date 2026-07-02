#pragma once

#include "validation/ValidationResult.h"

#include <QVector>

namespace NFSScanner::Validation {

struct ValidationReportMeta
{
    QString gitCommit;
    QString appVersion;
    QString qtVersion;
    bool buildPass = false;
    QString buildMessage;
    bool selfCheckPass = false;
    int selfCheckCount = 0;
    QString selfCheckMessage;
    QString portableZipPath;
    bool portablePass = false;
};

class ValidationReport
{
public:
    void addResult(const ValidationResult &result);
    const QVector<ValidationResult> &results() const { return results_; }

    int passCount() const;
    int failCount() const;
    int warnCount() const;
    int skipCount() const;

    bool overallPass() const;
    QString overallConclusion() const;

    bool saveResultsJson(const QString &path) const;
    bool loadResultsJson(const QString &path);
    bool appendResultsJson(const QString &path, const ValidationResult &result) const;

    QString generateMarkdown(const ValidationReportMeta &meta) const;
    bool writeMarkdown(const QString &path, const ValidationReportMeta &meta) const;

    static QString detectGitCommit();
    static QString detectQtVersion();

private:
    QVector<ValidationResult> results_;
};

} // namespace NFSScanner::Validation
