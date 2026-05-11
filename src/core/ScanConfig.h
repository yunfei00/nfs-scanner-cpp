#pragma once

#include <algorithm>

namespace NFSScanner::Core {

struct ScanConfig
{
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
