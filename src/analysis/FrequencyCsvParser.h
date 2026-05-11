#pragma once

#include <QString>

namespace NFSScanner::Analysis {

class FrequencyData;

class FrequencyCsvParser
{
public:
    bool loadFile(const QString &filePath, FrequencyData *outData);
    QString lastError() const;

private:
    QString lastError_;
};

} // namespace NFSScanner::Analysis
