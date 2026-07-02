#pragma once

#include "core/ScanConfig.h"
#include "core/ScanPoint.h"
#include "core/ScanResult.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace NFSScanner::Storage {

struct PointTimingRecord
{
    QDateTime moveStart;
    QDateTime moveEnd;
    QDateTime acquisitionStart;
    QDateTime acquisitionEnd;
    QString motionStatus;
    QString spectrumStatus;
    QString pointStatus = QStringLiteral("ok");
    int retryCount = 0;
};

class TaskStorage
{
public:
    TaskStorage() = default;

    bool beginTask(const Core::ScanConfig &config, int pointCount);
    bool appendPoint(const Core::ScanPoint &point,
                     const QDateTime &timestamp,
                     const PointTimingRecord &timing = PointTimingRecord{});
    bool appendTrace(const Core::ScanResult &result);
    bool appendFailedPoint(const Core::ScanPoint &point, const QString &reason, const PointTimingRecord &timing = PointTimingRecord{});
    bool finishTask();

    QString taskDir() const;
    QString lastError() const;

private:
    bool writeTextFile(const QString &path, const QString &content);
    bool appendTextLine(const QString &path, const QString &line);
    bool writeMetaJson(const Core::ScanConfig &config, int pointCount);
    bool writeScanConfigJson(const Core::ScanConfig &config);
    bool writeTraceFrequencyRow(const QVector<double> &freqs);
    void setError(const QString &message);

    QString taskDir_;
    QString lastError_;
    QString pointsPath_;
    QString tracesPath_;
    QString failedPointsPath_;
    int traceFrequencyCount_ = 0;
    bool traceFrequencyWritten_ = false;
};

} // namespace NFSScanner::Storage
