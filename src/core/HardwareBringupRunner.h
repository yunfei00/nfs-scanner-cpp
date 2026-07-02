#pragma once

#include "core/HardwareBringupPlan.h"
#include "core/ScanConfig.h"

#include <QString>
#include <QVector>

namespace NFSScanner::Core {

class DeviceManager;

struct HardwareBringupRunResult
{
    QString profileName;
    QVector<BringupStepResult> steps;
    QString reportMarkdown;
    QString exportPath;

    bool overallOk() const;
    int passCount() const;
    int failCount() const;
};

class HardwareBringupRunner
{
public:
    static HardwareBringupRunResult runPlan(DeviceManager *deviceManager,
                                              const HardwareBringupPlan &plan,
                                              const ScanConfig *scanConfig,
                                              bool projectExists,
                                              bool licenseValid);
    static BringupStepResult runStep(DeviceManager *deviceManager,
                                     const BringupStepDefinition &step,
                                     const ScanConfig *scanConfig,
                                     bool projectExists,
                                     bool licenseValid);
    static bool exportReport(const HardwareBringupRunResult &result, QString *exportPath);
};

} // namespace NFSScanner::Core
