#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

namespace NFSScanner::Core {

struct ScanResult
{
    int index = 0;
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    QDateTime timestamp;
    QString traceId = QStringLiteral("Trc1_S21");
    QVector<double> freqs;
    QVector<double> values;
};

} // namespace NFSScanner::Core
