#include "license/LicenseManager.h"

#include <QDate>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include "license/MachineId.h"

namespace NFSScanner::License {

QString licenseStatusText(LicenseStatus status)
{
    switch (status) {
    case LicenseStatus::Demo:
        return QStringLiteral("Demo 模式");
    case LicenseStatus::Valid:
        return QStringLiteral("已授权");
    case LicenseStatus::Expired:
        return QStringLiteral("已过期");
    case LicenseStatus::Invalid:
        return QStringLiteral("无效授权");
    }
    return QStringLiteral("未知");
}

LicenseManager::LicenseManager(QObject *parent)
    : QObject(parent)
    , machineId_(MachineId::generate())
    , features_{QStringLiteral("scan"), QStringLiteral("analysis"), QStringLiteral("report")}
{
    loadFromDefaultPath();
}

LicenseStatus LicenseManager::status() const
{
    return status_;
}

QString LicenseManager::machineId() const
{
    return machineId_;
}

QString LicenseManager::licenseId() const
{
    return licenseId_;
}

QString LicenseManager::expireDate() const
{
    return expireDate_;
}

QStringList LicenseManager::features() const
{
    return features_;
}

bool LicenseManager::isFeatureEnabled(const QString &feature) const
{
    if (status_ == LicenseStatus::Valid) {
        return features_.contains(feature);
    }
    // Demo mode: allow core workflow, restrict advanced export in UI if needed.
    return feature == QStringLiteral("scan")
        || feature == QStringLiteral("analysis")
        || feature == QStringLiteral("report");
}

QString LicenseManager::lastError() const
{
    return lastError_;
}

bool LicenseManager::loadFromDefaultPath()
{
    const QString path = QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
                             .filePath(QStringLiteral("license/license.json"));
    return loadFromFile(path);
}

bool LicenseManager::loadFromFile(const QString &path)
{
    lastError_.clear();
    if (!QFile::exists(path)) {
        status_ = LicenseStatus::Demo;
        emit licenseChanged();
        return true;
    }
    return verifyDemoLicense(path);
}

bool LicenseManager::verifyDemoLicense(const QString &path)
{
    // TODO(license): replace DemoLicenseVerifier with asymmetric signature validation.
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        lastError_ = QStringLiteral("无法读取 license.json。");
        status_ = LicenseStatus::Invalid;
        emit licenseChanged();
        return false;
    }

    const QJsonObject obj = QJsonDocument::fromJson(file.readAll()).object();
    licenseId_ = obj.value(QStringLiteral("license_id")).toString();
    const QString boundMachine = obj.value(QStringLiteral("machine_id")).toString();
    expireDate_ = obj.value(QStringLiteral("expire_date")).toString();
    const QString signature = obj.value(QStringLiteral("signature")).toString();

    features_.clear();
    const QJsonArray featureArray = obj.value(QStringLiteral("features")).toArray();
    for (const QJsonValue &value : featureArray) {
        features_ << value.toString();
    }
    if (features_.isEmpty()) {
        features_ = {QStringLiteral("scan"), QStringLiteral("analysis"), QStringLiteral("report")};
    }

    if (boundMachine != machineId_) {
        lastError_ = QStringLiteral("machine_id 不匹配。");
        status_ = LicenseStatus::Invalid;
        emit licenseChanged();
        return false;
    }

    if (!expireDate_.isEmpty()) {
        const QDate expire = QDate::fromString(expireDate_, Qt::ISODate);
        if (expire.isValid() && expire < QDate::currentDate()) {
            status_ = LicenseStatus::Expired;
            emit licenseChanged();
            return false;
        }
    }

    if (signature.isEmpty()) {
        lastError_ = QStringLiteral("Demo 校验：signature 为空，按 Demo 规则接受。");
    }

    status_ = LicenseStatus::Valid;
    emit licenseChanged();
    return true;
}

} // namespace NFSScanner::License
