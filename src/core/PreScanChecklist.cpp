#include "core/PreScanChecklist.h"

#include "core/DeviceManager.h"
#include "config/HardwareConfig.h"

namespace NFSScanner::Core {

QString checklistLevelText(ChecklistLevel level)
{
    switch (level) {
    case ChecklistLevel::Info:
        return QStringLiteral("信息");
    case ChecklistLevel::Warning:
        return QStringLiteral("警告");
    case ChecklistLevel::Error:
        return QStringLiteral("错误");
    }
    return QStringLiteral("未知");
}

bool PreScanChecklistResult::hasErrors() const
{
    for (const ChecklistItem &item : items) {
        if (item.level == ChecklistLevel::Error) {
            return true;
        }
    }
    return false;
}

bool PreScanChecklistResult::hasWarnings() const
{
    for (const ChecklistItem &item : items) {
        if (item.level == ChecklistLevel::Warning) {
            return true;
        }
    }
    return false;
}

bool PreScanChecklistResult::canProceed(bool allowWarnings) const
{
    if (hasErrors()) {
        return false;
    }
    if (!allowWarnings && hasWarnings()) {
        return false;
    }
    return true;
}

QString PreScanChecklistResult::summaryText() const
{
    QStringList lines;
    for (const ChecklistItem &item : items) {
        lines.append(QStringLiteral("[%1] %2").arg(checklistLevelText(item.level), item.message));
    }
    return lines.join(QStringLiteral("\n"));
}

namespace {

void addItem(QVector<ChecklistItem> *items, const QString &id, ChecklistLevel level, const QString &message)
{
    items->append(ChecklistItem{id, message, level});
}

bool pointWithinLimits(double value, double minValue, double maxValue)
{
    return value >= minValue && value <= maxValue;
}

} // namespace

PreScanChecklistResult PreScanChecklist::evaluate(const PreScanChecklistContext &context)
{
    PreScanChecklistResult result;

    if (!context.projectExists) {
        addItem(&result.items, QStringLiteral("project"), ChecklistLevel::Warning,
                QStringLiteral("当前未打开项目，扫描结果将写入临时目录。"));
    }

    if (context.pointCount <= 0) {
        addItem(&result.items, QStringLiteral("path"), ChecklistLevel::Error,
                context.plannerError.isEmpty()
                    ? QStringLiteral("扫描路径无效，请检查起止坐标与步进。")
                    : context.plannerError);
    } else {
        addItem(&result.items, QStringLiteral("path"), ChecklistLevel::Info,
                QStringLiteral("扫描路径有效，共 %1 个点。").arg(context.pointCount));
    }

    const Config::HardwareConfig hw = context.deviceManager
        ? context.deviceManager->hardwareConfig()
        : Config::HardwareConfig::defaults();

    const auto &limits = hw.motion.limits;
    const bool startOk = pointWithinLimits(context.scanConfig.startX, limits.xMin, limits.xMax)
        && pointWithinLimits(context.scanConfig.startY, limits.yMin, limits.yMax)
        && pointWithinLimits(context.scanConfig.startZ, limits.zMin, limits.zMax);
    const bool endOk = pointWithinLimits(context.scanConfig.endX, limits.xMin, limits.xMax)
        && pointWithinLimits(context.scanConfig.endY, limits.yMin, limits.yMax)
        && pointWithinLimits(context.scanConfig.endZ, limits.zMin, limits.zMax);

    if (!startOk || !endOk) {
        addItem(&result.items, QStringLiteral("limits"), ChecklistLevel::Error,
                QStringLiteral("扫描范围超出运动限位 (X[%1,%2] Y[%3,%4] Z[%5,%6])。")
                    .arg(limits.xMin)
                    .arg(limits.xMax)
                    .arg(limits.yMin)
                    .arg(limits.yMax)
                    .arg(limits.zMin)
                    .arg(limits.zMax));
    } else {
        addItem(&result.items, QStringLiteral("limits"), ChecklistLevel::Info,
                QStringLiteral("扫描坐标在运动限位范围内。"));
    }

    if (context.deviceManager) {
        const bool motionMock = context.mockMode || context.deviceManager->motionMockMode();
        if (hw.motion.enabled && !motionMock && !context.deviceManager->isMotionConnected()) {
            addItem(&result.items, QStringLiteral("motion"), ChecklistLevel::Error,
                    QStringLiteral("运动平台已启用真实模式但未连接。"));
        } else if (motionMock) {
            addItem(&result.items, QStringLiteral("motion"), ChecklistLevel::Info,
                    QStringLiteral("运动平台 Mock 模式可用。"));
        } else {
            addItem(&result.items, QStringLiteral("motion"), ChecklistLevel::Info,
                    QStringLiteral("运动平台已连接。"));
        }

        const bool spectrumMock = context.mockMode
            || hw.spectrum.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
        if (hw.spectrum.enabled && !spectrumMock && !context.deviceManager->isSpectrumConnected()) {
            addItem(&result.items, QStringLiteral("spectrum"), ChecklistLevel::Error,
                    QStringLiteral("频谱仪已启用真实模式但未连接。"));
        } else if (spectrumMock) {
            addItem(&result.items, QStringLiteral("spectrum"), ChecklistLevel::Info,
                    QStringLiteral("频谱仪 Mock 模式可用。"));
        } else {
            addItem(&result.items, QStringLiteral("spectrum"), ChecklistLevel::Info,
                    QStringLiteral("频谱仪已连接。"));
        }

        if (hw.camera.enabled) {
            const bool cameraMock = hw.camera.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
            if (!cameraMock && !context.deviceManager->isCameraConnected()) {
                addItem(&result.items, QStringLiteral("camera"), ChecklistLevel::Error,
                        QStringLiteral("相机已启用但未连接。"));
            } else {
                addItem(&result.items, QStringLiteral("camera"), ChecklistLevel::Info,
                        QStringLiteral("相机检查通过。"));
            }
        }

        if (hw.probe.enabled) {
            const bool probeMock = hw.probe.type.compare(QStringLiteral("mock"), Qt::CaseInsensitive) == 0;
            if (!probeMock && !context.deviceManager->isProbeConnected()) {
                addItem(&result.items, QStringLiteral("probe"), ChecklistLevel::Error,
                        QStringLiteral("探头方向控制器已启用但未连接。"));
            } else {
                addItem(&result.items, QStringLiteral("probe"), ChecklistLevel::Info,
                        QStringLiteral("探头方向 %1 检查通过。")
                            .arg(Config::probeOrientationToString(context.scanConfig.probeOrientation)));
            }
        }
    }

    if (!context.outputDirWritable) {
        addItem(&result.items, QStringLiteral("output"), ChecklistLevel::Error,
                QStringLiteral("扫描输出目录不可写。"));
    } else {
        addItem(&result.items, QStringLiteral("output"), ChecklistLevel::Info,
                QStringLiteral("输出目录可写。"));
    }

    if (!context.hasAlignment) {
        addItem(&result.items, QStringLiteral("alignment"), ChecklistLevel::Warning,
                QStringLiteral("未配置 Alignment，扫描结果无法叠加背景图。"));
    }

    if (!context.licenseValid) {
        addItem(&result.items, QStringLiteral("license"), ChecklistLevel::Error,
                QStringLiteral("授权无效，无法开始扫描。"));
    } else if (context.licenseDemo) {
        addItem(&result.items, QStringLiteral("license"), ChecklistLevel::Warning,
                QStringLiteral("Demo 模式，部分功能受限。"));
    }

    return result;
}

} // namespace NFSScanner::Core
