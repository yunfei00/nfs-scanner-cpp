#include "devices/spectrum/ScpiCommandProfile.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

namespace NFSScanner::Devices::Spectrum {

namespace {

QString readStringField(const QJsonObject &object, const QString &key, const QString &fallback)
{
    return object.contains(key) && object.value(key).isString() ? object.value(key).toString() : fallback;
}

QString profilesRoot()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QDir dir(QDir(appDir).filePath(QStringLiteral("../config/scpi_profiles")));
    if (dir.exists()) {
        return dir.absolutePath();
    }
    return QDir(QDir(appDir).filePath(QStringLiteral("config/scpi_profiles"))).absolutePath();
}

} // namespace

ScpiCommandProfile ScpiCommandProfile::genericDefaults()
{
    ScpiCommandProfile profile;
    profile.name = QStringLiteral("generic_scpi");
    return profile;
}

ScpiCommandProfile ScpiCommandProfile::zna67Defaults()
{
    ScpiCommandProfile profile = genericDefaults();
    profile.name = QStringLiteral("zna67");
    profile.readTraceCommand = QStringLiteral("CALC:DATA? FDAT");
    profile.readComplexTraceCommand = QStringLiteral("CALC:DATA? SDATA");
    return profile;
}

ScpiCommandProfile ScpiCommandProfile::fswDefaults()
{
    ScpiCommandProfile profile = genericDefaults();
    profile.name = QStringLiteral("fsw");
    profile.readTraceCommand = QStringLiteral("TRAC:DATA? TRACE1");
    return profile;
}

ScpiCommandProfile ScpiCommandProfile::n9020aDefaults()
{
    ScpiCommandProfile profile = genericDefaults();
    profile.name = QStringLiteral("n9020a");
    profile.readTraceCommand = QStringLiteral(":TRAC:DATA? TRACE1");
    profile.setStartFreqCommand = QStringLiteral(":SENS:FREQ:STAR %1");
    profile.setStopFreqCommand = QStringLiteral(":SENS:FREQ:STOP %1");
    return profile;
}

bool ScpiCommandProfile::loadFromFile(const QString &path, ScpiCommandProfile *profile, QString *error)
{
    if (!profile) {
        return false;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("无法读取 SCPI profile: %1").arg(path);
        }
        return false;
    }

    const QJsonDocument document = QJsonDocument::fromJson(file.readAll());
    if (!document.isObject()) {
        if (error) {
            *error = QStringLiteral("SCPI profile JSON 无效: %1").arg(path);
        }
        return false;
    }

    const QJsonObject object = document.object();
    profile->name = readStringField(object, QStringLiteral("name"), profile->name);
    profile->idnQuery = readStringField(object, QStringLiteral("idnQuery"), profile->idnQuery);
    profile->resetCommand = readStringField(object, QStringLiteral("resetCommand"), profile->resetCommand);
    profile->clearStatusCommand = readStringField(object, QStringLiteral("clearStatusCommand"), profile->clearStatusCommand);
    profile->systemErrorQuery = readStringField(object, QStringLiteral("systemErrorQuery"), profile->systemErrorQuery);
    profile->setStartFreqCommand = readStringField(object, QStringLiteral("setStartFreqCommand"), profile->setStartFreqCommand);
    profile->setStopFreqCommand = readStringField(object, QStringLiteral("setStopFreqCommand"), profile->setStopFreqCommand);
    profile->setCenterFreqCommand = readStringField(object, QStringLiteral("setCenterFreqCommand"), profile->setCenterFreqCommand);
    profile->setSpanCommand = readStringField(object, QStringLiteral("setSpanCommand"), profile->setSpanCommand);
    profile->setRbwCommand = readStringField(object, QStringLiteral("setRbwCommand"), profile->setRbwCommand);
    profile->setVbwCommand = readStringField(object, QStringLiteral("setVbwCommand"), profile->setVbwCommand);
    profile->setSweepPointsCommand = readStringField(object, QStringLiteral("setSweepPointsCommand"), profile->setSweepPointsCommand);
    profile->setSweepTimeCommand = readStringField(object, QStringLiteral("setSweepTimeCommand"), profile->setSweepTimeCommand);
    profile->singleSweepCommand = readStringField(object, QStringLiteral("singleSweepCommand"), profile->singleSweepCommand);
    profile->waitOperationCompleteQuery = readStringField(object, QStringLiteral("waitOperationCompleteQuery"), profile->waitOperationCompleteQuery);
    profile->readTraceCommand = readStringField(object, QStringLiteral("readTraceCommand"), profile->readTraceCommand);
    profile->readComplexTraceCommand = readStringField(object, QStringLiteral("readComplexTraceCommand"), profile->readComplexTraceCommand);
    return true;
}

ScpiCommandProfile ScpiCommandProfile::loadProfileByType(const QString &deviceType)
{
    const QString normalized = deviceType.trimmed().toLower();
    QString fileName = QStringLiteral("generic_scpi.json");
    ScpiCommandProfile fallback = genericDefaults();

    if (normalized == QStringLiteral("zna67")) {
        fileName = QStringLiteral("zna67.json");
        fallback = zna67Defaults();
    } else if (normalized == QStringLiteral("fsw")) {
        fileName = QStringLiteral("fsw.json");
        fallback = fswDefaults();
    } else if (normalized == QStringLiteral("n9020a")) {
        fileName = QStringLiteral("n9020a.json");
        fallback = n9020aDefaults();
    }

    const QString path = QDir(profilesRoot()).filePath(fileName);
    ScpiCommandProfile loaded = fallback;
    QString error;
    if (!loadFromFile(path, &loaded, &error)) {
        return fallback;
    }
    return loaded;
}

} // namespace NFSScanner::Devices::Spectrum
