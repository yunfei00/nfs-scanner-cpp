#include "config/HardwareConfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QJsonObject>

namespace NFSScanner::Config {

namespace {

bool readDouble(const QJsonObject &object, const QString &key, double *value, QStringList *errors)
{
    if (!object.contains(key)) {
        if (errors) {
            errors->append(QStringLiteral("缺少字段: %1").arg(key));
        }
        return false;
    }
    if (!object.value(key).isDouble()) {
        if (errors) {
            errors->append(QStringLiteral("字段类型错误: %1 (应为 number)").arg(key));
        }
        return false;
    }
    *value = object.value(key).toDouble();
    return true;
}

bool readInt(const QJsonObject &object, const QString &key, int *value, QStringList *errors)
{
    if (!object.contains(key)) {
        if (errors) {
            errors->append(QStringLiteral("缺少字段: %1").arg(key));
        }
        return false;
    }
    if (!object.value(key).isDouble()) {
        if (errors) {
            errors->append(QStringLiteral("字段类型错误: %1 (应为 number)").arg(key));
        }
        return false;
    }
    *value = object.value(key).toInt();
    return true;
}

bool readBool(const QJsonObject &object, const QString &key, bool *value, QStringList *errors)
{
    if (!object.contains(key)) {
        if (errors) {
            errors->append(QStringLiteral("缺少字段: %1").arg(key));
        }
        return false;
    }
    if (!object.value(key).isBool()) {
        if (errors) {
            errors->append(QStringLiteral("字段类型错误: %1 (应为 boolean)").arg(key));
        }
        return false;
    }
    *value = object.value(key).toBool();
    return true;
}

bool readString(const QJsonObject &object, const QString &key, QString *value, QStringList *errors)
{
    if (!object.contains(key)) {
        if (errors) {
            errors->append(QStringLiteral("缺少字段: %1").arg(key));
        }
        return false;
    }
    if (!object.value(key).isString()) {
        if (errors) {
            errors->append(QStringLiteral("字段类型错误: %1 (应为 string)").arg(key));
        }
        return false;
    }
    *value = object.value(key).toString();
    return true;
}

void writeDouble(QJsonObject *object, const QString &key, double value)
{
    object->insert(key, value);
}

void writeInt(QJsonObject *object, const QString &key, int value)
{
    object->insert(key, value);
}

void writeBool(QJsonObject *object, const QString &key, bool value)
{
    object->insert(key, value);
}

void writeString(QJsonObject *object, const QString &key, const QString &value)
{
    object->insert(key, value);
}

} // namespace

HardwareConfig HardwareConfig::defaults()
{
    return HardwareConfig{};
}

bool HardwareConfig::validate(QStringList *errors) const
{
    QStringList localErrors;
    QStringList *target = errors ? errors : &localErrors;

    if (motion.enabled) {
        if (motion.port.trimmed().isEmpty()) {
            target->append(QStringLiteral("motion.port 不能为空"));
        }
        if (motion.baudrate <= 0) {
            target->append(QStringLiteral("motion.baudrate 必须大于 0"));
        }
        if (motion.timeoutMs <= 0) {
            target->append(QStringLiteral("motion.timeout_ms 必须大于 0"));
        }
        if (motion.limits.xMin >= motion.limits.xMax) {
            target->append(QStringLiteral("motion.limits.x_min 必须小于 x_max"));
        }
        if (motion.limits.yMin >= motion.limits.yMax) {
            target->append(QStringLiteral("motion.limits.y_min 必须小于 y_max"));
        }
        if (motion.limits.zMin >= motion.limits.zMax) {
            target->append(QStringLiteral("motion.limits.z_min 必须小于 z_max"));
        }
    }

    if (spectrum.enabled) {
        if (spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) != 0) {
            if (spectrum.address.trimmed().isEmpty()) {
                target->append(QStringLiteral("spectrum.address 不能为空"));
            }
            if (spectrum.port <= 0 || spectrum.port > 65535) {
                target->append(QStringLiteral("spectrum.port 必须在 1-65535"));
            }
        }
        if (spectrum.timeoutMs <= 0) {
            target->append(QStringLiteral("spectrum.timeout_ms 必须大于 0"));
        }
        if (spectrum.startFreqHz >= spectrum.stopFreqHz) {
            target->append(QStringLiteral("spectrum.start_freq_hz 必须小于 stop_freq_hz"));
        }
        if (spectrum.points <= 0) {
            target->append(QStringLiteral("spectrum.points 必须大于 0"));
        }
    }

    if (camera.enabled && camera.saveDir.trimmed().isEmpty()) {
        target->append(QStringLiteral("camera.save_dir 不能为空"));
    }

    if (probe.enabled && !isValidProbeOrientation(probe.orientation)) {
        target->append(QStringLiteral("probe.orientation 必须为 Hx 或 Hy"));
    }

    return target->isEmpty();
}

