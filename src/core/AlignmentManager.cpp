#include "core/AlignmentManager.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPolygonF>

namespace NFSScanner::Core {

namespace {

QJsonArray cornersToJson(const QVector<QPointF> &corners)
{
    QJsonArray array;
    for (const QPointF &point : corners) {
        QJsonObject obj;
        obj.insert(QStringLiteral("x"), point.x());
        obj.insert(QStringLiteral("y"), point.y());
        array.append(obj);
    }
    return array;
}

bool cornersFromJson(const QJsonArray &array, QVector<QPointF> *corners)
{
    if (!corners || array.size() != 4) {
        return false;
    }
    corners->clear();
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        corners->push_back(QPointF(obj.value(QStringLiteral("x")).toDouble(),
                                   obj.value(QStringLiteral("y")).toDouble()));
    }
    return corners->size() == 4;
}

} // namespace

void AlignmentManager::setConfig(const AlignmentConfig &config)
{
    config_ = config;
    if (config_.mappingMode == AlignmentMappingMode::PerspectiveFourPoint
        && (config_.pixelCorners.size() != 4 || config_.worldCorners.size() != 4)) {
        config_.syncCornersFromRectangle();
    }
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
    const QString mode = obj.value(QStringLiteral("mapping_mode")).toString(QStringLiteral("linear_rectangle"));
    loaded.mappingMode = mode == QStringLiteral("perspective_four_point")
        ? AlignmentMappingMode::PerspectiveFourPoint
        : AlignmentMappingMode::LinearRectangle;
    loaded.worldXMin = obj.value(QStringLiteral("world_x_min")).toDouble(loaded.worldXMin);
    loaded.worldXMax = obj.value(QStringLiteral("world_x_max")).toDouble(loaded.worldXMax);
    loaded.worldYMin = obj.value(QStringLiteral("world_y_min")).toDouble(loaded.worldYMin);
    loaded.worldYMax = obj.value(QStringLiteral("world_y_max")).toDouble(loaded.worldYMax);
    loaded.worldZ = obj.value(QStringLiteral("world_z")).toDouble(loaded.worldZ);
    loaded.pixelXMin = obj.value(QStringLiteral("pixel_x_min")).toDouble(loaded.pixelXMin);
    loaded.pixelXMax = obj.value(QStringLiteral("pixel_x_max")).toDouble(loaded.pixelXMax);
    loaded.pixelYMin = obj.value(QStringLiteral("pixel_y_min")).toDouble(loaded.pixelYMin);
    loaded.pixelYMax = obj.value(QStringLiteral("pixel_y_max")).toDouble(loaded.pixelYMax);
    loaded.fixedAspectRatio = obj.value(QStringLiteral("fixed_aspect_ratio")).toBool(true);
    loaded.enabled = obj.value(QStringLiteral("enabled")).toBool(true);

    if (obj.contains(QStringLiteral("pixel_corners"))) {
        cornersFromJson(obj.value(QStringLiteral("pixel_corners")).toArray(), &loaded.pixelCorners);
    }
    if (obj.contains(QStringLiteral("world_corners"))) {
        cornersFromJson(obj.value(QStringLiteral("world_corners")).toArray(), &loaded.worldCorners);
    }
    if (loaded.mappingMode == AlignmentMappingMode::PerspectiveFourPoint
        && (loaded.pixelCorners.size() != 4 || loaded.worldCorners.size() != 4)) {
        loaded.syncCornersFromRectangle();
    }

    config_ = loaded;
    return true;
}

bool AlignmentManager::saveToFile(const QString &filePath) const
{
    lastError_.clear();
    QJsonObject obj;
    obj.insert(QStringLiteral("background_image"), config_.backgroundImagePath);
    obj.insert(QStringLiteral("mapping_mode"),
               config_.mappingMode == AlignmentMappingMode::PerspectiveFourPoint
                   ? QStringLiteral("perspective_four_point")
                   : QStringLiteral("linear_rectangle"));
    obj.insert(QStringLiteral("world_x_min"), config_.worldXMin);
    obj.insert(QStringLiteral("world_x_max"), config_.worldXMax);
    obj.insert(QStringLiteral("world_y_min"), config_.worldYMin);
    obj.insert(QStringLiteral("world_y_max"), config_.worldYMax);
    obj.insert(QStringLiteral("world_z"), config_.worldZ);
    obj.insert(QStringLiteral("pixel_x_min"), config_.pixelXMin);
    obj.insert(QStringLiteral("pixel_x_max"), config_.pixelXMax);
    obj.insert(QStringLiteral("pixel_y_min"), config_.pixelYMin);
    obj.insert(QStringLiteral("pixel_y_max"), config_.pixelYMax);
    obj.insert(QStringLiteral("fixed_aspect_ratio"), config_.fixedAspectRatio);
    obj.insert(QStringLiteral("enabled"), config_.enabled);
    if (config_.mappingMode == AlignmentMappingMode::PerspectiveFourPoint) {
        obj.insert(QStringLiteral("pixel_corners"), cornersToJson(config_.pixelCorners));
        obj.insert(QStringLiteral("world_corners"), cornersToJson(config_.worldCorners));
    }

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

bool AlignmentManager::buildWorldToPixelTransform(QTransform *transform) const
{
    if (!transform) {
        return false;
    }

    if (!config_.isValid()) {
        return false;
    }

    if (config_.mappingMode == AlignmentMappingMode::LinearRectangle) {
        const double sx = (config_.pixelXMax - config_.pixelXMin) / (config_.worldXMax - config_.worldXMin);
        const double sy = (config_.pixelYMax - config_.pixelYMin) / (config_.worldYMax - config_.worldYMin);
        *transform = QTransform()
                         .translate(config_.pixelXMin, config_.pixelYMin)
                         .scale(sx, sy)
                         .translate(-config_.worldXMin, -config_.worldYMin);
        return true;
    }

    if (config_.pixelCorners.size() != 4 || config_.worldCorners.size() != 4) {
        return false;
    }

    QPolygonF worldQuad;
    QPolygonF pixelQuad;
    for (int i = 0; i < 4; ++i) {
        worldQuad << config_.worldCorners.at(i);
        pixelQuad << config_.pixelCorners.at(i);
    }
    return QTransform::quadToQuad(worldQuad, pixelQuad, *transform);
}

QPointF AlignmentManager::worldToPixel(double worldX, double worldY) const
{
    QTransform transform;
    if (!buildWorldToPixelTransform(&transform)) {
        return QPointF(worldX, worldY);
    }
    return transform.map(QPointF(worldX, worldY));
}

QPointF AlignmentManager::pixelToWorld(double pixelX, double pixelY) const
{
    QTransform transform;
    if (!buildWorldToPixelTransform(&transform)) {
        return QPointF(pixelX, pixelY);
    }

    bool invertible = false;
    const QTransform inverse = transform.inverted(&invertible);
    if (!invertible) {
        return QPointF(pixelX, pixelY);
    }
    return inverse.map(QPointF(pixelX, pixelY));
}

} // namespace NFSScanner::Core
