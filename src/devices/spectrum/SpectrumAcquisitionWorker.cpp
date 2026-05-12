#include "devices/spectrum/SpectrumAcquisitionWorker.h"

#include <QDateTime>

#include <algorithm>

namespace NFSScanner::Devices::Spectrum {

SpectrumAcquisitionWorker::SpectrumAcquisitionWorker(QObject *parent)
    : QObject(parent)
{
}

void SpectrumAcquisitionWorker::setAnalyzer(ISpectrumAnalyzer *analyzer)
{
    analyzer_ = analyzer;
}

void SpectrumAcquisitionWorker::setFallbackAnalyzer(ISpectrumAnalyzer *fallbackAnalyzer)
{
    fallbackAnalyzer_ = fallbackAnalyzer;
}

void SpectrumAcquisitionWorker::setStopOnError(bool stopOnError)
{
    stopOnError_ = stopOnError;
}

void SpectrumAcquisitionWorker::setRetryCount(int retryCount)
{
    retryCount_ = std::clamp(retryCount, 0, 10);
}

void SpectrumAcquisitionWorker::acquire(const SpectrumAcquisitionRequest &request)
{
    emit acquisitionStarted(request.pointIndex);

    SpectrumAcquisitionResult result;
    result.pointIndex = request.pointIndex;
    result.x = request.x;
    result.y = request.y;
    result.z = request.z;
    result.timestamp = QDateTime::currentDateTime();

    ISpectrumAnalyzer *analyzer = activeAnalyzer();
    if (!analyzer) {
        result.error = QStringLiteral("没有可用频谱仪，且 Mock fallback 不可用。");
        emit errorOccurred(result.error);
        emit acquisitionFinished(result);
        return;
    }

    const int attempts = std::max(0, request.retryCount >= 0 ? request.retryCount : retryCount_) + 1;
    for (int attempt = 1; attempt <= attempts; ++attempt) {
        if (attempt > 1) {
            emit logMessage(QStringLiteral("频谱采集重试：点 %1，第 %2/%3 次。")
                                .arg(request.pointIndex)
                                .arg(attempt - 1)
                                .arg(attempts - 1));
        }

        const SpectrumTrace trace = analyzer->singleSweep(request.pointIndex, request.x, request.y, request.z);
        if (!trace.freqs.isEmpty() && !trace.values.isEmpty() && trace.freqs.size() == trace.values.size()) {
            result.ok = true;
            result.trace = trace;
            result.timestamp = trace.timestamp.isValid() ? trace.timestamp : QDateTime::currentDateTime();
            emit acquisitionFinished(result);
            return;
        }

        result.error = analyzer->lastError().isEmpty()
            ? QStringLiteral("频谱采集返回空数据或频率/数据点数量不一致。")
            : analyzer->lastError();
    }

    Q_UNUSED(stopOnError_)
    emit errorOccurred(result.error);
    emit acquisitionFinished(result);
}

ISpectrumAnalyzer *SpectrumAcquisitionWorker::activeAnalyzer() const
{
    if (analyzer_ && analyzer_->isConnected()) {
        return analyzer_;
    }
    if (fallbackAnalyzer_ && fallbackAnalyzer_->isConnected()) {
        return fallbackAnalyzer_;
    }
    return nullptr;
}

} // namespace NFSScanner::Devices::Spectrum
