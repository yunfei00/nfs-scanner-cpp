#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

namespace NFSScanner::License {

enum class LicenseStatus {
    Demo,
    Valid,
    Expired,
    Invalid
};

QString licenseStatusText(LicenseStatus status);

class LicenseManager final : public QObject
{
    Q_OBJECT

public:
    explicit LicenseManager(QObject *parent = nullptr);

    LicenseStatus status() const;
    QString machineId() const;
    QString licenseId() const;
    QString expireDate() const;
    QStringList features() const;
    bool isFeatureEnabled(const QString &feature) const;
    QString lastError() const;

    bool loadFromDefaultPath();
    bool loadFromFile(const QString &path);

signals:
    void licenseChanged();

private:
    bool verifyLicenseFile(const QString &path);
    bool verifyDemoLicense(const QString &path);

    LicenseStatus status_ = LicenseStatus::Demo;
    QString machineId_;
    QString licenseId_;
    QString expireDate_;
    QStringList features_;
    QString lastError_;
};

} // namespace NFSScanner::License
