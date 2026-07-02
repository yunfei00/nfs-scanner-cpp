#pragma once

#include "core/ScanConfig.h"
#include "core/ScanHardware.h"

#include <QString>
#include <QVector>

namespace NFSScanner::Core {

class DeviceManager;
class ScanPathPlanner;

enum class ChecklistLevel {
    Info,
    Warning,
    Error
};

struct ChecklistItem
{
    QString id;
    QString message;
    ChecklistLevel level = ChecklistLevel::Info;
};

struct PreScanChecklistResult
{
    QVector<ChecklistItem> items;

    bool hasErrors() const;
    bool hasWarnings() const;
    bool canProceed(bool allowWarnings = true) const;
    QString summaryText() const;
};

struct PreScanChecklistContext
{
    ScanConfig scanConfig;
    const DeviceManager *deviceManager = nullptr;
    bool projectExists = false;
    bool outputDirWritable = false;
    bool mockMode = true;
    bool licenseDemo = true;
    bool licenseValid = true;
    bool hasAlignment = false;
    int pointCount = 0;
    QString plannerError;
    HardwareMode hardwareMode = HardwareMode::MockAll;
    QString hardwareProfileName;
};

class PreScanChecklist
{
public:
    static PreScanChecklistResult evaluate(const PreScanChecklistContext &context);
};

QString checklistLevelText(ChecklistLevel level);

} // namespace NFSScanner::Core
