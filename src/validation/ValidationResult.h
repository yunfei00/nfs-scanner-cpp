#pragma once

#include <QString>

namespace NFSScanner::Validation {

enum class ValidationStatus {
    Pass,
    Fail,
    Warn,
    Skip
};

QString validationStatusText(ValidationStatus status);

struct ValidationResult
{
    QString name;
    ValidationStatus status = ValidationStatus::Fail;
    qint64 durationMs = 0;
    QString message;
    QString evidencePath;
    QString errorDetail;

    bool isPass() const { return status == ValidationStatus::Pass; }
    bool isFail() const { return status == ValidationStatus::Fail; }
};

} // namespace NFSScanner::Validation
