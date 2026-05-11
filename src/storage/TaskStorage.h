#pragma once

#include "core/ScanConfig.h"
#include "core/ScanPoint.h"
#include "core/ScanResult.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace NFSScanner::Storage {

class TaskStorage
{
public:
    TaskStorage() = default;

    bool beginTask(const Core::ScanConfig &config, int pointCount);
    bool appendPoint(const Core::ScanPoint &point, const QDateTime &timestamp);
    bool appendTrace(const Core::ScanResult &result);
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
    int traceFrequencyCount_ = 0;
    bool traceFrequencyWritten_ = false;
};

} // namespace NFSScanner::Storage
