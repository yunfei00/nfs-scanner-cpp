#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

namespace NFSScanner::Core {

enum class BringupStepStatus {
    Pending,
    Pass,
    Warn,
    Fail,
    Skipped
};

struct BringupStepDefinition
{
    QString id;
    QString title;
    QString category;
    bool optional = false;
    bool requiresHardware = false;
};

struct BringupStepResult
{
    QString id;
    QString title;
    BringupStepStatus status = BringupStepStatus::Pending;
    QString message;
    QStringList manualChecks;
};

struct HardwareBringupPlan
{
    QString profileName;
    QVector<BringupStepDefinition> steps;

    static HardwareBringupPlan planForProfile(const QString &profileName);
    static QStringList availableProfiles();
};

QString bringupStepStatusText(BringupStepStatus status);

} // namespace NFSScanner::Core
