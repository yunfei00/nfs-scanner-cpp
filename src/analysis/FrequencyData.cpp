#include "analysis/FrequencyData.h"

#include <algorithm>
#include <cmath>

namespace NFSScanner::Analysis {

namespace {

QString numberKey(double value)
{
    return QString::number(value, 'g', 17);
}

void appendUnique(QVector<double> &values, double value)
{
    if (!values.contains(value)) {
        values.push_back(value);
    }
}

} // namespace

void FrequencyData::clear()
{
    freqs_.clear();
    traceIds_.clear();
    xs_.clear();
    ys_.clear();
    zs_.clear();
    data_.clear();
    pointKeys_.clear();
    lastError_.clear();
    valid_ = false;
}

bool FrequencyData::isValid() const
{
    return valid_ && !freqs_.isEmpty() && !data_.isEmpty();
}

QString FrequencyData::lastError() const
{
    return lastError_;
}

QVector<double> FrequencyData::freqs() const
{
    return freqs_;
}

QStringList FrequencyData::traceIds() const
{
    return traceIds_;
}

QVector<double> FrequencyData::xs() const
{
    return xs_;
}

QVector<double> FrequencyData::ys() const
{
    return ys_;
}

QVector<double> FrequencyData::zs() const
{
    return zs_;
}

int FrequencyData::frequencyCount() const
{
    return freqs_.size();
}

int FrequencyData::pointCount() const
{
    return pointKeys_.size();
}

bool FrequencyData::hasValue(double x, double y, double z, const QString &traceId) const
{
    return data_.contains(makeKey(x, y, z, traceId));
}

std::complex<double> FrequencyData::complexValue(double x,
                                                 double y,
                                                 double z,
                                                 const QString &traceId,
                                                 int freqIndex) const
{
    const QString key = makeKey(x, y, z, traceId);
    const auto it = data_.constFind(key);
    if (it == data_.constEnd()) {
        lastError_ = QStringLiteral("缺少数据点：%1").arg(key);
        return {};
    }

    if (freqIndex < 0 || freqIndex >= it->size()) {
        lastError_ = QStringLiteral("频率索引越界：%1").arg(freqIndex);
        return {};
    }

    return it->at(freqIndex);
}

double FrequencyData::scalarValue(double x,
                                  double y,
                                  double z,
                                  const QString &traceId,
                                  int freqIndex,
                                  const QString &mode) const
{
    const std::complex<double> value = complexValue(x, y, z, traceId, freqIndex);
    const QString normalizedMode = mode.trimmed().toLower();

    if (normalizedMode == QStringLiteral("magnitude")) {
        return std::abs(value);
    }
    if (normalizedMode == QStringLiteral("db")) {
        return 20.0 * std::log10(std::abs(value) + 1e-12);
    }
    if (normalizedMode == QStringLiteral("phase")) {
        return std::atan2(value.imag(), value.real());
    }
    if (normalizedMode == QStringLiteral("real")) {
        return value.real();
    }
    if (normalizedMode == QStringLiteral("imag")) {
        return value.imag();
    }

    lastError_ = QStringLiteral("不支持的显示模式：%1").arg(mode);
    return 0.0;
}

void FrequencyData::setLastError(const QString &error)
{
    lastError_ = error;
}

void FrequencyData::setFrequencies(const QVector<double> &freqs)
{
    freqs_ = freqs;
    valid_ = false;
}

void FrequencyData::addTraceValues(double x,
                                   double y,
                                   double z,
                                   const QString &traceId,
                                   const QVector<std::complex<double>> &values)
{
    if (traceId.trimmed().isEmpty()) {
        lastError_ = QStringLiteral("TraceId 为空，数据未写入。");
        return;
    }

    data_.insert(makeKey(x, y, z, traceId), values);
    pointKeys_.insert(coordinateKey(x, y, z));
    appendUnique(xs_, x);
    appendUnique(ys_, y);
    appendUnique(zs_, z);
    if (!traceIds_.contains(traceId)) {
        traceIds_.push_back(traceId);
    }
}

void FrequencyData::finalize()
{
    auto sortDoubles = [](QVector<double> &values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };

    sortDoubles(xs_);
    sortDoubles(ys_);
    sortDoubles(zs_);
    traceIds_.sort();
    valid_ = !freqs_.isEmpty() && !data_.isEmpty();
    if (!valid_) {
        lastError_ = QStringLiteral("未解析到有效频谱数据。");
    }
}

QString FrequencyData::makeKey(double x, double y, double z, const QString &traceId)
{
    return QStringLiteral("%1|%2|%3|%4")
        .arg(numberKey(x), numberKey(y), numberKey(z), traceId);
}

QString FrequencyData::coordinateKey(double x, double y, double z)
{
    return QStringLiteral("%1|%2|%3").arg(numberKey(x), numberKey(y), numberKey(z));
}

} // namespace NFSScanner::Analysis
