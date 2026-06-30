#pragma once

#include "report/ReportData.h"

#include <QString>

namespace NFSScanner::Report {

class ReportGenerator
{
public:
    bool exportHtml(const ReportData &data, const QString &outputPath) const;
    bool exportMarkdown(const ReportData &data, const QString &outputPath) const;
    bool exportPdf(const ReportData &data, const QString &outputPath) const;
    bool exportPngImages(const ReportData &data, const QString &outputDirectory) const;
    QString lastError() const;

private:
    QString buildSummaryHtml(const ReportData &data) const;
    mutable QString lastError_;
};

} // namespace NFSScanner::Report
