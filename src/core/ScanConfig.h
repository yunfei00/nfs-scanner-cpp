#pragma once

#include <algorithm>

#include <QString>

namespace NFSScanner::Core {

struct ScanConfig
{
    double startX = 0.0;
    double startY = 0.0;
    double startZ = 1.0;

    double endX = 10.0;
    double endY = 10.0;
    double endZ = 1.0;

    double stepX = 1.0;
    double stepY = 1.0;
    double stepZ = 1.0;

    double feed = 1000.0;
    int dwellMs = 100;

    bool snakeMode = true;

    QString projectName;
    QString testName;
    QString outputDir;

    int columns = 40;
    int rows = 30;
    double stepMm = 1.0;
    double startFrequencyMhz = 1000.0;
    double stopFrequencyMhz = 3000.0;

    int totalPoints() const
    {
        return std::max(0, columns) * std::max(0, rows);
    }
};

} // namespace NFSScanner::Core
