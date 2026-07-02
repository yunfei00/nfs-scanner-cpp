#pragma once

#include "validation/ValidationReport.h"
#include "validation/ValidationResult.h"

#include <functional>
#include <QString>

namespace NFSScanner::Validation {

class MockValidationRunner
{
public:
    void setOutputRoot(const QString &path);
    void setProfileDir(const QString &path);
    void setProfileName(const QString &name);
    void setPortableZipPath(const QString &path);

    QString outputRoot() const { return outputRoot_; }
    ValidationReport &report() { return report_; }
    const ValidationReport &report() const { return report_; }

    ValidationResult validateBuildEnvironment();
    ValidationResult validateHardwareProfiles();
    ValidationResult validateMockMotion();
    ValidationResult validateMockSpectrum();
    ValidationResult validateMockCamera();
    ValidationResult validateMockProbe();
    ValidationResult validatePreScanChecklist();
    ValidationResult validateMockBringup();
    ValidationResult validateMockScanE2E();
    ValidationResult validateTraceCsvParsing();
    ValidationResult validateHeatmapGeneration();
    ValidationResult validateAlignmentRectLinear();
    ValidationResult validateAlignmentFourPoint();
    ValidationResult validateReportExport();
    ValidationResult validateDiagnosticsExport();
    ValidationResult validateFaultInjection();
    ValidationResult validateSessionReplay();
    ValidationResult validateCommandLineHelp();
    ValidationResult validatePortablePackage();

    bool runValidateProfiles();
    bool runMockBringup();
    bool runMockE2E();
    bool runExportDiagnostics();
    bool runAllMockValidations();
    bool generateFinalReport(const ValidationReportMeta &meta);

    QString lastError() const { return lastError_; }
    QString mockScanTaskDir() const { return mockScanTaskDir_; }

private:
    ValidationResult runTimed(const QString &name, const std::function<ValidationResult()> &fn);
    void ensureOutputTree();
    bool createMockProject();
    QString resultsJsonPath() const;

    QString outputRoot_;
    QString profileDir_;
    QString profileName_ = QStringLiteral("mock_all");
    QString portableZipPath_;
    QString mockScanTaskDir_;
    QString mockProjectDir_;
    ValidationReport report_;
    QString lastError_;
};

} // namespace NFSScanner::Validation