QString defaultHardwareConfigPath()
{
    const QString appDir = QCoreApplication::applicationDirPath();
    const QDir configDir(QDir(appDir).filePath(QStringLiteral("../config")));
    if (configDir.exists()) {
        return configDir.filePath(QStringLiteral("hardware_config.json"));
    }
    return QDir(appDir).filePath(QStringLiteral("config/hardware_config.json"));
}

QString probeOrientationToString(const QString &orientation)
{
    return orientation.compare(QStringLiteral("Hy"), Qt::CaseInsensitive) == 0
        ? QStringLiteral("Hy")
        : QStringLiteral("Hx");
}

bool isValidProbeOrientation(const QString &orientation)
{
    return orientation.compare(QStringLiteral("Hx"), Qt::CaseInsensitive) == 0
        || orientation.compare(QStringLiteral("Hy"), Qt::CaseInsensitive) == 0;
}

QJsonObject motionLimitsToJson(const MotionLimits &limits)
{
    QJsonObject object;
    writeDouble(&object, QStringLiteral("x_min"), limits.xMin);
    writeDouble(&object, QStringLiteral("x_max"), limits.xMax);
    writeDouble(&object, QStringLiteral("y_min"), limits.yMin);
    writeDouble(&object, QStringLiteral("y_max"), limits.yMax);
    writeDouble(&object, QStringLiteral("z_min"), limits.zMin);
    writeDouble(&object, QStringLiteral("z_max"), limits.zMax);
    return object;
}

MotionLimits motionLimitsFromJson(const QJsonObject &object, QStringList *errors)
{
    MotionLimits limits;
    readDouble(object, QStringLiteral("x_min"), &limits.xMin, errors);
    readDouble(object, QStringLiteral("x_max"), &limits.xMax, errors);
    readDouble(object, QStringLiteral("y_min"), &limits.yMin, errors);
    readDouble(object, QStringLiteral("y_max"), &limits.yMax, errors);
    readDouble(object, QStringLiteral("z_min"), &limits.zMin, errors);
    readDouble(object, QStringLiteral("z_max"), &limits.zMax, errors);
    return limits;
}

QJsonObject hardwareConfigToJson(const HardwareConfig &config)
{
    QJsonObject root;

    QJsonObject motion;
    writeBool(&motion, QStringLiteral("enabled"), config.motion.enabled);
    writeString(&motion, QStringLiteral("type"), config.motion.type);
    writeString(&motion, QStringLiteral("port"), config.motion.port);
    writeInt(&motion, QStringLiteral("baudrate"), config.motion.baudrate);
    writeInt(&motion, QStringLiteral("timeout_ms"), config.motion.timeoutMs);
    writeDouble(&motion, QStringLiteral("feed_default"), config.motion.feedDefault);
    motion.insert(QStringLiteral("limits"), motionLimitsToJson(config.motion.limits));
    writeBool(&motion, QStringLiteral("home_on_connect"), config.motion.homeOnConnect);
    writeInt(&motion, QStringLiteral("poll_interval_ms"), config.motion.pollIntervalMs);
    root.insert(QStringLiteral("motion"), motion);

    QJsonObject spectrum;
    writeBool(&spectrum, QStringLiteral("enabled"), config.spectrum.enabled);
    writeString(&spectrum, QStringLiteral("type"), config.spectrum.type);
    writeString(&spectrum, QStringLiteral("address"), config.spectrum.address);
    writeInt(&spectrum, QStringLiteral("port"), config.spectrum.port);
    writeInt(&spectrum, QStringLiteral("timeout_ms"), config.spectrum.timeoutMs);
    writeInt(&spectrum, QStringLiteral("retry_count"), config.spectrum.retryCount);
    writeDouble(&spectrum, QStringLiteral("start_freq_hz"), config.spectrum.startFreqHz);
    writeDouble(&spectrum, QStringLiteral("stop_freq_hz"), config.spectrum.stopFreqHz);
    writeInt(&spectrum, QStringLiteral("points"), config.spectrum.points);
    writeDouble(&spectrum, QStringLiteral("rbw_hz"), config.spectrum.rbwHz);
    writeDouble(&spectrum, QStringLiteral("vbw_hz"), config.spectrum.vbwHz);
    writeDouble(&spectrum, QStringLiteral("sweep_time_s"), config.spectrum.sweepTimeS);
    writeString(&spectrum, QStringLiteral("trace"), config.spectrum.trace);
    root.insert(QStringLiteral("spectrum"), spectrum);

    QJsonObject camera;
    writeBool(&camera, QStringLiteral("enabled"), config.camera.enabled);
    writeString(&camera, QStringLiteral("type"), config.camera.type);
    writeInt(&camera, QStringLiteral("device_index"), config.camera.deviceIndex);
    writeString(&camera, QStringLiteral("save_dir"), config.camera.saveDir);
    writeInt(&camera, QStringLiteral("timeout_ms"), config.camera.timeoutMs);
    root.insert(QStringLiteral("camera"), camera);

    QJsonObject probe;
    writeBool(&probe, QStringLiteral("enabled"), config.probe.enabled);
    writeString(&probe, QStringLiteral("type"), config.probe.type);
    writeString(&probe, QStringLiteral("orientation"), config.probe.orientation);
    writeInt(&probe, QStringLiteral("switch_delay_ms"), config.probe.switchDelayMs);
    root.insert(QStringLiteral("probe"), probe);

    return root;
}

