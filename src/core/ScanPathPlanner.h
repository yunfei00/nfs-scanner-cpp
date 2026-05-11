#pragma once

#include "core/ScanConfig.h"
#include "core/ScanPoint.h"

#include <QString>
#include <QVector>

namespace NFSScanner::Core {

class ScanPathPlanner
{
public:
    QVector<ScanPoint> generate(const ScanConfig &config);
    QString lastError() const;

private:
    QString lastError_;
};

} // namespace NFSScanner::Core
