#pragma once

#include <QDateTime>
#include <QImage>
#include <QString>

namespace NFSScanner::Report {

struct ReportData
{
    QString reportId;
    QString projectName;
    QString scanTaskDir;
    QDateTime scanTime;
    QString operatorName;
    QString deviceSummary;
    double startFrequencyHz = 0.0;
    double stopFrequencyHz = 0.0;
    QString traceId;
    QString lutName;
    double vmin = 0.0;
    double vmax = 1.0;
    QString notes;
    QImage heatmapImage;
    QImage screenshotImage;
};

} // namespace NFSScanner::Report
