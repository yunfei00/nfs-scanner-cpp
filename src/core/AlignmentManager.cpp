#include "core/AlignmentManager.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointF>

namespace NFSScanner::Core {

void AlignmentManager::setConfig(const AlignmentConfig &config)
{
    config_ = config;
}

AlignmentConfig AlignmentManager::config() const
{
    return config_;
}

bool AlignmentManager::loadFromFile(const QString &filePath)
{
    lastError_.clear();
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        lastError_ = QStringLiteral("无法打开 alignment.json：%1").arg(filePath);
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject()) {
        lastError_ = QStringLiteral("alignment.json 格式无效。");
        return false;
    }

    const QJsonObject obj = doc.object();
    AlignmentConfig loaded;
    loaded.backgroundImagePath = obj.value(QStringLiteral("background_image")).toString();
    loaded.worldXMin = obj.value(QStringLiteral("world_x_min")).toDouble(loaded.worldXMin);
    loaded.worldXMax = obj.value(QStringLiteral("world_x_max")).toDouble(loaded.worldXMax);
    loaded.worldYMin = obj.value(QStringLiteral("world_y_min")).toDouble(loaded.worldYMin);
    loaded.worldYMax = obj.value(QStringLiteral("world_y_max")).toDouble(loaded.worldYMax);
    loaded.pixelXMin = obj.value(QStringLiteral("pixel_x_min")).toDouble(loaded.pixelXMin);
    loaded.pixelXMax = obj.value(QStringLiteral("pixel_x_max")).toDouble(loaded.pixelXMax);
    loaded.pixelYMin = obj.value(QStringLiteral("pixel_y_min")).toDouble(loaded.pixelYMin);
    loaded.pixelYMax = obj.value(QStringLiteral("pixel_y_max")).toDouble(loaded.pixelYMax);
    loaded.fixedAspectRatio = obj.value(QStringLiteral("fixed_aspect_ratio")).toBool(true);
    loaded.enabled = obj.value(QStringLiteral("enabled")).toBool(true);
    config_ = loaded;
    return true;
}

bool AlignmentManager::saveToFile(const QString &filePath) const
{
    lastError_.clear();
    QJsonObject obj;
    obj.insert(QStringLiteral("background_image"), config_.backgroundImagePath);
    obj.insert(QStringLiteral("world_x_min"), config_.worldXMin);
    obj.insert(QStringLiteral("world_x_max"), config_.worldXMax);
    obj.insert(QStringLiteral("world_y_min"), config_.worldYMin);
    obj.insert(QStringLiteral("world_y_max"), config_.worldYMax);
    obj.insert(QStringLiteral("pixel_x_min"), config_.pixelXMin);
    obj.insert(QStringLiteral("pixel_x_max"), config_.pixelXMax);
    obj.insert(QStringLiteral("pixel_y_min"), config_.pixelYMin);
    obj.insert(QStringLiteral("pixel_y_max"), config_.pixelYMax);
    obj.insert(QStringLiteral("fixed_aspect_ratio"), config_.fixedAspectRatio);
    obj.insert(QStringLiteral("enabled"), config_.enabled);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        lastError_ = QStringLiteral("无法写入 alignment.json：%1").arg(filePath);
        return false;
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    return true;
}

QString AlignmentManager::lastError() const
{
    return lastError_;
}

QPointF AlignmentManager::worldToPixel(double worldX, double worldY) const
{
    if (!config_.isValid()) {
        return QPointF(worldX, worldY);
    }

    const double u = (worldX - config_.worldXMin) / (config_.worldXMax - config_.worldXMin);
    const double v = (worldY - config_.worldYMin) / (config_.worldYMax - config_.worldYMin);
    const double px = config_.pixelXMin + u * (config_.pixelXMax - config_.pixelXMin);
    const double py = config_.pixelYMin + v * (config_.pixelYMax - config_.pixelYMin);
    return QPointF(px, py);
}

QPointF AlignmentManager::pixelToWorld(double pixelX, double pixelY) const
{
    if (!config_.isValid()) {
        return QPointF(pixelX, pixelY);
    }

    const double u = (pixelX - config_.pixelXMin) / (config_.pixelXMax - config_.pixelXMin);
    const double v = (pixelY - config_.pixelYMin) / (config_.pixelYMax - config_.pixelYMin);
    const double wx = config_.worldXMin + u * (config_.worldXMax - config_.worldXMin);
    const double wy = config_.worldYMin + v * (config_.worldYMax - config_.worldYMin);
    return QPointF(wx, wy);
}

} // namespace NFSScanner::Core