bool hardwareConfigFromJson(const QJsonObject &root, HardwareConfig *config, QStringList *errors)
{
    if (!config) {
        return false;
    }

    QStringList localErrors;
    QStringList *target = errors ? errors : &localErrors;

    if (!root.contains(QStringLiteral("motion")) || !root.value(QStringLiteral("motion")).isObject()) {
        target->append(QStringLiteral("缺少 motion 对象"));
    } else {
        const QJsonObject motion = root.value(QStringLiteral("motion")).toObject();
        readBool(motion, QStringLiteral("enabled"), &config->motion.enabled, target);
        readString(motion, QStringLiteral("type"), &config->motion.type, target);
        readString(motion, QStringLiteral("port"), &config->motion.port, target);
        readInt(motion, QStringLiteral("baudrate"), &config->motion.baudrate, target);
        readInt(motion, QStringLiteral("timeout_ms"), &config->motion.timeoutMs, target);
        readDouble(motion, QStringLiteral("feed_default"), &config->motion.feedDefault, target);
        if (motion.contains(QStringLiteral("limits")) && motion.value(QStringLiteral("limits")).isObject()) {
            config->motion.limits = motionLimitsFromJson(motion.value(QStringLiteral("limits")).toObject(), target);
        }
        readBool(motion, QStringLiteral("home_on_connect"), &config->motion.homeOnConnect, target);
        readInt(motion, QStringLiteral("poll_interval_ms"), &config->motion.pollIntervalMs, target);
    }

    if (!root.contains(QStringLiteral("spectrum")) || !root.value(QStringLiteral("spectrum")).isObject()) {
        target->append(QStringLiteral("缺少 spectrum 对象"));
    } else {
        const QJsonObject spectrum = root.value(QStringLiteral("spectrum")).toObject();
        readBool(spectrum, QStringLiteral("enabled"), &config->spectrum.enabled, target);
        readString(spectrum, QStringLiteral("type"), &config->spectrum.type, target);
        readString(spectrum, QStringLiteral("address"), &config->spectrum.address, target);
        readInt(spectrum, QStringLiteral("port"), &config->spectrum.port, target);
        readInt(spectrum, QStringLiteral("timeout_ms"), &config->spectrum.timeoutMs, target);
        readInt(spectrum, QStringLiteral("retry_count"), &config->spectrum.retryCount, target);
        readDouble(spectrum, QStringLiteral("start_freq_hz"), &config->spectrum.startFreqHz, target);
        readDouble(spectrum, QStringLiteral("stop_freq_hz"), &config->spectrum.stopFreqHz, target);
        readInt(spectrum, QStringLiteral("points"), &config->spectrum.points, target);
        readDouble(spectrum, QStringLiteral("rbw_hz"), &config->spectrum.rbwHz, target);
        readDouble(spectrum, QStringLiteral("vbw_hz"), &config->spectrum.vbwHz, target);
        readDouble(spectrum, QStringLiteral("sweep_time_s"), &config->spectrum.sweepTimeS, target);
        readString(spectrum, QStringLiteral("trace"), &config->spectrum.trace, target);
    }

    if (!root.contains(QStringLiteral("camera")) || !root.value(QStringLiteral("camera")).isObject()) {
        target->append(QStringLiteral("缺少 camera 对象"));
    } else {
        const QJsonObject camera = root.value(QStringLiteral("camera")).toObject();
        readBool(camera, QStringLiteral("enabled"), &config->camera.enabled, target);
        readString(camera, QStringLiteral("type"), &config->camera.type, target);
        readInt(camera, QStringLiteral("device_index"), &config->camera.deviceIndex, target);
        readString(camera, QStringLiteral("save_dir"), &config->camera.saveDir, target);
        readInt(camera, QStringLiteral("timeout_ms"), &config->camera.timeoutMs, target);
    }

    if (!root.contains(QStringLiteral("probe")) || !root.value(QStringLiteral("probe")).isObject()) {
        target->append(QStringLiteral("缺少 probe 对象"));
    } else {
        const QJsonObject probe = root.value(QStringLiteral("probe")).toObject();
        readBool(probe, QStringLiteral("enabled"), &config->probe.enabled, target);
        readString(probe, QStringLiteral("type"), &config->probe.type, target);
        readString(probe, QStringLiteral("orientation"), &config->probe.orientation, target);
        readInt(probe, QStringLiteral("switch_delay_ms"), &config->probe.switchDelayMs, target);
    }

    return target->isEmpty();
}

} // namespace NFSScanner::Config
