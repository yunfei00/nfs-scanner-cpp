#include "devices/spectrum/N9020aSpectrumAnalyzer.h"

#include <QDateTime>
#include <QRegularExpression>
#include <QVariantMap>

#include <algorithm>
#include <cmath>

namespace NFSScanner::Devices::Spectrum {

N9020aSpectrumAnalyzer::N9020aSpectrumAnalyzer(QObject *parent)
    : ISpectrumAnalyzer(parent)
    , client_(this)
{
    connect(&client_, &ScpiTcpClient::logMessage, this, &ISpectrumAnalyzer::logMessage);
    connect(&client_, &ScpiTcpClient::errorOccurred, this, &ISpectrumAnalyzer::errorOccurred);
}

QString N9020aSpectrumAnalyzer::name() const
{
    return QStringLiteral("Keysight N9020A");
}

bool N9020aSpectrumAnalyzer::connectDevice(const QVariantMap &options)
{
    const QString host = options.value(QStringLiteral("host")).toString();
    const quint16 port = static_cast<quint16>(options.value(QStringLiteral("port"), 5025).toInt());
    if (!client_.connectToHost(host, port, 3000)) {
        lastError_ = client_.lastError();
        return false;
    }
    lastError_.clear();
    emit connectedChanged(true);
    emit logMessage(QStringLiteral("Keysight N9020A 已连接。"));
    return true;
}

void N9020aSpectrumAnalyzer::disconnectDevice()
{
    client_.disconnectFromHost();
    emit connectedChanged(false);
}

bool N9020aSpectrumAnalyzer::isConnected() const
{
    return client_.isConnected();
}

bool N9020aSpectrumAnalyzer::configure(const SpectrumConfig &config)
{
    if (!isConnected()) {
        setError(QStringLiteral("Keysight N9020A 未连接，无法应用配置。"));
        return false;
    }

    config_ = config;
    config_.sweepPoints = std::max(2, config_.sweepPoints);
    config_.stopFreqHz = std::max(config_.stopFreqHz, config_.startFreqHz + 1.0);
    config_.centerFreqHz = (config_.startFreqHz + config_.stopFreqHz) * 0.5;
    config_.spanHz = config_.stopFreqHz - config_.startFreqHz;

    const QStringList commands{
        QStringLiteral(":FREQ:STAR %1").arg(config_.startFreqHz, 0, 'f', 0),
        QStringLiteral(":FREQ:STOP %1").arg(config_.stopFreqHz, 0, 'f', 0),
        QStringLiteral(":BAND %1").arg(config_.rbwHz, 0, 'f', 0),
        QStringLiteral(":SWE:POIN %1").arg(config_.sweepPoints),
        QStringLiteral(":INIT:CONT OFF"),
    };

    bool ok = true;
    for (const QString &command : commands) {
        ok = client_.writeCommand(command) && ok;
    }
    if (!ok) {
        lastError_ = client_.lastError();
        emit logMessage(QStringLiteral("N9020A 配置命令部分发送失败：%1").arg(lastError_));
        return false;
    }

    lastError_.clear();
    emit logMessage(QStringLiteral("N9020A 配置完成：%1 Hz ~ %2 Hz，RBW=%3 Hz，点数=%4。")
                        .arg(config_.startFreqHz, 0, 'f', 0)
                        .arg(config_.stopFreqHz, 0, 'f', 0)
                        .arg(config_.rbwHz, 0, 'f', 0)
                        .arg(config_.sweepPoints));
    return true;
}

SpectrumTrace N9020aSpectrumAnalyzer::singleSweep(int pointIndex, double x, double y, double z)
{
    Q_UNUSED(pointIndex)
    Q_UNUSED(x)
    Q_UNUSED(y)
    Q_UNUSED(z)

    if (!isConnected()) {
        setError(QStringLiteral("Keysight N9020A 未连接，无法执行单次扫描。"));
        return {};
    }

    if (!client_.writeCommand(QStringLiteral(":INIT:IMM"))) {
        setError(QStringLiteral("N9020A sweep timeout 或启动扫描失败：%1").arg(client_.lastError()));
        return {};
    }
    const QString opc = client_.queryString(QStringLiteral("*OPC?"), 10000);
    if (opc.isEmpty()) {
        setError(QStringLiteral("N9020A sweep timeout：%1").arg(client_.lastError()));
        return {};
    }

    const QVector<double> numbers = parseAsciiNumbers(client_.queryBinaryOrText(QStringLiteral(":TRAC:DATA? TRACE1"), 10000));
    if (numbers.isEmpty()) {
        setError(QStringLiteral("N9020A trace query timeout 或返回为空：%1").arg(client_.lastError()));
        return {};
    }
    return traceFromNumbers(numbers);
}

QString N9020aSpectrumAnalyzer::queryIdn()
{
    if (!isConnected()) {
        setError(QStringLiteral("Keysight N9020A 未连接，无法查询 IDN。"));
        return {};
    }
    const QString idn = client_.queryString(QStringLiteral("*IDN?"), 5000);
    if (idn.isEmpty()) {
        lastError_ = client_.lastError();
    }
    return idn;
}

QString N9020aSpectrumAnalyzer::lastError() const
{
    return lastError_;
}

QVector<double> N9020aSpectrumAnalyzer::parseAsciiNumbers(const QByteArray &payload) const
{
    QString text = QString::fromLatin1(payload).trimmed();
    if (text.startsWith(QLatin1Char('#')) && text.size() > 2) {
        bool ok = false;
        const int lengthDigits = text.mid(1, 1).toInt(&ok);
        if (ok && text.size() > 2 + lengthDigits) {
            text = text.mid(2 + lengthDigits);
        }
    }

    QVector<double> values;
    const QStringList tokens = text.split(QRegularExpression(QStringLiteral("[,\\s;]+")), Qt::SkipEmptyParts);
    values.reserve(tokens.size());
    for (const QString &token : tokens) {
        bool ok = false;
        const double value = token.trimmed().toDouble(&ok);
        if (ok && std::isfinite(value)) {
            values.push_back(value);
        }
    }
    return values;
}

QVector<double> N9020aSpectrumAnalyzer::buildFrequencies(int pointCount) const
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

SpectrumTrace N9020aSpectrumAnalyzer::traceFromNumbers(const QVector<double> &numbers) const
{
    SpectrumTrace trace;
    trace.traceId = config_.traceId.trimmed().isEmpty() ? QStringLiteral("Trc1_S21") : config_.traceId;
    trace.timestamp = QDateTime::currentDateTime();
    trace.source = QStringLiteral("Keysight N9020A");

    const int expected = std::max(2, config_.sweepPoints);
    if (numbers.size() == expected * 2) {
        trace.values.reserve(expected);
        for (int i = 0; i + 1 < numbers.size(); i += 2) {
            trace.values.push_back(std::hypot(numbers.at(i), numbers.at(i + 1)));
        }
    } else {
        if (numbers.size() != expected) {
            emit const_cast<N9020aSpectrumAnalyzer *>(this)->logMessage(
                QStringLiteral("N9020A 返回点数与配置不一致，按实际点数保存：expected=%1 actual=%2")
                    .arg(expected)
                    .arg(numbers.size()));
        }
        trace.values = numbers;
    }

    trace.freqs = buildFrequencies(trace.values.size());
    const_cast<N9020aSpectrumAnalyzer *>(this)->lastError_.clear();
    return trace;
}

void N9020aSpectrumAnalyzer::setError(const QString &message)
{
    lastError_ = message;
    emit errorOccurred(message);
}

} // namespace NFSScanner::Devices::Spectrum
