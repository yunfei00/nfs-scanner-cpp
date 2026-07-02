#include "validation/ValidationResult.h"

namespace NFSScanner::Validation {

QString validationStatusText(ValidationStatus status)
{
    switch (status) {
    case ValidationStatus::Pass:
        return QStringLiteral("PASS");
    case ValidationStatus::Fail:
        return QStringLiteral("FAIL");
    case ValidationStatus::Warn:
        return QStringLiteral("WARN");
    case ValidationStatus::Skip:
        return QStringLiteral("SKIP");
    }
    return QStringLiteral("UNKNOWN");
}

} // namespace NFSScanner::Validation
