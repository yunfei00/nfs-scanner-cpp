#include "devices/spectrum/GenericScpiSpectrumAnalyzer.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum {

GenericScpiSpectrumAnalyzer::GenericScpiSpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
    , client_(this)
{
    connect(&client_, &ScpiTcpClient::logMessage, this, &ISpectrumAnalyzer::logMessage);
    connect(&client_, &ScpiTcpClient::errorOccurred, this, &ISpectrumAnalyzer::errorOccurred);
}

QString GenericScpiSpectrumAnalyzer::name() const
{
    return QStringLiteral("Generic SCPI");
}

bool GenericScpiSpectrumAnalyzer::connectDevice(const QVariantMap &options)
{
    const QString host = options.value(QStringLiteral("host")).toString();
    const quint16 port = static_cast<quint16>(options.value(QStringLiteral("port"), 5025).toInt());
    if (!client_.connectToHost(host, port, 3000)) {
        lastError_ = client_.lastError();
        return false;
    }

    lastError_.clear();
    emit connectedChanged(true);
    emit logMessage(QStringLiteral("%1 已连接。").arg(name()));
    return true;
}

void GenericScpiSpectrumAnalyzer::disconnectDevice()
{
    client_.disconnectFromHost();
    emit connectedChanged(false);
}

bool GenericScpiSpectrumAnalyzer::isConnected() const
{
    return client_.isConnected();
}

bool GenericScpiSpectrumAnalyzer::configure(const SpectrumConfig &config)
{
    if (!isConnected()) {
        setError(QStringLiteral("Generic SCPI 未连接，无法应用配置。"));
        return false;
    }

    config_ = config;
    config_.sweepPoints = std::max(2, config_.sweepPoints);
    config_.stopFreqHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;

    const QStringList commands{
        QStringLiteral("FREQ:STAR %1").arg(config_.startFreqHz, 0, 'f', 0),
        QStringLiteral("FREQ:STOP %1").arg(config_.stopFreqHz, 0, 'f', 0),
        QStringLiteral("BAND %1").arg(config_.rbwHz, 0, 'f', 0),
        QStringLiteral("SWE:POIN %1").arg(config_.sweepPoints),
    };

    bool allWritten = true;
    for (const QString &command : commands) {
        allWritten = client_.writeCommand(command) && allWritten;
    }

    if (!allWritten) {
        lastError_ = client_.lastError();
        emit logMessage(QStringLiteral("Generic SCPI 配置命令部分发送失败：%1").arg(lastError_));
        return false;
    }

    lastError_.clear();
    emit logMessage(QStringLiteral("Generic SCPI 配置完成：%1 Hz ~ %2 Hz，RBW=%3 Hz，点数=%4。")
                        .arg(config_.startFreqHz, 0, 'f', 0)
                        .arg(config_.stopFreqHz, 0, 'f', 0)
                        .arg(config_.rbwHz, 0, 'f', 0)
                        .arg(config_.sweepPoints));
    return true;
}

SpectrumTrace GenericScpiSpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(pointIndex)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(z)

    if (!isConnected()) {
        setError(QStringLiteral("Generic SCPI 未连接，无法执行单次扫描。"));
        return {};
    }

    client_.writeCommand(QStringLiteral("INIT:IMM"));
    QByteArray payload = client_.queryBinaryOrText(QStringLiteral("TRAC:DATA? TRACE1"), 10000);
    QVector<double> numbers = parseAsciiNumbers(payload);
    if (numbers.isEmpty()) {
        payload = client_.queryBinaryOrText(QStringLiteral("CALC:DATA? SDATA"), 10000);
        numbers = parseAsciiNumbers(payload);
    }
    if (numbers.isEmpty()) {
        setError(QStringLiteral("Generic SCPI trace query timeout 或返回为空。"));
        return {};
    }

    SpectrumTrace trace = traceFromNumbers(numbers);
    if (trace.values.isEmpty()) {
        return {};
    }
    lastError_.clear();
    return trace;
}

QString GenericScpiSpectrumAnalyzer::queryIdn()
{
    if (!isConnected()) {
        setError(QStringLiteral("Generic SCPI 未连接，无法查询 IDN。"));
        return {};
    }

    const QString idn = client_.queryString(QStringLiteral("*IDN?"), 5000);
    if (idn.isEmpty()) {
        lastError_ = client_.lastError();
    }
    return idn;
}

QString GenericScpiSpectrumAnalyzer::lastError() const
{
    return lastError_;
}

QVector<double> GenericScpiSpectrumAnalyzer::parseAsciiNumbers(const QByteArray &payload) const
{
    QString text = QString::fromLatin1(payload).trimmed();
    if (text.startsWith(QLatin1Char('#')) && text.size() > 2) {
        bool ok = false;
        const int lengthDigits = text.mid(1, 1).toInt(&ok);
        if (ok && text.size() > 2 + lengthDigits) {
            text = text.mid(2 + lengthDigits);
        }
    }

    const QStringList tokens = text.split(QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts);
    QVector<double> numbers;
    numbers.reserve(tokens.size());
    for (const QString &token : tokens) {
        bool ok = false;
        const double value = token.trimmed().toDouble(&ok);
        if (ok && std::isfinite(value)) {
            numbers.push_back(value);
        }
    }
    return numbers;
}

QVector<double> GenericScpiSpectrumAnalyzer::buildFrequencies(int pointCount) const
{
    QVector<double> freqs;
    const int safeCount = std::max(2, pointCount);
    freqs.reserve(safeCount);
    for (int i = 0; i < safeCount; ++i) {
        const double t = static_cast<double>(i) / static_cast<double>(safeCount - 1);
        freqs.push_back(config_.startFreqHz + t * (config_.stopFreqHz - config_.startFreqHz));
    }
    return freqs;
}

SpectrumTrace GenericScpiSpectrumAnalyzer::traceFromNumbers(const QVector<double> &numbers) const
{
    SpectrumTrace trace;
    trace.traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    trace.timestamp = QDateTime::currentDateTime();
    trace.source = name();

    const int expected = std::max(2, config_.sweepPoints);
    if (numbers.size() == expected) {
        trace.values = numbers;
    } else if (numbers.size() == expected * 2) {
        trace.values.reserve(expected);
        for (int i = 0; i + 1 < numbers.size(); i += 2) {
            trace.values.push_back(std::hypot(numbers.at(i), numbers.at(i + 1)));
        }
    } else {
        const_cast<GenericScpiSpectrumAnalyzer *>(this)->setError(
            QStringLiteral("Generic SCPI 返回点数不一致：%1，期望 %2 或 %3。")
                .arg(numbers.size())
                .arg(expected)
                .arg(expected * 2));
        return {};
    }

    trace.freqs = buildFrequencies(trace.values.size());
    return trace;
}

void GenericScpiSpectrumAnalyzer::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
