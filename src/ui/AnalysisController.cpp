#include "ui/AnalysisController.h"

#include "analysis/FrequencyCsvParser.h"
#include "analysis/HeatmapGenerator.h"

#include <QTimer>

#include <algorithm>

namespace NFSScanner::UI {

AnalysisController::AnalysisController(QObject *parent)
    : QObject(parent)
{
    debounceTimer_ = new QTimer(this);
    debounceTimer_->setSingleShot(true);
    connect(debounceTimer_, &QTimer::timeout, this, &AnalysisController::onDebounceTimeout);
}

const Analysis::FrequencyData &AnalysisController::frequencyData() const
{
    return frequencyData_;
}

QImage AnalysisController::heatmapImage() const
{
    return heatmapImage_;
}

QImage AnalysisController::colorbarImage() const
{
    return colorbarImage_;
}

double AnalysisController::vmin() const
{
    return vmin_;
}

double AnalysisController::vmax() const
{
    return vmax_;
}

bool AnalysisController::loadTraceCsv(const QString &path, QString *errorMessage)
{
    Analysis::FrequencyCsvParser parser;
    Analysis::FrequencyData loadedData;
    if (!parser.loadFile(path, &loadedData)) {
        const QString message = parser.lastError();
        if (errorMessage) {
            *errorMessage = message;
        }
        emit loadFailed(message);
        return false;
    }

    frequencyData_ = loadedData;
    return true;
}

bool AnalysisController::refreshPreview(const AnalysisRenderParams &params, QString *errorMessage)
{
    if (!frequencyData_.isValid()) {
        const QString message = QStringLiteral("频谱数据无效，请先加载 traces.csv。");
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    if (params.traceId.trimmed().isEmpty()) {
        const QString message = QStringLiteral("未选择 Trace。");
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    if (params.freqIndex < 0 || params.freqIndex >= frequencyData_.frequencyCount()) {
        const QString message = QStringLiteral("频率索引越界。");
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    Analysis::HeatmapGenerator generator;
    Analysis::HeatmapRenderOptions options;
    options.traceId = params.traceId;
    options.freqIndex = params.freqIndex;
    options.mode = params.mode;
    options.lutName = params.lutName;
    options.autoRange = params.autoRange;
    options.vmin = params.vmin;
    options.vmax = params.vmax;
    options.alpha = std::clamp(params.opacityPercent * 255 / 100, 0, 255);
    options.width = params.width;
    options.height = params.height;

    const Analysis::HeatmapRenderResult result = generator.generate(frequencyData_, options);
    if (!result.ok || result.image.isNull()) {
        const QString message = !result.error.isEmpty() ? result.error : generator.lastError();
        if (errorMessage) {
            *errorMessage = message;
        }
        return false;
    }

    heatmapImage_ = result.image;
    colorbarImage_ = result.colorbar;
    vmin_ = result.actualVmin;
    vmax_ = result.actualVmax;
    emit previewUpdated();
    if (options.autoRange) {
        emit actualRangeUpdated(vmin_, vmax_);
    }
    return true;
}

void AnalysisController::scheduleRefresh(const AnalysisRenderParams &params, int debounceMs)
{
    pendingParams_ = params;
    hasPendingParams_ = true;
    debounceTimer_->start(std::max(50, debounceMs));
}

void AnalysisController::onDebounceTimeout()
{
    if (!hasPendingParams_) {
        return;
    }
    hasPendingParams_ = false;
    refreshPreview(pendingParams_, nullptr);
}

} // namespace NFSScanner::UI
